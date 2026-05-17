#pragma once

#include <cstdint>

#if __has_include("rive/gpu_texture_format.hpp")
#include "rive/gpu_texture_format.hpp"
#define RIVEQT_HAS_RIVE_GPU_TEXTURE_FORMAT 1
#else
#define RIVEQT_HAS_RIVE_GPU_TEXTURE_FORMAT 0
#endif

namespace riveqt::rendering {

template <typename RenderContextImpl>
auto makeRiveImageTexture(RenderContextImpl* impl,
  uint32_t width,
  uint32_t height,
  uint32_t mipLevelCount,
  const uint8_t* imageDataRGBAPremul)
{
#if RIVEQT_HAS_RIVE_GPU_TEXTURE_FORMAT
  if constexpr (requires {
                  impl->makeImageTexture(width,
                    height,
                    mipLevelCount,
                    rive::GPUTextureFormat::rgba32,
                    imageDataRGBAPremul);
                }) {
    return impl->makeImageTexture(width,
      height,
      mipLevelCount,
      rive::GPUTextureFormat::rgba32,
      imageDataRGBAPremul);
  } else
#endif
  {
    return impl->makeImageTexture(width,
      height,
      mipLevelCount,
      imageDataRGBAPremul);
  }
}

} // namespace riveqt::rendering

#undef RIVEQT_HAS_RIVE_GPU_TEXTURE_FORMAT
