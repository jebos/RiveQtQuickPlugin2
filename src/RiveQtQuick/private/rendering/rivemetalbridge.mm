#include "rivemetalbridge.h"

#ifdef RIVEQT_ENABLE_METAL

#include <mutex>
#include <unordered_map>

#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include "rive/artboard.hpp"
#include "rive/renderer/metal/render_context_metal_impl.h"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/scene.hpp"
#include "rivelogging.h"

namespace {
template <typename Handle>
Handle objcHandleFromVoid(void* ptr)
{
  return (__bridge Handle)(ptr);
}

template <typename Handle>
Handle objcHandleFromObject(quint64 object)
{
  return objcHandleFromVoid<Handle>(
    reinterpret_cast<void*>(static_cast<quintptr>(object)));
}
} // namespace

struct RiveMetalBridge::SharedContext {
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> commandQueue = nil;
  std::unique_ptr<rive::gpu::RenderContext> renderContext;
};

namespace {
std::mutex g_sharedContextMutex;
std::unordered_map<quintptr, std::weak_ptr<void>> g_sharedContexts;
}

std::unique_ptr<RiveBackendBridge> createRiveMetalBridge()
{
  return std::make_unique<RiveMetalBridge>();
}

QSGRendererInterface::GraphicsApi RiveMetalBridge::api() const
{
  return QSGRendererInterface::Metal;
}

RiveBackendBridge::TargetKind RiveMetalBridge::targetKind() const
{
  return TargetKind::Texture;
}

bool RiveMetalBridge::syncPresentation(QQuickWindow* window,
  const QSize& pixelSize)
{
  m_window = window;
  if (!window || !window->rhi()) {
    qCDebug(lcRiveMetal) << "Qt did not expose a QRhi for the scenegraph.";
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
    qCDebug(lcRiveMetal) << "failed to create the output texture.";
    return false;
  }

  m_outputPixelSize = pixelSize;
  return true;
}

QRhiTexture* RiveMetalBridge::outputTexture() const
{
  return m_outputTexture;
}

bool RiveMetalBridge::beginFrame(QQuickWindow* window,
  QRhiCommandBuffer* commandBuffer,
  QPainter* painter,
  const QSize& pixelSize,
  bool targetYUp)
{
  Q_UNUSED(painter);
  Q_UNUSED(pixelSize);
  Q_UNUSED(targetYUp);
  m_window = window;
  if (!m_outputTexture) {
    qCDebug(lcRiveMetal) << "output texture is not initialized.";
    return false;
  }

  if (!ensureContext(window, commandBuffer)) {
    return false;
  }

  auto nativeTexture = m_outputTexture->nativeTexture();
  id<MTLTexture> rawTexture = objcHandleFromObject<id<MTLTexture>>(nativeTexture.object);
  if (!rawTexture) {
    qCDebug(lcRiveMetal) << "Qt did not expose an output texture.";
    return false;
  }

  if (!m_renderTarget
    || m_renderTarget->width() != static_cast<uint32_t>(m_outputPixelSize.width())
    || m_renderTarget->height() != static_cast<uint32_t>(m_outputPixelSize.height())) {
    auto* impl =
      m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
    m_renderTarget = impl->makeRenderTarget(
      MTLPixelFormatRGBA8Unorm,
      static_cast<uint32_t>(m_outputPixelSize.width()),
      static_cast<uint32_t>(m_outputPixelSize.height()));
  }

  m_renderTarget->setTargetTexture(rawTexture);
  return true;
}

rive::Factory* RiveMetalBridge::factory() const
{
  return m_sharedContext ? m_sharedContext->renderContext.get() : nullptr;
}

rive::rcp<rive::RenderImage> RiveMetalBridge::createRenderImage(
  const QImage& image)
{
  if (!m_sharedContext || !m_sharedContext->renderContext) {
    qCDebug(lcRiveMetal) << "render context is not initialized.";
    return nullptr;
  }

  const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
  auto* impl =
    m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextMetalImpl>();
  auto texture = impl->makeImageTexture(
    static_cast<uint32_t>(rgba.width()),
    static_cast<uint32_t>(rgba.height()),
    1,
    rgba.constBits());
  return rive::make_rcp<rive::RiveRenderImage>(texture);
}

bool RiveMetalBridge::render(rive::ArtboardInstance* artboard,
  rive::Scene* scene,
  const rive::Mat2D& transform)
{
  if (!m_sharedContext || !m_sharedContext->renderContext || !m_renderTarget) {
    qCDebug(lcRiveMetal) << "render target is not ready.";
    return false;
  }

  id<MTLCommandBuffer> commandBuffer =
    m_sharedContext->commandQueue ? [m_sharedContext->commandQueue commandBuffer] : nil;
  if (!commandBuffer) {
    qCDebug(lcRiveMetal) << "Qt did not expose a Metal command queue.";
    return false;
  }

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
    .externalCommandBuffer = (__bridge void*)commandBuffer,
    .currentFrameNumber = currentFrameNumber,
    .safeFrameNumber = safeFrameNumber });
  [commandBuffer commit];
  return true;
}

void RiveMetalBridge::release()
{
  m_renderTarget = nullptr;
  m_sharedContext.reset();
  m_outputTexture = nullptr;
  m_outputPixelSize = {};
  m_window = nullptr;
  m_frameNumber = 0;
}

bool RiveMetalBridge::ensureContext(QQuickWindow* window,
  QRhiCommandBuffer* commandBuffer)
{
  Q_UNUSED(commandBuffer);
  auto* rendererInterface = window ? window->rendererInterface() : nullptr;
  id<MTLDevice> rawDevice = rendererInterface
    ? objcHandleFromVoid<id<MTLDevice>>(
        rendererInterface->getResource(window, QSGRendererInterface::DeviceResource))
    : nil;
  id<MTLCommandQueue> rawCommandQueue = rendererInterface
    ? objcHandleFromVoid<id<MTLCommandQueue>>(
        rendererInterface->getResource(window, QSGRendererInterface::CommandQueueResource))
    : nil;

  if (!rawDevice || !rawCommandQueue) {
    qCDebug(lcRiveMetal) << "Qt did not expose a device/command queue for the scenegraph.";
    return false;
  }

  if (m_sharedContext && m_sharedContext->renderContext) {
    return true;
  }

  const quintptr contextKey = reinterpret_cast<quintptr>(rawDevice) ^
    (reinterpret_cast<quintptr>(rawCommandQueue) << 1);

  {
    std::lock_guard<std::mutex> lock(g_sharedContextMutex);
    auto it = g_sharedContexts.find(contextKey);
    if (it != g_sharedContexts.end()) {
      m_sharedContext = std::static_pointer_cast<SharedContext>(it->second.lock());
    }

    if (!m_sharedContext) {
      auto sharedContext = std::make_shared<SharedContext>();
      sharedContext->device = rawDevice;
      sharedContext->commandQueue = rawCommandQueue;
      sharedContext->renderContext = rive::gpu::RenderContextMetalImpl::MakeContext(rawDevice);
      if (!sharedContext->renderContext) {
        qCDebug(lcRiveMetal) << "failed to create the official render context.";
        return false;
      }

      g_sharedContexts[contextKey] = sharedContext;
      m_sharedContext = std::move(sharedContext);
    }
  }

  return m_sharedContext && m_sharedContext->renderContext;
}

#endif
