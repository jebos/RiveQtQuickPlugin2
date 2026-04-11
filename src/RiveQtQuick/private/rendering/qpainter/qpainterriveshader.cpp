#include "qpainterriveshader.h"

QPainterRiveShader::QPainterRiveShader(QBrush brush)
    : m_brush(std::move(brush))
{
}

const QBrush& QPainterRiveShader::brush() const
{
  return m_brush;
}
