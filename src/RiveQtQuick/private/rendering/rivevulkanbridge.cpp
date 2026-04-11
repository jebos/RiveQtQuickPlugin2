#include "rivevulkanbridge.h"

#ifdef RIVEQT_ENABLE_VULKAN

#include <cstring>
#include <mutex>
#include <unordered_map>

#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include "rive/artboard.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "rive/scene.hpp"
#include "rivelogging.h"

namespace
{
template <typename Handle>
Handle nativeHandleFromObject(quint64 object)
{
    Handle handle{};
    std::memcpy(&handle, &object, sizeof(handle));
    return handle;
}

constexpr VkFormat kRiveOutputFormat = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkImageUsageFlags kRiveTargetUsageFlags =
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
} // namespace

struct RiveVulkanBridge::SharedContext
{
    std::unique_ptr<rive::gpu::RenderContext> renderContext;
};

namespace
{
std::mutex g_sharedContextMutex;
std::unordered_map<quintptr, std::weak_ptr<void>> g_sharedContexts;
}

std::unique_ptr<RiveBackendBridge> createRiveVulkanBridge()
{
    return std::make_unique<RiveVulkanBridge>();
}

QSGRendererInterface::GraphicsApi RiveVulkanBridge::api() const
{
    return QSGRendererInterface::Vulkan;
}

RiveBackendBridge::TargetKind RiveVulkanBridge::targetKind() const
{
    return TargetKind::Texture;
}

bool RiveVulkanBridge::syncPresentation(QQuickWindow* window,
                                        const QSize& pixelSize)
{
    m_window = window;
    if (!window || !window->rhi())
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose a QRhi for the scenegraph.";
        return false;
    }

    if (pixelSize.isEmpty())
    {
        m_outputPixelSize = {};
        return true;
    }

    bool needsCreate = !m_outputTexture || m_outputPixelSize != pixelSize;
    if (!m_outputTexture)
    {
        m_outputTexture = window->rhi()->newTexture(
            QRhiTexture::RGBA8,
            pixelSize,
            1,
            QRhiTexture::RenderTarget |
                QRhiTexture::UsedAsTransferSource |
                QRhiTexture::UsedWithLoadStore);
    }
    else if (m_outputPixelSize != pixelSize)
    {
        m_outputTexture->setPixelSize(pixelSize);
    }

    if (needsCreate && !m_outputTexture->create())
    {
        qCDebug(lcRiveVulkan) << "failed to create the output texture.";
        return false;
    }

    m_outputPixelSize = pixelSize;
    return true;
}

QRhiTexture* RiveVulkanBridge::outputTexture() const
{
    return m_outputTexture;
}

bool RiveVulkanBridge::beginFrame(QQuickWindow* window,
                                  QRhiCommandBuffer* commandBuffer,
                                  QPainter* painter,
                                  const QSize& pixelSize,
                                  bool targetYUp)
{
    m_window = window;
    Q_UNUSED(painter);
    Q_UNUSED(pixelSize);
    Q_UNUSED(targetYUp);
    Q_UNUSED(commandBuffer);
    if (!m_outputTexture)
    {
        qCDebug(lcRiveVulkan) << "output texture is not initialized.";
        return false;
    }

    if (!ensureContext(window) || !ensureOutputImageView())
    {
        return false;
    }

    auto nativeTexture = m_outputTexture->nativeTexture();
    m_outputImage = nativeHandleFromObject<VkImage>(nativeTexture.object);
    if (m_outputImage == VK_NULL_HANDLE || m_outputImageView == VK_NULL_HANDLE)
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose an output image.";
        return false;
    }

    if (!m_renderTarget ||
        m_renderTarget->width() != static_cast<uint32_t>(m_outputPixelSize.width()) ||
        m_renderTarget->height() != static_cast<uint32_t>(m_outputPixelSize.height()))
    {
        auto* impl =
            m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
        m_renderTarget = impl->makeRenderTarget(
            static_cast<uint32_t>(m_outputPixelSize.width()),
            static_cast<uint32_t>(m_outputPixelSize.height()),
            kRiveOutputFormat,
            kRiveTargetUsageFlags);
    }

    rive::gpu::vkutil::ImageAccess lastAccess = {
        .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_SHADER_READ_BIT,
        .layout = static_cast<VkImageLayout>(nativeTexture.layout),
    };
    m_renderTarget->setTargetImageView(m_outputImageView,
                                       m_outputImage,
                                       lastAccess);
    return true;
}

rive::Factory* RiveVulkanBridge::factory() const
{
    return m_sharedContext ? m_sharedContext->renderContext.get() : nullptr;
}

rive::rcp<rive::RenderImage> RiveVulkanBridge::createRenderImage(
    const QImage& image)
{
    if (!m_sharedContext || !m_sharedContext->renderContext)
    {
        qCDebug(lcRiveVulkan) << "render context is not initialized.";
        return nullptr;
    }

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    auto* impl =
        m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextVulkanImpl>();
    auto texture = impl->makeImageTexture(static_cast<uint32_t>(rgba.width()),
                                          static_cast<uint32_t>(rgba.height()),
                                          1,
                                          rgba.constBits());
    return rive::make_rcp<rive::RiveRenderImage>(texture);
}

bool RiveVulkanBridge::render(rive::ArtboardInstance* artboard,
                              rive::Scene* scene,
                              const rive::Mat2D& transform)
{
    if (!m_sharedContext || !m_sharedContext->renderContext || !m_renderTarget || m_deviceFunctions == nullptr ||
        m_device == VK_NULL_HANDLE || m_queue == VK_NULL_HANDLE)
    {
        qCDebug(lcRiveVulkan) << "render target is not ready.";
        return false;
    }

    if (m_commandPool == VK_NULL_HANDLE)
    {
        const VkCommandPoolCreateInfo commandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_queueFamilyIndex,
        };
        const VkResult poolResult = m_deviceFunctions->vkCreateCommandPool(
            m_device,
            &commandPoolInfo,
            nullptr,
            &m_commandPool);
        if (poolResult != VK_SUCCESS)
        {
            qCDebug(lcRiveVulkan) << "failed to create the command pool.";
            return false;
        }
    }

    if (m_ownedCommandBuffer == VK_NULL_HANDLE)
    {
        const VkCommandBufferAllocateInfo commandBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        const VkResult allocationResult = m_deviceFunctions->vkAllocateCommandBuffers(
            m_device,
            &commandBufferInfo,
            &m_ownedCommandBuffer);
        if (allocationResult != VK_SUCCESS)
        {
            qCDebug(lcRiveVulkan) << "failed to allocate the command buffer.";
            return false;
        }
    }

    m_deviceFunctions->vkResetCommandPool(m_device, m_commandPool, 0);

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    const VkResult beginResult = m_deviceFunctions->vkBeginCommandBuffer(
        m_ownedCommandBuffer,
        &beginInfo);
    if (beginResult != VK_SUCCESS)
    {
        qCDebug(lcRiveVulkan) << "failed to begin recording the command buffer.";
        return false;
    }

    m_currentCommandBuffer = m_ownedCommandBuffer;

    rive::gpu::RenderContext::FrameDescriptor descriptor = {};
    descriptor.renderTargetWidth = static_cast<uint32_t>(m_outputPixelSize.width());
    descriptor.renderTargetHeight = static_cast<uint32_t>(m_outputPixelSize.height());
    descriptor.loadAction = rive::gpu::LoadAction::clear;
    descriptor.clearColor = 0;

    m_sharedContext->renderContext->beginFrame(descriptor);
    rive::RiveRenderer renderer(m_sharedContext->renderContext.get());
    renderer.save();
    renderer.transform(transform);

    if (scene)
    {
        scene->draw(&renderer);
    }
    else if (artboard)
    {
        artboard->draw(&renderer);
    }

    renderer.restore();

    auto graphicsState = m_window ? m_window->graphicsStateInfo()
                                  : QQuickWindow::GraphicsStateInfo{0, 1};
    uint64_t currentFrameNumber = ++m_frameNumber;
    uint64_t safeFrameNumber =
        currentFrameNumber > static_cast<uint64_t>(graphicsState.framesInFlight)
            ? currentFrameNumber - static_cast<uint64_t>(graphicsState.framesInFlight)
            : 0;
    m_sharedContext->renderContext->flush({.renderTarget = m_renderTarget.get(),
                                           .externalCommandBuffer = m_currentCommandBuffer,
                                           .currentFrameNumber = currentFrameNumber,
                                           .safeFrameNumber = safeFrameNumber});

    const rive::gpu::vkutil::ImageAccess shaderReadAccess = {
        .pipelineStages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .accessMask = VK_ACCESS_SHADER_READ_BIT,
        .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    m_renderTarget->accessTargetImageView(m_currentCommandBuffer, shaderReadAccess);
    const VkResult endResult =
        m_deviceFunctions->vkEndCommandBuffer(m_ownedCommandBuffer);
    if (endResult != VK_SUCCESS)
    {
        qCDebug(lcRiveVulkan) << "failed to end recording the command buffer.";
        return false;
    }

    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_ownedCommandBuffer,
    };
    const VkResult submitResult =
        m_deviceFunctions->vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (submitResult != VK_SUCCESS)
    {
        qCDebug(lcRiveVulkan) << "failed to submit the command buffer.";
        return false;
    }

    const VkResult waitResult = m_deviceFunctions->vkQueueWaitIdle(m_queue);
    if (waitResult != VK_SUCCESS)
    {
        qCDebug(lcRiveVulkan) << "failed to wait for the queue.";
        return false;
    }

    m_outputTexture->setNativeLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return true;
}

void RiveVulkanBridge::release()
{
    m_renderTarget = nullptr;
    m_sharedContext.reset();
    destroyOutputImageView();
    if (m_ownedCommandBuffer != VK_NULL_HANDLE && m_commandPool != VK_NULL_HANDLE &&
        m_deviceFunctions && m_device != VK_NULL_HANDLE)
    {
        m_deviceFunctions->vkFreeCommandBuffers(
            m_device,
            m_commandPool,
            1,
            &m_ownedCommandBuffer);
    }
    m_ownedCommandBuffer = VK_NULL_HANDLE;
    if (m_commandPool != VK_NULL_HANDLE && m_deviceFunctions && m_device != VK_NULL_HANDLE)
    {
        m_deviceFunctions->vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
    m_commandPool = VK_NULL_HANDLE;
    m_outputTexture = nullptr;
    m_outputPixelSize = {};
    m_vulkanInstance = nullptr;
    m_deviceFunctions = nullptr;
    m_instance = VK_NULL_HANDLE;
    m_physicalDevice = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_queue = VK_NULL_HANDLE;
    m_queueFamilyIndex = 0;
    m_outputImage = VK_NULL_HANDLE;
    m_currentCommandBuffer = VK_NULL_HANDLE;
    m_frameNumber = 0;
}

bool RiveVulkanBridge::ensureContext(QQuickWindow* window)
{
    if (m_sharedContext && m_sharedContext->renderContext)
    {
        return true;
    }

    auto* rhi = window ? window->rhi() : nullptr;
    const auto* nativeHandles =
        rhi ? static_cast<const QRhiVulkanNativeHandles*>(rhi->nativeHandles())
            : nullptr;
    if (!nativeHandles || !nativeHandles->inst || !nativeHandles->physDev ||
        !nativeHandles->dev)
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose an instance/device for the scenegraph.";
        return false;
    }

    m_vulkanInstance = nativeHandles->inst;
    m_deviceFunctions = m_vulkanInstance->deviceFunctions(nativeHandles->dev);
    m_instance = nativeHandles->inst->vkInstance();
    m_physicalDevice = nativeHandles->physDev;
    m_device = nativeHandles->dev;
    m_queue = nativeHandles->gfxQueue;
    m_queueFamilyIndex = nativeHandles->gfxQueueFamilyIdx;

    if (m_queue == VK_NULL_HANDLE)
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose a graphics queue.";
        return false;
    }

    auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        m_vulkanInstance->getInstanceProcAddr("vkGetInstanceProcAddr"));
    if (!getInstanceProcAddr)
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose vkGetInstanceProcAddr.";
        return false;
    }

    const quintptr contextKey =
        reinterpret_cast<quintptr>(m_instance) ^
        (reinterpret_cast<quintptr>(m_physicalDevice) << 1) ^
        (reinterpret_cast<quintptr>(m_device) << 2);

    {
        std::lock_guard<std::mutex> lock(g_sharedContextMutex);
        auto it = g_sharedContexts.find(contextKey);
        if (it != g_sharedContexts.end())
        {
            m_sharedContext =
                std::static_pointer_cast<SharedContext>(it->second.lock());
        }

        if (!m_sharedContext)
        {
            auto sharedContext = std::make_shared<SharedContext>();
            auto features = queryFeatures();
            sharedContext->renderContext = rive::gpu::RenderContextVulkanImpl::MakeContext(
                m_instance,
                m_physicalDevice,
                m_device,
                features,
                getInstanceProcAddr);
            if (!sharedContext->renderContext)
            {
                qCDebug(lcRiveVulkan) << "failed to create the official render context.";
                return false;
            }

            g_sharedContexts[contextKey] = sharedContext;
            m_sharedContext = std::move(sharedContext);
        }
    }

    if (!m_sharedContext || !m_sharedContext->renderContext)
    {
        return false;
    }

    return true;
}

bool RiveVulkanBridge::ensureOutputImageView()
{
    if (!m_outputTexture || !m_deviceFunctions)
    {
        return false;
    }

    auto nativeTexture = m_outputTexture->nativeTexture();
    const VkImage outputImage = nativeHandleFromObject<VkImage>(nativeTexture.object);
    if (m_outputImageView != VK_NULL_HANDLE && m_outputImage == outputImage)
    {
        return true;
    }

    destroyOutputImageView();
    m_outputImage = outputImage;

    if (m_outputImage == VK_NULL_HANDLE)
    {
        qCDebug(lcRiveVulkan) << "Qt did not expose an output image.";
        return false;
    }

    VkImageViewCreateInfo imageViewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_outputImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = kRiveOutputFormat,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    const VkResult result = m_deviceFunctions->vkCreateImageView(
        m_device,
        &imageViewInfo,
        nullptr,
        &m_outputImageView);
    if (result != VK_SUCCESS)
    {
        m_outputImageView = VK_NULL_HANDLE;
        qCDebug(lcRiveVulkan) << "failed to create an image view for the output texture.";
        return false;
    }

    return true;
}

rive::gpu::VulkanFeatures RiveVulkanBridge::queryFeatures() const
{
    rive::gpu::VulkanFeatures features;
    if (!m_vulkanInstance || m_physicalDevice == VK_NULL_HANDLE)
    {
        return features;
    }

    auto* functions = m_vulkanInstance->functions();

    VkPhysicalDeviceProperties properties = {};
    functions->vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    features.apiVersion = properties.apiVersion;

    VkPhysicalDeviceFeatures physicalFeatures = {};
    functions->vkGetPhysicalDeviceFeatures(m_physicalDevice, &physicalFeatures);
    features.independentBlend = physicalFeatures.independentBlend;
    features.fillModeNonSolid = physicalFeatures.fillModeNonSolid;
    features.fragmentStoresAndAtomics = physicalFeatures.fragmentStoresAndAtomics;
    features.shaderClipDistance = physicalFeatures.shaderClipDistance;

    uint32_t extensionCount = 0;
    functions->vkEnumerateDeviceExtensionProperties(
        m_physicalDevice,
        nullptr,
        &extensionCount,
        nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    if (extensionCount > 0)
    {
        functions->vkEnumerateDeviceExtensionProperties(
            m_physicalDevice,
            nullptr,
            &extensionCount,
            extensions.data());
    }

    auto hasExtension = [&extensions](const char* name) {
        return std::any_of(extensions.cbegin(),
                           extensions.cend(),
                           [name](const VkExtensionProperties& extension) {
                               return std::strcmp(extension.extensionName, name) == 0;
                           });
    };

    features.rasterizationOrderColorAttachmentAccess =
        hasExtension(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
    features.fragmentShaderPixelInterlock =
        hasExtension(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    features.VK_KHR_portability_subset =
        hasExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
    return features;
}

void RiveVulkanBridge::destroyOutputImageView()
{
    if (m_outputImageView != VK_NULL_HANDLE && m_deviceFunctions && m_device != VK_NULL_HANDLE)
    {
        m_deviceFunctions->vkDestroyImageView(m_device, m_outputImageView, nullptr);
    }
    m_outputImageView = VK_NULL_HANDLE;
}

#endif
