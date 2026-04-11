#pragma once

#include <memory>

#include <QImage>
#include <QSize>

#include "rivebackendbridge.h"

class QPainter;
class QQuickWindow;

class QPainterRiveFactory;

class RiveSoftwareBridge final : public RiveBackendBridge {
  public:
  RiveSoftwareBridge();
  ~RiveSoftwareBridge() override;

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
  QPainterRiveFactory* m_factory { nullptr };
  QQuickWindow* m_window { nullptr };
  QPainter* m_painter { nullptr };
  QSize m_outputPixelSize;
  bool m_targetYUp { false };
};

std::unique_ptr<RiveBackendBridge> createRiveSoftwareBridge();
