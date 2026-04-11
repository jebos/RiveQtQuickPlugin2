#include "qpainterrivefactory.h"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QLinearGradient>
#include <QRadialGradient>

#include "qpainterriveimage.h"
#include "qpainterrivepaint.h"
#include "qpainterrivepath.h"
#include "qpainterriveshader.h"

#include "rive/shapes/paint/color.hpp"
#include "utils/factory_utils.hpp"

namespace {
QColor toQColor(rive::ColorInt value)
{
  return QColor(rive::colorRed(value),
    rive::colorGreen(value),
    rive::colorBlue(value),
    rive::colorAlpha(value));
}
} // namespace

rive::rcp<rive::RenderBuffer> QPainterRiveFactory::makeRenderBuffer(
  rive::RenderBufferType type,
  rive::RenderBufferFlags flags,
  size_t sizeInBytes)
{
  return rive::make_rcp<rive::DataRenderBuffer>(type, flags, sizeInBytes);
}

rive::rcp<rive::RenderShader> QPainterRiveFactory::makeLinearGradient(float sx,
  float sy,
  float ex,
  float ey,
  const rive::ColorInt colors[],
  const float stops[],
  size_t count)
{
  QLinearGradient gradient(QPointF(sx, sy), QPointF(ex, ey));
  for (size_t index = 0; index < count; ++index) {
    gradient.setColorAt(stops[index], toQColor(colors[index]));
  }
  return rive::make_rcp<QPainterRiveShader>(QBrush(gradient));
}

rive::rcp<rive::RenderShader> QPainterRiveFactory::makeRadialGradient(float cx,
  float cy,
  float radius,
  const rive::ColorInt colors[],
  const float stops[],
  size_t count)
{
  QRadialGradient gradient(QPointF(cx, cy), radius);
  for (size_t index = 0; index < count; ++index) {
    gradient.setColorAt(stops[index], toQColor(colors[index]));
  }
  return rive::make_rcp<QPainterRiveShader>(QBrush(gradient));
}

rive::rcp<rive::RenderPath> QPainterRiveFactory::makeRenderPath(rive::RawPath& rawPath,
  rive::FillRule fillRule)
{
  return rive::make_rcp<QPainterRivePath>(rawPath, fillRule);
}

rive::rcp<rive::RenderPath> QPainterRiveFactory::makeEmptyRenderPath()
{
  return rive::make_rcp<QPainterRivePath>();
}

rive::rcp<rive::RenderPaint> QPainterRiveFactory::makeRenderPaint()
{
  return rive::make_rcp<QPainterRivePaint>();
}

rive::rcp<rive::RenderImage> QPainterRiveFactory::decodeImage(rive::Span<const uint8_t> data)
{
  if (data.empty()) {
    return nullptr;
  }

  QImage image;
  image.loadFromData(data.data(), static_cast<int>(data.size()));
  if (image.isNull()) {
    return nullptr;
  }

  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
  if (rgba.isNull()) {
    return nullptr;
  }

  return rive::make_rcp<QPainterRiveImage>(rgba);
}
