#include "rived3d12bridge.h"

#ifdef RIVEQT_ENABLE_D3D12

#include <mutex>
#include <unordered_map>

#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include "rive/artboard.hpp"
#include "rive/renderer/d3d/d3d.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/scene.hpp"
#include "rivelogging.h"

using Microsoft::WRL::ComPtr;

struct RiveD3D12Bridge::SharedContext
{
  ComPtr<ID3D12Device> device;
  std::unique_ptr<rive::gpu::RenderContext> renderContext;
};

namespace
{
std::mutex g_sharedContextMutex;
std::unordered_map<quintptr, std::weak_ptr<void>> g_sharedContexts;
}

std::unique_ptr<RiveBackendBridge> createRiveD3D12Bridge()
{
    return std::make_unique<RiveD3D12Bridge>();
}

QSGRendererInterface::GraphicsApi RiveD3D12Bridge::api() const
{
    return QSGRendererInterface::Direct3D12;
}

RiveBackendBridge::TargetKind RiveD3D12Bridge::targetKind() const
{
    return TargetKind::Texture;
}

bool RiveD3D12Bridge::syncPresentation(QQuickWindow* window,
                                       const QSize& pixelSize)
{
    m_window = window;
    if (!window || !window->rhi())
    {
        qCDebug(lcRiveD3D12) << "Qt did not expose a QRhi for the scenegraph.";
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
        qCDebug(lcRiveD3D12) << "failed to create the output texture.";
        return false;
    }

    m_outputPixelSize = pixelSize;
    return true;
}

QRhiTexture* RiveD3D12Bridge::outputTexture() const
{
    return m_outputTexture;
}

bool RiveD3D12Bridge::beginFrame(QQuickWindow* window,
                                 QRhiCommandBuffer* commandBuffer,
                                 QPainter* painter,
                                 const QSize& pixelSize,
                                 bool targetYUp)
{
    Q_UNUSED(painter);
    Q_UNUSED(pixelSize);
    Q_UNUSED(targetYUp);
    m_window = window;
    if (!m_outputTexture)
    {
        qCDebug(lcRiveD3D12) << "output texture is not initialized.";
        return false;
    }

    if (!ensureContext(window, commandBuffer))
    {
        return false;
    }

    auto nativeTexture = m_outputTexture->nativeTexture();
    auto* rawTexture = reinterpret_cast<ID3D12Resource*>(
        static_cast<quintptr>(nativeTexture.object));
    if (!rawTexture)
    {
        qCDebug(lcRiveD3D12) << "Qt did not expose an output texture.";
        return false;
    }

    if (!m_renderTarget ||
        m_renderTarget->width() != static_cast<uint32_t>(m_outputPixelSize.width()) ||
        m_renderTarget->height() != static_cast<uint32_t>(m_outputPixelSize.height()))
    {
        auto* impl =
            m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
        m_renderTarget = impl->makeRenderTarget(
            static_cast<uint32_t>(m_outputPixelSize.width()),
            static_cast<uint32_t>(m_outputPixelSize.height()));
    }

    auto* impl =
        m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
    auto externalTexture = impl->manager()->makeExternalTexture(
        rawTexture,
        static_cast<D3D12_RESOURCE_STATES>(nativeTexture.layout));
    m_renderTarget->setTargetTexture(externalTexture);

    const auto* commandBufferHandles =
        static_cast<const QRhiD3D12CommandBufferNativeHandles*>(
            commandBuffer->nativeHandles());
    if (!commandBufferHandles || !commandBufferHandles->commandList)
    {
        qCDebug(lcRiveD3D12) << "Qt did not expose a graphics command list.";
        return false;
    }

    m_commandLists.copyComandList = nullptr;
    m_commandLists.directComandList =
        static_cast<ID3D12GraphicsCommandList*>(commandBufferHandles->commandList);
    return true;
}

rive::Factory* RiveD3D12Bridge::factory() const
{
    return m_sharedContext ? m_sharedContext->renderContext.get() : nullptr;
}

rive::rcp<rive::RenderImage> RiveD3D12Bridge::createRenderImage(
    const QImage& image)
{
    if (!m_sharedContext || !m_sharedContext->renderContext)
    {
        qCDebug(lcRiveD3D12) << "render context is not initialized.";
        return nullptr;
    }

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    auto* impl =
        m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextD3D12Impl>();
    auto texture = impl->makeImageTexture(static_cast<uint32_t>(rgba.width()),
                                          static_cast<uint32_t>(rgba.height()),
                                          1,
                                          rgba.constBits());
    return rive::make_rcp<rive::RiveRenderImage>(texture);
}

bool RiveD3D12Bridge::render(rive::ArtboardInstance* artboard,
                             rive::Scene* scene,
                             const rive::Mat2D& transform)
{
    if (!m_sharedContext || !m_sharedContext->renderContext || !m_renderTarget)
    {
        qCDebug(lcRiveD3D12) << "render target is not ready.";
        return false;
    }

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
                                           .externalCommandBuffer = &m_commandLists,
                                           .currentFrameNumber = currentFrameNumber,
                                           .safeFrameNumber = safeFrameNumber});
    m_outputTexture->setNativeLayout(D3D12_RESOURCE_STATE_COMMON);
    return true;
}

void RiveD3D12Bridge::release()
{
    m_renderTarget = nullptr;
    m_sharedContext.reset();
    m_outputTexture = nullptr;
    m_outputPixelSize = {};
    m_commandLists = {};
    m_frameNumber = 0;
}

bool RiveD3D12Bridge::ensureContext(QQuickWindow* window,
                                    QRhiCommandBuffer* commandBuffer)
{
    if (m_sharedContext && m_sharedContext->renderContext)
    {
        return true;
    }

    auto* rhi = window ? window->rhi() : nullptr;
    const auto* rhiNativeHandles =
        rhi ? static_cast<const QRhiD3D12NativeHandles*>(rhi->nativeHandles())
            : nullptr;
    const auto* commandBufferHandles =
        commandBuffer
            ? static_cast<const QRhiD3D12CommandBufferNativeHandles*>(
                  commandBuffer->nativeHandles())
            : nullptr;

    auto* rawDevice =
        rhiNativeHandles ? static_cast<ID3D12Device*>(rhiNativeHandles->dev) : nullptr;
    auto* rawCommandList =
        commandBufferHandles
            ? static_cast<ID3D12GraphicsCommandList*>(commandBufferHandles->commandList)
            : nullptr;
    if (!rawDevice || !rawCommandList)
    {
        qCDebug(lcRiveD3D12) << "Qt did not expose a device/command list for the scenegraph.";
        return false;
    }

    const quintptr contextKey =
        reinterpret_cast<quintptr>(rawDevice) ^
        (reinterpret_cast<quintptr>(rawCommandList) << 1);

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
            sharedContext->device = rawDevice;

            rive::gpu::D3DContextOptions options;
            sharedContext->renderContext =
                rive::gpu::RenderContextD3D12Impl::MakeContext(sharedContext->device,
                                                               rawCommandList,
                                                               options);
            if (!sharedContext->renderContext)
            {
                qCDebug(lcRiveD3D12) << "failed to create the official render context.";
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

#endif
