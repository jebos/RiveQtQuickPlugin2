#include "qpainterriverenderer.h"

#include <algorithm>
#include <cmath>

#include "qpainterriveimage.h"
#include "qpainterrivepaint.h"
#include "qpainterrivepath.h"

#include "../rivelogging.h"

#include "rive/math/vec2d.hpp"
#include "utils/factory_utils.hpp"

namespace {
QPolygonF trianglePolygon(const rive::Vec2D& p0,
  const rive::Vec2D& p1,
  const rive::Vec2D& p2)
{
  QPolygonF polygon;
  polygon << QPointF(p0.x, p0.y) << QPointF(p1.x, p1.y) << QPointF(p2.x, p2.y);
  return polygon;
}

int clampedIndex(int value,
  int limit)
{
  return std::clamp(value, 0, limit - 1);
}
} // namespace

QPainterRiveRenderer::QPainterRiveRenderer(QPainter* painter)
    : m_painter(painter)
{
}

void QPainterRiveRenderer::save()
{
  if (!m_painter) {
    return;
  }

  m_painter->save();
  m_opacityStack.push_back(m_opacityStack.back());
}

void QPainterRiveRenderer::restore()
{
  if (!m_painter) {
    return;
  }

  m_painter->restore();
  if (m_opacityStack.size() > 1) {
    m_opacityStack.pop_back();
  }
}

void QPainterRiveRenderer::transform(const rive::Mat2D& transform)
{
  if (!m_painter) {
    return;
  }

  m_painter->setWorldTransform(toQTransform(transform), true);
}

void QPainterRiveRenderer::drawPath(rive::RenderPath* path,
  rive::RenderPaint* paint)
{
  if (!m_painter || !path || !paint) {
    return;
  }

  auto* qpPath = static_cast<QPainterRivePath*>(path);
  auto* qpPaint = static_cast<QPainterRivePaint*>(paint);

  if (!qpPaint->isStroke() && qpPaint->feather() > 0.0f &&
      drawFeatheredPath(qpPath->path(), *qpPaint)) {
    return;
  }

  m_painter->save();
  m_painter->setOpacity(std::max(0.0f, m_opacityStack.back()));
  m_painter->setCompositionMode(qpPaint->compositionMode());
  m_painter->setRenderHint(QPainter::Antialiasing, true);

  if (qpPaint->isStroke()) {
    QPen pen(qpPaint->brush(), qpPaint->strokeWidth());
    pen.setJoinStyle(qpPaint->joinStyle());
    pen.setCapStyle(qpPaint->capStyle());
    m_painter->setPen(pen);
    m_painter->setBrush(Qt::NoBrush);
  } else {
    m_painter->setPen(Qt::NoPen);
    m_painter->setBrush(qpPaint->brush());
  }

  m_painter->drawPath(qpPath->path());
  m_painter->restore();
}

void QPainterRiveRenderer::clipPath(rive::RenderPath* path)
{
  if (!m_painter || !path) {
    return;
  }

  auto* qpPath = static_cast<QPainterRivePath*>(path);
  setDeviceClipPath(qpPath->path(), Qt::IntersectClip);
}

void QPainterRiveRenderer::drawImage(const rive::RenderImage* image,
  rive::ImageSampler,
  rive::BlendMode blendMode,
  float opacity)
{
  if (!m_painter || !image) {
    return;
  }

  auto* qpImage = static_cast<const QPainterRiveImage*>(image);
  const float finalOpacity = std::max(0.0f, opacity * m_opacityStack.back());

  m_painter->save();
  m_painter->setOpacity(finalOpacity);
  m_painter->setCompositionMode(toCompositionMode(blendMode));
  m_painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  m_painter->drawImage(QPointF(0.0, 0.0), qpImage->image());
  m_painter->restore();
}

void QPainterRiveRenderer::drawImageMesh(const rive::RenderImage* image,
  rive::ImageSampler,
  rive::rcp<rive::RenderBuffer> vertices,
  rive::rcp<rive::RenderBuffer> uvCoords,
  rive::rcp<rive::RenderBuffer> indices,
  uint32_t vertexCount,
  uint32_t indexCount,
  rive::BlendMode blendMode,
  float opacity)
{
  if (!m_painter || !image || !vertices || !uvCoords || !indices) {
    return;
  }

  auto* qpImage = static_cast<const QPainterRiveImage*>(image);
  auto* indexBuffer = static_cast<rive::DataRenderBuffer*>(indices.get());
  auto* vertexBuffer = static_cast<rive::DataRenderBuffer*>(vertices.get());
  auto* uvBuffer = static_cast<rive::DataRenderBuffer*>(uvCoords.get());
  if (!indexBuffer || !vertexBuffer || !uvBuffer) {
    return;
  }

  Q_UNUSED(vertexCount);

  const float imageWidth = static_cast<float>(qpImage->image().width());
  const float imageHeight = static_cast<float>(qpImage->image().height());
  const float finalOpacity = std::max(0.0f, opacity * m_opacityStack.back());

  const uint16_t* currentIndex = indexBuffer->u16s();
  const rive::Vec2D* points = vertexBuffer->vecs();
  const rive::Vec2D* uvs = uvBuffer->vecs();
  const size_t triangleCount = indexCount / 3;

  m_painter->save();
  m_painter->setOpacity(finalOpacity);
  m_painter->setCompositionMode(toCompositionMode(blendMode));
  m_painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

  for (size_t triangleIndex = 0; triangleIndex < triangleCount; ++triangleIndex) {
    const uint16_t index0 = *currentIndex++;
    const uint16_t index1 = *currentIndex++;
    const uint16_t index2 = *currentIndex++;

    const rive::Vec2D p0 = points[index0];
    const rive::Vec2D p1 = points[index1];
    const rive::Vec2D p2 = points[index2];

    const rive::Vec2D uv0(uvs[index0].x * imageWidth, uvs[index0].y * imageHeight);
    const rive::Vec2D uv1(uvs[index1].x * imageWidth, uvs[index1].y * imageHeight);
    const rive::Vec2D uv2(uvs[index2].x * imageWidth, uvs[index2].y * imageHeight);

    const rive::Mat2D matrix =
      basisMatrix(p0, p1, p2) * basisMatrix(uv0, uv1, uv2).invertOrIdentity();

    m_painter->save();
    QPainterPath clip;
    clip.addPolygon(trianglePolygon(p0, p1, p2));
    clip.closeSubpath();
    setDeviceClipPath(clip, Qt::IntersectClip);
    m_painter->setWorldTransform(toQTransform(matrix), true);
    m_painter->drawImage(QPointF(0.0, 0.0), qpImage->image());
    m_painter->restore();
  }

  m_painter->restore();
}

void QPainterRiveRenderer::modulateOpacity(float opacity)
{
  m_opacityStack.back() = std::max(0.0f, m_opacityStack.back() * opacity);
}

bool QPainterRiveRenderer::drawFeatheredPath(const QPainterPath& path,
  const QPainterRivePaint& paint)
{
  if (!m_painter || path.isEmpty()) {
    return false;
  }

  const QTransform currentTransform = m_painter->worldTransform();
  const QPainterPath devicePath = currentTransform.map(path);
  const int blurRadius = blurRadiusForFeather(paint.feather(), currentTransform);
  if (blurRadius <= 0) {
    return false;
  }

  QRectF deviceBounds = devicePath.boundingRect();
  if (!deviceBounds.isValid() || deviceBounds.isEmpty()) {
    return false;
  }

  const int padding = blurRadius * 2;
  deviceBounds = deviceBounds.adjusted(-padding, -padding, padding, padding);
  const QRect imageRect = deviceBounds.toAlignedRect();
  if (imageRect.isEmpty() || imageRect.width() > 4096 || imageRect.height() > 4096) {
    return false;
  }

  QImage mask(imageRect.size(), QImage::Format_ARGB32_Premultiplied);
  mask.fill(Qt::transparent);

  QPainter maskPainter(&mask);
  maskPainter.setRenderHint(QPainter::Antialiasing, true);
  maskPainter.translate(-imageRect.topLeft());
  maskPainter.setPen(Qt::NoPen);

  QBrush brush = paint.brush();
  brush.setTransform(currentTransform * brush.transform());
  maskPainter.setBrush(brush);
  maskPainter.drawPath(devicePath);
  maskPainter.end();

  const QImage blurred = boxBlurImage(mask, blurRadius);

  m_painter->save();
  m_painter->setWorldTransform(QTransform(), false);
  m_painter->setOpacity(std::max(0.0f, m_opacityStack.back()));
  m_painter->setCompositionMode(paint.compositionMode());
  m_painter->drawImage(imageRect.topLeft(), blurred);
  m_painter->restore();
  return true;
}

void QPainterRiveRenderer::setDeviceClipPath(const QPainterPath& path,
  Qt::ClipOperation operation)
{
  if (!m_painter) {
    return;
  }

  const QTransform currentTransform = m_painter->worldTransform();
  m_painter->setWorldTransform(QTransform(), false);
  m_painter->setClipPath(currentTransform.map(path), operation);
  m_painter->setWorldTransform(currentTransform, false);
}

QImage QPainterRiveRenderer::boxBlurImage(const QImage& image,
  int radius)
{
  if (radius <= 0 || image.isNull()) {
    return image;
  }

  const QImage source = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const int width = source.width();
  const int height = source.height();
  const int windowSize = radius * 2 + 1;

  QImage horizontal(source.size(), QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < height; ++y) {
    const QRgb* srcLine = reinterpret_cast<const QRgb*>(source.constScanLine(y));
    QRgb* dstLine = reinterpret_cast<QRgb*>(horizontal.scanLine(y));

    int sumR = 0;
    int sumG = 0;
    int sumB = 0;
    int sumA = 0;
    for (int sample = -radius; sample <= radius; ++sample) {
      const QRgb pixel = srcLine[clampedIndex(sample, width)];
      sumR += qRed(pixel);
      sumG += qGreen(pixel);
      sumB += qBlue(pixel);
      sumA += qAlpha(pixel);
    }

    for (int x = 0; x < width; ++x) {
      dstLine[x] = qRgba(sumR / windowSize,
        sumG / windowSize,
        sumB / windowSize,
        sumA / windowSize);

      const QRgb removed = srcLine[clampedIndex(x - radius, width)];
      const QRgb added = srcLine[clampedIndex(x + radius + 1, width)];
      sumR += qRed(added) - qRed(removed);
      sumG += qGreen(added) - qGreen(removed);
      sumB += qBlue(added) - qBlue(removed);
      sumA += qAlpha(added) - qAlpha(removed);
    }
  }

  QImage vertical(source.size(), QImage::Format_ARGB32_Premultiplied);
  for (int x = 0; x < width; ++x) {
    int sumR = 0;
    int sumG = 0;
    int sumB = 0;
    int sumA = 0;
    for (int sample = -radius; sample <= radius; ++sample) {
      const QRgb pixel = horizontal.pixel(x, clampedIndex(sample, height));
      sumR += qRed(pixel);
      sumG += qGreen(pixel);
      sumB += qBlue(pixel);
      sumA += qAlpha(pixel);
    }

    for (int y = 0; y < height; ++y) {
      vertical.setPixel(x,
        y,
        qRgba(sumR / windowSize,
          sumG / windowSize,
          sumB / windowSize,
          sumA / windowSize));

      const QRgb removed = horizontal.pixel(x, clampedIndex(y - radius, height));
      const QRgb added = horizontal.pixel(x, clampedIndex(y + radius + 1, height));
      sumR += qRed(added) - qRed(removed);
      sumG += qGreen(added) - qGreen(removed);
      sumB += qBlue(added) - qBlue(removed);
      sumA += qAlpha(added) - qAlpha(removed);
    }
  }

  return vertical;
}

int QPainterRiveRenderer::blurRadiusForFeather(float feather,
  const QTransform& transform)
{
  return std::max(1, static_cast<int>(std::ceil(feather * maxScaleForTransform(transform) * 0.375)));
}

qreal QPainterRiveRenderer::maxScaleForTransform(const QTransform& transform)
{
  const qreal scaleX = std::hypot(transform.m11(), transform.m21());
  const qreal scaleY = std::hypot(transform.m12(), transform.m22());
  return std::max(scaleX, scaleY);
}

QTransform QPainterRiveRenderer::toQTransform(const rive::Mat2D& transform)
{
  return QTransform(transform[0],
    transform[1],
    transform[2],
    transform[3],
    transform[4],
    transform[5]);
}

rive::Mat2D QPainterRiveRenderer::basisMatrix(const rive::Vec2D& p0,
  const rive::Vec2D& p1,
  const rive::Vec2D& p2)
{
  const rive::Vec2D e0 = p1 - p0;
  const rive::Vec2D e1 = p2 - p0;
  return rive::Mat2D(e0.x, e0.y, e1.x, e1.y, p0.x, p0.y);
}

QPainter::CompositionMode QPainterRiveRenderer::toCompositionMode(rive::BlendMode value)
{
  switch (value) {
  case rive::BlendMode::screen:
    return QPainter::CompositionMode_Screen;
  case rive::BlendMode::overlay:
    return QPainter::CompositionMode_Overlay;
  case rive::BlendMode::darken:
    return QPainter::CompositionMode_Darken;
  case rive::BlendMode::lighten:
    return QPainter::CompositionMode_Lighten;
  case rive::BlendMode::colorDodge:
    return QPainter::CompositionMode_ColorDodge;
  case rive::BlendMode::colorBurn:
    return QPainter::CompositionMode_ColorBurn;
  case rive::BlendMode::hardLight:
    return QPainter::CompositionMode_HardLight;
  case rive::BlendMode::softLight:
    return QPainter::CompositionMode_SoftLight;
  case rive::BlendMode::difference:
    return QPainter::CompositionMode_Difference;
  case rive::BlendMode::exclusion:
    return QPainter::CompositionMode_Exclusion;
  case rive::BlendMode::multiply:
    return QPainter::CompositionMode_Multiply;
  default:
    return QPainter::CompositionMode_SourceOver;
  }
}
