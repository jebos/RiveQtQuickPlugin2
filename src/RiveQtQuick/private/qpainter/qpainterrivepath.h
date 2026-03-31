#pragma once

#include <QPainterPath>
#include <QTransform>

#include "rive/renderer.hpp"

class QPainterRivePath final : public rive::RenderPath {
  public:
  QPainterRivePath() = default;
  QPainterRivePath(rive::RawPath& rawPath,
    rive::FillRule fillRule);

  const QPainterPath& path() const;

  void rewind() override;
  void fillRule(rive::FillRule value) override;
  void addRenderPath(rive::RenderPath* path,
    const rive::Mat2D& transform) override;
  void moveTo(float x,
    float y) override;
  void lineTo(float x,
    float y) override;
  void cubicTo(float ox,
    float oy,
    float ix,
    float iy,
    float x,
    float y) override;
  void close() override;
  void addRawPath(const rive::RawPath& path) override;

  private:
  static Qt::FillRule toQtFillRule(rive::FillRule value);
  static QTransform toQTransform(const rive::Mat2D& transform);

  QPainterPath m_path;
};
