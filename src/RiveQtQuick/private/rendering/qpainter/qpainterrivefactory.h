#pragma once

#include "rive/factory.hpp"

class QPainterRiveFactory final : public rive::Factory {
  public:
  rive::rcp<rive::RenderBuffer> makeRenderBuffer(rive::RenderBufferType type,
    rive::RenderBufferFlags flags,
    size_t sizeInBytes) override;
  rive::rcp<rive::RenderShader> makeLinearGradient(float sx,
    float sy,
    float ex,
    float ey,
    const rive::ColorInt colors[],
    const float stops[],
    size_t count) override;
  rive::rcp<rive::RenderShader> makeRadialGradient(float cx,
    float cy,
    float radius,
    const rive::ColorInt colors[],
    const float stops[],
    size_t count) override;
  rive::rcp<rive::RenderPath> makeRenderPath(rive::RawPath& rawPath,
    rive::FillRule fillRule) override;
  rive::rcp<rive::RenderPath> makeEmptyRenderPath() override;
  rive::rcp<rive::RenderPaint> makeRenderPaint() override;
  rive::rcp<rive::RenderImage> decodeImage(rive::Span<const uint8_t> data) override;
};
