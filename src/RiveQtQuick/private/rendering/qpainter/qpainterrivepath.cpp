#include "qpainterrivepath.h"

#include "rive/math/raw_path.hpp"

QPainterRivePath::QPainterRivePath(rive::RawPath& rawPath,
  rive::FillRule fillRule)
{
  m_path.setFillRule(toQtFillRule(fillRule));
  addRawPath(rawPath);
}

const QPainterPath& QPainterRivePath::path() const
{
  return m_path;
}

void QPainterRivePath::rewind()
{
  const Qt::FillRule fillRule = m_path.fillRule();
  m_path = QPainterPath();
  m_path.setFillRule(fillRule);
}

void QPainterRivePath::fillRule(rive::FillRule value)
{
  m_path.setFillRule(toQtFillRule(value));
}

void QPainterRivePath::addRenderPath(rive::RenderPath* path,
  const rive::Mat2D& transform)
{
  if (!path) {
    return;
  }

  auto* qpPath = static_cast<QPainterRivePath*>(path);
  m_path.addPath(toQTransform(transform).map(qpPath->path()));
}

void QPainterRivePath::moveTo(float x,
  float y)
{
  m_path.moveTo(x, y);
}

void QPainterRivePath::lineTo(float x,
  float y)
{
  m_path.lineTo(x, y);
}

void QPainterRivePath::cubicTo(float ox,
  float oy,
  float ix,
  float iy,
  float x,
  float y)
{
  m_path.cubicTo(ox, oy, ix, iy, x, y);
}

void QPainterRivePath::close()
{
  m_path.closeSubpath();
}

void QPainterRivePath::addRawPath(const rive::RawPath& path)
{
  for (auto [verb, pts] : path) {
    switch (verb) {
    case rive::PathVerb::move:
      m_path.moveTo(pts[0].x, pts[0].y);
      break;
    case rive::PathVerb::line:
      m_path.lineTo(pts[1].x, pts[1].y);
      break;
    case rive::PathVerb::quad:
      m_path.quadTo(pts[1].x, pts[1].y, pts[2].x, pts[2].y);
      break;
    case rive::PathVerb::cubic:
      m_path.cubicTo(pts[1].x,
        pts[1].y,
        pts[2].x,
        pts[2].y,
        pts[3].x,
        pts[3].y);
      break;
    case rive::PathVerb::close:
      m_path.closeSubpath();
      break;
    }
  }
}

Qt::FillRule QPainterRivePath::toQtFillRule(rive::FillRule value)
{
  return value == rive::FillRule::evenOdd ? Qt::OddEvenFill : Qt::WindingFill;
}

QTransform QPainterRivePath::toQTransform(const rive::Mat2D& transform)
{
  return QTransform(transform[0],
    transform[1],
    transform[2],
    transform[3],
    transform[4],
    transform[5]);
}
