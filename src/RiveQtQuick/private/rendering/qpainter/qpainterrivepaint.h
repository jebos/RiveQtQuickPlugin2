#pragma once

#include <QColor>
#include <QPainter>

#include "rive/renderer.hpp"

class QPainterRiveShader;

class QPainterRivePaint final : public rive::RenderPaint {
  public:
  void style(rive::RenderPaintStyle style) override;
  void color(rive::ColorInt value) override;
  void thickness(float value) override;
  void join(rive::StrokeJoin value) override;
  void cap(rive::StrokeCap value) override;
  void feather(float value) override;
  void blendMode(rive::BlendMode value) override;
  void shader(rive::rcp<rive::RenderShader> shader) override;
  void invalidateStroke() override;

  bool isStroke() const;
  float strokeWidth() const;
  float feather() const;
  Qt::PenJoinStyle joinStyle() const;
  Qt::PenCapStyle capStyle() const;
  QPainter::CompositionMode compositionMode() const;
  QBrush brush() const;

  private:
  static QColor toQColor(rive::ColorInt value);
  static Qt::PenJoinStyle toQtJoinStyle(rive::StrokeJoin value);
  static Qt::PenCapStyle toQtCapStyle(rive::StrokeCap value);
  static QPainter::CompositionMode toCompositionMode(rive::BlendMode value);

  bool m_isStroke { false };
  QColor m_color { Qt::white };
  float m_strokeWidth { 1.0f };
  float m_feather { 0.0f };
  Qt::PenJoinStyle m_joinStyle { Qt::MiterJoin };
  Qt::PenCapStyle m_capStyle { Qt::FlatCap };
  QPainter::CompositionMode m_compositionMode { QPainter::CompositionMode_SourceOver };
  rive::rcp<rive::RenderShader> m_shader;
};
