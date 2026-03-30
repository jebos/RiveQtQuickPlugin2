#pragma once

#include <memory>

#include <QImage>
#include <QSGRendererInterface>

#include "rive/math/mat2d.hpp"
#include "rive/refcnt.hpp"

class QQuickWindow;
class QRhiCommandBuffer;
class QRhiTexture;

namespace rive {
class ArtboardInstance;
class Factory;
class RenderImage;
class Scene;
} // namespace rive

class RiveBackendBridge {
  public:
  virtual ~RiveBackendBridge() = default;

  static std::unique_ptr<RiveBackendBridge> create(
    QSGRendererInterface::GraphicsApi api);
  static QString graphicsApiName(QSGRendererInterface::GraphicsApi api);

  virtual QSGRendererInterface::GraphicsApi api() const = 0;
  virtual bool syncPresentation(QQuickWindow* window,
    const QSize& pixelSize)
    = 0;
  virtual QRhiTexture* outputTexture() const = 0;
  virtual bool requiresExternalCommands() const { return false; }
  virtual bool prepareFrame(QQuickWindow* window,
    QRhiCommandBuffer* commandBuffer)
    = 0;
  virtual rive::Factory* factory() const = 0;
  virtual rive::rcp<rive::RenderImage> createRenderImage(
    const QImage& image)
    = 0;
  virtual bool render(rive::ArtboardInstance* artboard,
    rive::Scene* scene,
    const rive::Mat2D& transform)
    = 0;
  virtual void release() = 0;
};
