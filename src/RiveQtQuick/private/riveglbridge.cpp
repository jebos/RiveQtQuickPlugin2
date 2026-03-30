#include "riveglbridge.h"

#ifdef RIVEQT_ENABLE_OPENGL

#include <mutex>
#include <unordered_map>

#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include "qtglprocaddress.h"
#ifdef GLAPI
#undef GLAPI
#endif
#include "rive/artboard.hpp"
#include "rive/renderer/gl/gles3.hpp"
#include "rive/renderer/gl/render_context_gl_impl.hpp"
#include "rive/renderer/gl/render_target_gl.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/scene.hpp"
#include "rivelogging.h"

struct RiveGLBridge::SharedContext {
  void* context { nullptr };
  std::unique_ptr<rive::gpu::RenderContext> renderContext;
};

namespace {
std::mutex g_sharedContextMutex;
std::unordered_map<quintptr, std::weak_ptr<void>> g_sharedContexts;
thread_local void* g_gladLoaderContext = nullptr;

GLADapiproc loadGladSymbol(const char* name)
{
  return reinterpret_cast<GLADapiproc>(
    resolveQtOpenGLProcAddress(g_gladLoaderContext, name));
}

bool isDesktopOpenGL42OrNewer(void* contextHandle)
{
  const QtOpenGLContextInfo contextInfo = queryQtOpenGLContextInfo(contextHandle);
  if (!contextInfo.valid) {
    qCWarning(lcRiveGL)
      << "Qt did not expose a QOpenGLContext for the scenegraph.";
    return false;
  }

  if (contextInfo.isOpenGLES) {
    qCWarning(lcRiveGL).nospace()
      << "Desktop OpenGL backend requires a desktop OpenGL 4.2+ core context, "
      << "but Qt created OpenGL ES "
      << contextInfo.majorVersion << '.' << contextInfo.minorVersion << '.';
    return false;
  }

  if (contextInfo.majorVersion < 4
    || (contextInfo.majorVersion == 4 && contextInfo.minorVersion < 2)) {
    qCWarning(lcRiveGL).nospace()
      << "Desktop OpenGL backend requires OpenGL 4.2+, but Qt created "
      << contextInfo.majorVersion << '.' << contextInfo.minorVersion << '.';
    return false;
  }

  return true;
}
} // namespace

std::unique_ptr<RiveBackendBridge> createRiveGLBridge()
{
  return std::make_unique<RiveGLBridge>();
}

QSGRendererInterface::GraphicsApi RiveGLBridge::api() const
{
  return QSGRendererInterface::OpenGL;
}

bool RiveGLBridge::syncPresentation(QQuickWindow* window,
  const QSize& pixelSize)
{
  m_window = window;
  if (!window || !window->rhi()) {
    qCDebug(lcRiveGL) << "Qt did not expose a QRhi for the scenegraph.";
    return false;
  }

  if (pixelSize.isEmpty()) {
    m_outputPixelSize = {};
    return true;
  }

  bool needsCreate = !m_outputTexture || m_outputPixelSize != pixelSize;
  if (!m_outputTexture) {
    m_outputTexture = window->rhi()->newTexture(
      QRhiTexture::RGBA8,
      pixelSize,
      1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
  } else if (m_outputPixelSize != pixelSize) {
    m_outputTexture->setPixelSize(pixelSize);
  }

  if (needsCreate && !m_outputTexture->create()) {
    qCDebug(lcRiveGL) << "failed to create the output texture.";
    return false;
  }

  m_outputPixelSize = pixelSize;
  return true;
}

QRhiTexture* RiveGLBridge::outputTexture() const
{
  return m_outputTexture;
}

bool RiveGLBridge::requiresExternalCommands() const
{
  return true;
}

bool RiveGLBridge::prepareFrame(QQuickWindow* window,
  QRhiCommandBuffer* commandBuffer)
{
  Q_UNUSED(commandBuffer);
  m_window = window;
  if (!m_outputTexture) {
    qCDebug(lcRiveGL) << "output texture is not initialized.";
    return false;
  }

  if (!ensureContext(window)) {
    return false;
  }

  const auto nativeTexture = m_outputTexture->nativeTexture();
  const GLuint textureId = static_cast<GLuint>(nativeTexture.object);
  if (!textureId) {
    qCDebug(lcRiveGL) << "Qt did not expose an output texture.";
    return false;
  }

  if (!m_renderTarget
    || m_renderTarget->width() != static_cast<uint32_t>(m_outputPixelSize.width())
    || m_renderTarget->height() != static_cast<uint32_t>(m_outputPixelSize.height())) {
    m_renderTarget = std::make_unique<rive::gpu::TextureRenderTargetGL>(
      static_cast<uint32_t>(m_outputPixelSize.width()),
      static_cast<uint32_t>(m_outputPixelSize.height()));
  }
  m_renderTarget->setTargetTexture(textureId);
  return true;
}

rive::Factory* RiveGLBridge::factory() const
{
  return m_sharedContext ? m_sharedContext->renderContext.get() : nullptr;
}

rive::rcp<rive::RenderImage> RiveGLBridge::createRenderImage(
  const QImage& image)
{
  if (!m_sharedContext || !m_sharedContext->renderContext) {
    qCDebug(lcRiveGL) << "render context is not initialized.";
    return nullptr;
  }

  const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
  auto* impl =
    m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>();
  auto texture = impl->makeImageTexture(
    static_cast<uint32_t>(rgba.width()),
    static_cast<uint32_t>(rgba.height()),
    1,
    rgba.constBits());
  return rive::make_rcp<rive::RiveRenderImage>(texture);
}

bool RiveGLBridge::render(rive::ArtboardInstance* artboard,
  rive::Scene* scene,
  const rive::Mat2D& transform)
{
  if (!m_sharedContext || !m_sharedContext->renderContext || !m_renderTarget) {
    qCDebug(lcRiveGL) << "render target is not ready.";
    return false;
  }

  auto* impl =
    m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextGLImpl>();

  if (!isQtOpenGLContextCurrent(m_loaderContext ? m_loaderContext : m_contextHandle)) {
    qCDebug(lcRiveGL).nospace()
      << "OpenGL render called without the scenegraph context current. current="
      << currentQtOpenGLContext() << " expected=" << m_loaderContext
      << " resource=" << m_contextHandle;
    return false;
  }

  if (!glad_glGetString) {
    qCDebug(lcRiveGL) << "glGetString is unavailable at render time.";
    return false;
  }

  impl->invalidateGLState();

  rive::gpu::RenderContext::FrameDescriptor descriptor = {};
  descriptor.renderTargetWidth = static_cast<uint32_t>(m_outputPixelSize.width());
  descriptor.renderTargetHeight = static_cast<uint32_t>(m_outputPixelSize.height());
  descriptor.loadAction = rive::gpu::LoadAction::clear;
  descriptor.clearColor = 0;

  m_sharedContext->renderContext->beginFrame(descriptor);
  rive::RiveRenderer renderer(m_sharedContext->renderContext.get());
  renderer.save();
  renderer.transform(transform);

  if (scene) {
    scene->draw(&renderer);
  } else if (artboard) {
    artboard->draw(&renderer);
  }

  renderer.restore();
  auto graphicsState = m_window ? m_window->graphicsStateInfo()
                                : QQuickWindow::GraphicsStateInfo { 0, 1 };
  uint64_t currentFrameNumber = ++m_frameNumber;
  uint64_t safeFrameNumber = currentFrameNumber > static_cast<uint64_t>(graphicsState.framesInFlight)
    ? currentFrameNumber - static_cast<uint64_t>(graphicsState.framesInFlight)
    : 0;
  m_sharedContext->renderContext->flush({ .renderTarget = m_renderTarget.get(),
    .currentFrameNumber = currentFrameNumber,
    .safeFrameNumber = safeFrameNumber });
  impl->unbindGLInternalResources();
  return true;
}

void RiveGLBridge::release()
{
  m_renderTarget.reset();
  m_sharedContext.reset();
  m_outputTexture = nullptr;
  m_outputPixelSize = {};
  m_contextHandle = nullptr;
  m_loaderContext = nullptr;
  m_window = nullptr;
  m_frameNumber = 0;
}

bool RiveGLBridge::ensureContext(QQuickWindow* window)
{
  auto* rendererInterface = window ? window->rendererInterface() : nullptr;
  void* rawContext = rendererInterface
    ? rendererInterface->getResource(window, QSGRendererInterface::OpenGLContextResource)
    : nullptr;
  const auto* nativeHandles =
    window && window->rhi()
      ? static_cast<const QRhiGles2NativeHandles*>(window->rhi()->nativeHandles())
      : nullptr;
  void* loaderContext = nativeHandles ? nativeHandles->context : rawContext;
  if (!rawContext) {
    rawContext = loaderContext;
  }
  if (!rawContext || !loaderContext) {
    qCDebug(lcRiveGL) << "Qt did not expose an OpenGL context for the scenegraph.";
    return false;
  }

  if (!isDesktopOpenGL42OrNewer(loaderContext)) {
    return false;
  }

  m_contextHandle = rawContext;
  m_loaderContext = loaderContext;

  if (m_sharedContext && m_sharedContext->renderContext) {
    return true;
  }

  const quintptr contextKey = reinterpret_cast<quintptr>(rawContext);

  {
    std::lock_guard<std::mutex> lock(g_sharedContextMutex);
    auto it = g_sharedContexts.find(contextKey);
    if (it != g_sharedContexts.end()) {
      m_sharedContext = std::static_pointer_cast<SharedContext>(it->second.lock());
      if (m_sharedContext) {
        qCDebug(lcRiveGL).nospace()
          << "reusing shared OpenGL render context for " << rawContext;
      }
    }

    if (!m_sharedContext) {
      auto sharedContext = std::make_shared<SharedContext>();
      sharedContext->context = rawContext;

      if (!resolveQtOpenGLProcAddress(loaderContext, "glGetString")) {
        qCDebug(lcRiveGL) << "failed to resolve glGetString through Qt.";
        return false;
      }

      g_gladLoaderContext = loaderContext;
      const int gladResult = gladLoadCustomLoader(loadGladSymbol);
      g_gladLoaderContext = nullptr;
      if (gladResult == 0) {
        qCDebug(lcRiveGL) << "failed to load OpenGL symbols through Qt.";
        return false;
      }

      sharedContext->renderContext = rive::gpu::RenderContextGLImpl::MakeContext();
      if (!sharedContext->renderContext) {
        qCDebug(lcRiveGL) << "failed to create the official render context.";
        return false;
      }

      qCDebug(lcRiveGL).nospace()
        << "created shared OpenGL render context for " << rawContext;
      g_sharedContexts[contextKey] = sharedContext;
      m_sharedContext = std::move(sharedContext);
    }
  }

  return m_sharedContext && m_sharedContext->renderContext;
}

#endif
