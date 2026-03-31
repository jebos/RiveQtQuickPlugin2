#pragma once

#include <QBrush>

#include "rive/renderer.hpp"

class QPainterRiveShader final : public rive::RenderShader {
  public:
  explicit QPainterRiveShader(QBrush brush);

  const QBrush& brush() const;

  private:
  QBrush m_brush;
};
