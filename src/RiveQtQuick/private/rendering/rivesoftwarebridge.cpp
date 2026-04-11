#include "rivesoftwarebridge.h"

#include "rivelogging.h"

#if defined(RIVEQT_ENABLE_SOFTWARE) && __has_include("qpainter/qpainterrivefactory.h") && __has_include("qpainter/qpainterriverenderer.h") && __has_include("qpainter/qpainterriveimage.h")

#include <QPainter>
#include <QtQuick/QQuickWindow>

#include "qpainter/qpainterrivefactory.h"
#include "qpainter/qpainterriverenderer.h"
#include "qpainter/qpainterriveimage.h"

#include "rive/artboard.hpp"
#include "rive/renderer.hpp"
#include "rive/scene.hpp"

std::unique_ptr<RiveBackendBridge> createRiveSoftwareBridge()
{
  return std::make_unique<RiveSoftwareBridge>();
}

RiveSoftwareBridge::RiveSoftwareBridge() = default;

RiveSoftwareBridge::~RiveSoftwareBridge()
{
  delete m_factory;
  m_factory = nullptr;
}

QSGRendererInterface::GraphicsApi RiveSoftwareBridge::api() const
{
  return QSGRendererInterface::Software;
}

RiveBackendBridge::TargetKind RiveSoftwareBridge::targetKind() const
{
  return TargetKind::Painter;
}

bool RiveSoftwareBridge::syncPresentation(QQuickWindow* window,
  const QSize& pixelSize)
{
  m_window = window;
  m_outputPixelSize = pixelSize;
  return true;
}

QRhiTexture* RiveSoftwareBridge::outputTexture() const
{
  return nullptr;
}

bool RiveSoftwareBridge::beginFrame(QQuickWindow* window,
  QRhiCommandBuffer* commandBuffer,
  QPainter* painter,
  const QSize& pixelSize,
  bool targetYUp)
{
  Q_UNUSED(commandBuffer);

  m_window = window;
  m_painter = painter;
  m_outputPixelSize = pixelSize;
  m_targetYUp = targetYUp;

  if (!m_painter) {
    qCDebug(lcRiveSoftware) << "Qt did not expose a software QPainter.";
    return false;
  }

  if (!m_factory) {
    m_factory = new QPainterRiveFactory();
  }

  return true;
}

rive::Factory* RiveSoftwareBridge::factory() const
{
  return m_factory;
}

rive::rcp<rive::RenderImage> RiveSoftwareBridge::createRenderImage(
  const QImage& image)
{
  if (!m_factory) {
    m_factory = new QPainterRiveFactory();
  }

  const QImage rgba = image.convertToFormat(
    QImage::Format_RGBA8888_Premultiplied);
  if (rgba.isNull()) {
    qCDebug(lcRiveSoftware) << "failed to convert hosted image to RGBA.";
    return nullptr;
  }

  return rive::make_rcp<QPainterRiveImage>(rgba);
}

bool RiveSoftwareBridge::render(rive::ArtboardInstance* artboard,
  rive::Scene* scene,
  const rive::Mat2D& transform)
{
  if (!m_painter) {
    qCDebug(lcRiveSoftware) << "render called without an active painter.";
    return false;
  }

  QPainterRiveRenderer renderer(m_painter);
  renderer.save();
  renderer.transform(transform);

  if (scene) {
    scene->draw(&renderer);
  } else if (artboard) {
    artboard->draw(&renderer);
  }

  renderer.restore();
  return true;
}

void RiveSoftwareBridge::release()
{
  m_window = nullptr;
  m_painter = nullptr;
  m_outputPixelSize = {};
  m_targetYUp = false;
}

#else

std::unique_ptr<RiveBackendBridge> createRiveSoftwareBridge()
{
  qCDebug(lcRiveSoftware) << "QPainter software backend sources are not available yet.";
  return nullptr;
}

RiveSoftwareBridge::RiveSoftwareBridge() = default;
RiveSoftwareBridge::~RiveSoftwareBridge() = default;

QSGRendererInterface::GraphicsApi RiveSoftwareBridge::api() const
{
  return QSGRendererInterface::Software;
}

RiveBackendBridge::TargetKind RiveSoftwareBridge::targetKind() const
{
  return TargetKind::Painter;
}

bool RiveSoftwareBridge::syncPresentation(QQuickWindow* window,
  const QSize& pixelSize)
{
  Q_UNUSED(window);
  Q_UNUSED(pixelSize);
  return false;
}

QRhiTexture* RiveSoftwareBridge::outputTexture() const
{
  return nullptr;
}

bool RiveSoftwareBridge::beginFrame(QQuickWindow* window,
  QRhiCommandBuffer* commandBuffer,
  QPainter* painter,
  const QSize& pixelSize,
  bool targetYUp)
{
  Q_UNUSED(window);
  Q_UNUSED(commandBuffer);
  Q_UNUSED(painter);
  Q_UNUSED(pixelSize);
  Q_UNUSED(targetYUp);
  return false;
}

rive::Factory* RiveSoftwareBridge::factory() const
{
  return nullptr;
}

rive::rcp<rive::RenderImage> RiveSoftwareBridge::createRenderImage(
  const QImage& image)
{
  Q_UNUSED(image);
  return nullptr;
}

bool RiveSoftwareBridge::render(rive::ArtboardInstance* artboard,
  rive::Scene* scene,
  const rive::Mat2D& transform)
{
  Q_UNUSED(artboard);
  Q_UNUSED(scene);
  Q_UNUSED(transform);
  return false;
}

void RiveSoftwareBridge::release()
{
}

#endif
