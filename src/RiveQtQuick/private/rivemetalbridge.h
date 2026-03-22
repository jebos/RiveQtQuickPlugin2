#pragma once

#include "rivebackendbridge.h"

#ifdef RIVEQT_ENABLE_METAL

#include <memory>

namespace rive::gpu {
class RenderContext;
class RenderTargetMetal;
} // namespace rive::gpu

class RiveMetalBridge : public RiveBackendBridge {
  public:
  QSGRendererInterface::GraphicsApi api() const override;
  bool syncPresentation(QQuickWindow* window,
    const QSize& pixelSize) override;
  QRhiTexture* outputTexture() const override;
  bool prepareFrame(QQuickWindow* window,
    QRhiCommandBuffer* commandBuffer) override;
  rive::Factory* factory() const override;
  rive::rcp<rive::RenderImage> createRenderImage(const QImage& image) override;
  bool render(rive::ArtboardInstance* artboard,
    rive::Scene* scene,
    const rive::Mat2D& transform) override;
  void release() override;

  private:
  struct SharedContext;

  bool ensureContext(QQuickWindow* window,
    QRhiCommandBuffer* commandBuffer);

  QQuickWindow* m_window { nullptr };
  QRhiTexture* m_outputTexture { nullptr };
  QSize m_outputPixelSize;
  std::shared_ptr<SharedContext> m_sharedContext;
  rive::rcp<rive::gpu::RenderTargetMetal> m_renderTarget;
  uint64_t m_frameNumber { 0 };
};

#endif
