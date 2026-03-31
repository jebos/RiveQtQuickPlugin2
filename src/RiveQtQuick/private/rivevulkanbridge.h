#pragma once

#include "rivebackendbridge.h"

#ifdef RIVEQT_ENABLE_VULKAN

#include <memory>

#include <QtGui/qvulkanfunctions.h>
#include <QtGui/qvulkaninstance.h>

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

namespace rive::gpu {
class RenderContext;
} // namespace rive::gpu

class RiveVulkanBridge : public RiveBackendBridge {
  public:
  QSGRendererInterface::GraphicsApi api() const override;
  TargetKind targetKind() const override;
  bool syncPresentation(QQuickWindow* window,
    const QSize& pixelSize) override;
  QRhiTexture* outputTexture() const override;
  bool beginFrame(QQuickWindow* window,
    QRhiCommandBuffer* commandBuffer,
    QPainter* painter,
    const QSize& pixelSize,
    bool targetYUp) override;
  rive::Factory* factory() const override;
  rive::rcp<rive::RenderImage> createRenderImage(const QImage& image) override;
  bool render(rive::ArtboardInstance* artboard,
    rive::Scene* scene,
    const rive::Mat2D& transform) override;
  void release() override;

  private:
  struct SharedContext;

  bool ensureContext(QQuickWindow* window);
  bool ensureOutputImageView();
  rive::gpu::VulkanFeatures queryFeatures() const;
  void destroyOutputImageView();

  QQuickWindow* m_window { nullptr };
  QRhiTexture* m_outputTexture { nullptr };
  QSize m_outputPixelSize;
  QVulkanInstance* m_vulkanInstance { nullptr };
  QVulkanDeviceFunctions* m_deviceFunctions { nullptr };
  VkInstance m_instance { VK_NULL_HANDLE };
  VkPhysicalDevice m_physicalDevice { VK_NULL_HANDLE };
  VkDevice m_device { VK_NULL_HANDLE };
  VkQueue m_queue { VK_NULL_HANDLE };
  uint32_t m_queueFamilyIndex { 0 };
  VkCommandPool m_commandPool { VK_NULL_HANDLE };
  VkCommandBuffer m_ownedCommandBuffer { VK_NULL_HANDLE };
  VkImage m_outputImage { VK_NULL_HANDLE };
  VkImageView m_outputImageView { VK_NULL_HANDLE };
  VkCommandBuffer m_currentCommandBuffer { VK_NULL_HANDLE };
  std::shared_ptr<SharedContext> m_sharedContext;
  rive::rcp<rive::gpu::RenderTargetVulkanImpl> m_renderTarget;
  uint64_t m_frameNumber { 0 };
};

#endif
