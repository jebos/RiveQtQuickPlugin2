#pragma once

#include <QImage>

#include "rive/renderer.hpp"

class QPainterRiveImage final : public rive::RenderImage {
  public:
  explicit QPainterRiveImage(QImage image);
  QPainterRiveImage(QImage image,
    const rive::Mat2D& uvTransform);

  const QImage& image() const;

  private:
  QImage m_image;
};
