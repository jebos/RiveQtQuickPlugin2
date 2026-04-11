#pragma once

#include <QImage>
#include <QPainter>

#include "rive/renderer.hpp"

class QPainterRivePaint;

class QPainterRiveRenderer final : public rive::Renderer {
  public:
  explicit QPainterRiveRenderer(QPainter* painter);

  void save() override;
  void restore() override;
  void transform(const rive::Mat2D& transform) override;
  void drawPath(rive::RenderPath* path,
    rive::RenderPaint* paint) override;
  void clipPath(rive::RenderPath* path) override;
  void drawImage(const rive::RenderImage* image,
    rive::ImageSampler sampler,
    rive::BlendMode blendMode,
    float opacity) override;
  void drawImageMesh(const rive::RenderImage* image,
    rive::ImageSampler sampler,
    rive::rcp<rive::RenderBuffer> vertices_f32,
    rive::rcp<rive::RenderBuffer> uvCoords_f32,
    rive::rcp<rive::RenderBuffer> indices_u16,
    uint32_t vertexCount,
    uint32_t indexCount,
    rive::BlendMode blendMode,
    float opacity) override;
  void modulateOpacity(float opacity) override;

  private:
  bool drawFeatheredPath(const QPainterPath& path,
    const QPainterRivePaint& paint);
  void setDeviceClipPath(const QPainterPath& path,
    Qt::ClipOperation operation);

  static QImage boxBlurImage(const QImage& image,
    int radius);
  static int blurRadiusForFeather(float feather,
    const QTransform& transform);
  static qreal maxScaleForTransform(const QTransform& transform);
  static QTransform toQTransform(const rive::Mat2D& transform);
  static rive::Mat2D basisMatrix(const rive::Vec2D& p0,
    const rive::Vec2D& p1,
    const rive::Vec2D& p2);
  static QPainter::CompositionMode toCompositionMode(rive::BlendMode value);

  QPainter* m_painter { nullptr };
  std::vector<float> m_opacityStack { 1.0f };
};
