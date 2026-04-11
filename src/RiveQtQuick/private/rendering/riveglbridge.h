#pragma once

#include "rivebackendbridge.h"

#ifdef RIVEQT_ENABLE_OPENGL

#include <memory>

namespace rive::gpu {
class RenderContext;
class TextureRenderTargetGL;
} // namespace rive::gpu

class RiveGLBridge : public RiveBackendBridge {
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

  QQuickWindow* m_window { nullptr };
  QRhiTexture* m_outputTexture { nullptr };
  QSize m_outputPixelSize;
  std::shared_ptr<SharedContext> m_sharedContext;
  std::unique_ptr<rive::gpu::TextureRenderTargetGL> m_renderTarget;
  void* m_contextHandle { nullptr };
  void* m_loaderContext { nullptr };
  uint64_t m_frameNumber { 0 };
};

std::unique_ptr<RiveBackendBridge> createRiveGLBridge();

#endif
