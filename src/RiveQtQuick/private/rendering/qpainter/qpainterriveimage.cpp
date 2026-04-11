#include "qpainterriveimage.h"

QPainterRiveImage::QPainterRiveImage(QImage image)
    : m_image(std::move(image))
{
  m_Width = m_image.width();
  m_Height = m_image.height();
}

QPainterRiveImage::QPainterRiveImage(QImage image,
  const rive::Mat2D& uvTransform)
    : rive::RenderImage(uvTransform)
    , m_image(std::move(image))
{
  m_Width = m_image.width();
  m_Height = m_image.height();
}

const QImage& QPainterRiveImage::image() const
{
  return m_image;
}
