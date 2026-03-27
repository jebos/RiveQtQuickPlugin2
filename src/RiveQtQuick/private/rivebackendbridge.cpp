#include "rivebackendbridge.h"

#include "rivelogging.h"

#ifdef RIVEQT_ENABLE_OPENGL
#include "riveglbridge.h"
#endif

#ifdef Q_OS_WIN
#include "rived3d11bridge.h"
#endif

#ifdef RIVEQT_ENABLE_D3D12
std::unique_ptr<RiveBackendBridge> createRiveD3D12Bridge();
#endif

#ifdef RIVEQT_ENABLE_METAL
std::unique_ptr<RiveBackendBridge> createRiveMetalBridge();
#endif

#ifdef RIVEQT_ENABLE_VULKAN
std::unique_ptr<RiveBackendBridge> createRiveVulkanBridge();
#endif

std::unique_ptr<RiveBackendBridge> RiveBackendBridge::create(
  QSGRendererInterface::GraphicsApi api)
{
  switch (api) {
  case QSGRendererInterface::OpenGL:
#ifdef RIVEQT_ENABLE_OPENGL
    return createRiveGLBridge();
#else
    break;
#endif
  case QSGRendererInterface::Direct3D11:
#ifdef Q_OS_WIN
    return std::make_unique<RiveD3D11Bridge>();
#else
    break;
#endif
  case QSGRendererInterface::Direct3D12:
#if defined(Q_OS_WIN) && defined(RIVEQT_ENABLE_D3D12)
    return createRiveD3D12Bridge();
#else
    break;
#endif
  case QSGRendererInterface::Metal:
#ifdef RIVEQT_ENABLE_METAL
    return createRiveMetalBridge();
#else
    break;
#endif
  case QSGRendererInterface::Vulkan:
#ifdef RIVEQT_ENABLE_VULKAN
    return createRiveVulkanBridge();
#else
    break;
#endif
  default:
    break;
  }

  qCDebug(lcRiveBackend).noquote()
    << "backend not implemented for"
    << graphicsApiName(api);
  return nullptr;
}

QString RiveBackendBridge::graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
  switch (api) {
  case QSGRendererInterface::Software:
    return "Software";
  case QSGRendererInterface::OpenVG:
    return "OpenVG";
  case QSGRendererInterface::OpenGL:
    return "OpenGL";
  case QSGRendererInterface::Direct3D11:
    return "Direct3D11";
  case QSGRendererInterface::Vulkan:
    return "Vulkan";
  case QSGRendererInterface::Metal:
    return "Metal";
  case QSGRendererInterface::Null:
    return "Null";
  case QSGRendererInterface::Direct3D12:
    return "Direct3D12";
  default:
    return "Unknown";
  }
}
