#include "qpainterrivepaint.h"

#include <algorithm>

#include <QSet>

#include "../rivelogging.h"
#include "qpainterriveshader.h"

#include "rive/shapes/paint/color.hpp"

namespace {
void logUnsupportedBlendMode(rive::BlendMode mode)
{
  static QSet<int> loggedModes;
  const int key = static_cast<int>(mode);
  if (loggedModes.contains(key)) {
    return;
  }

  loggedModes.insert(key);
  qCWarning(lcRiveSoftware) << "approximating unsupported software blend mode" << key
                            << "with SourceOver";
}

void logIgnoredFeather()
{
  static bool logged = false;
  if (logged) {
    return;
  }

  logged = true;
  qCWarning(lcRiveSoftware) << "approximating feathered paint softness in software rendering";
}
} // namespace

void QPainterRivePaint::style(rive::RenderPaintStyle style)
{
  m_isStroke = style == rive::RenderPaintStyle::stroke;
}

void QPainterRivePaint::color(rive::ColorInt value)
{
  m_color = toQColor(value);
}

void QPainterRivePaint::thickness(float value)
{
  m_strokeWidth = value;
}

void QPainterRivePaint::join(rive::StrokeJoin value)
{
  m_joinStyle = toQtJoinStyle(value);
}

void QPainterRivePaint::cap(rive::StrokeCap value)
{
  m_capStyle = toQtCapStyle(value);
}

void QPainterRivePaint::feather(float value)
{
  m_feather = std::max(0.0f, value);
  if (value > 0.0f) {
    logIgnoredFeather();
  }
}

void QPainterRivePaint::blendMode(rive::BlendMode value)
{
  m_compositionMode = toCompositionMode(value);
}

void QPainterRivePaint::shader(rive::rcp<rive::RenderShader> shader)
{
  m_shader = std::move(shader);
}

void QPainterRivePaint::invalidateStroke()
{
}

bool QPainterRivePaint::isStroke() const
{
  return m_isStroke;
}

float QPainterRivePaint::strokeWidth() const
{
  return m_strokeWidth;
}

float QPainterRivePaint::feather() const
{
  return m_feather;
}

Qt::PenJoinStyle QPainterRivePaint::joinStyle() const
{
  return m_joinStyle;
}

Qt::PenCapStyle QPainterRivePaint::capStyle() const
{
  return m_capStyle;
}

QPainter::CompositionMode QPainterRivePaint::compositionMode() const
{
  return m_compositionMode;
}

QBrush QPainterRivePaint::brush() const
{
  if (m_shader) {
    auto* shader = static_cast<QPainterRiveShader*>(m_shader.get());
    return shader->brush();
  }

  return QBrush(m_color);
}

QColor QPainterRivePaint::toQColor(rive::ColorInt value)
{
  return QColor(rive::colorRed(value),
    rive::colorGreen(value),
    rive::colorBlue(value),
    rive::colorAlpha(value));
}

Qt::PenJoinStyle QPainterRivePaint::toQtJoinStyle(rive::StrokeJoin value)
{
  switch (value) {
  case rive::StrokeJoin::round:
    return Qt::RoundJoin;
  case rive::StrokeJoin::bevel:
    return Qt::BevelJoin;
  default:
    return Qt::MiterJoin;
  }
}

Qt::PenCapStyle QPainterRivePaint::toQtCapStyle(rive::StrokeCap value)
{
  switch (value) {
  case rive::StrokeCap::round:
    return Qt::RoundCap;
  case rive::StrokeCap::square:
    return Qt::SquareCap;
  default:
    return Qt::FlatCap;
  }
}

QPainter::CompositionMode QPainterRivePaint::toCompositionMode(rive::BlendMode value)
{
  switch (value) {
  case rive::BlendMode::srcOver:
    return QPainter::CompositionMode_SourceOver;
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
  case rive::BlendMode::hue:
  case rive::BlendMode::saturation:
  case rive::BlendMode::color:
  case rive::BlendMode::luminosity:
    logUnsupportedBlendMode(value);
    return QPainter::CompositionMode_SourceOver;
  default:
    return QPainter::CompositionMode_SourceOver;
  }
}
