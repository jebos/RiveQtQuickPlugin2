#include "rived3d11bridge.h"

#ifdef Q_OS_WIN

#include <mutex>
#include <unordered_map>

#include <d3d11.h>

#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include "rive/artboard.hpp"
#include "rive/renderer/d3d/d3d.hpp"
#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/scene.hpp"
#include "rivelogging.h"

using Microsoft::WRL::ComPtr;

struct RiveD3D11Bridge::SharedContext {
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> deviceContext;
  std::unique_ptr<rive::gpu::RenderContext> renderContext;
};

namespace {
std::mutex g_sharedContextMutex;
std::unordered_map<quintptr, std::weak_ptr<void>>
  g_sharedContexts;
}

QSGRendererInterface::GraphicsApi RiveD3D11Bridge::api() const
{
  return QSGRendererInterface::Direct3D11;
}

RiveBackendBridge::TargetKind RiveD3D11Bridge::targetKind() const
{
  return TargetKind::Texture;
}

bool RiveD3D11Bridge::syncPresentation(QQuickWindow* window,
  const QSize& pixelSize)
{
  m_window = window;
  if (!window || !window->rhi()) {
    qCDebug(lcRiveD3D11) << "Qt did not expose a QRhi for the scenegraph.";
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
    qCDebug(lcRiveD3D11) << "failed to create the output texture.";
    return false;
  }

  m_outputPixelSize = pixelSize;
  return true;
}

QRhiTexture* RiveD3D11Bridge::outputTexture() const
{
  return m_outputTexture;
}

bool RiveD3D11Bridge::beginFrame(QQuickWindow* window,
  QRhiCommandBuffer*,
  QPainter* painter,
  const QSize& pixelSize,
  bool targetYUp)
{
  Q_UNUSED(painter);
  Q_UNUSED(pixelSize);
  Q_UNUSED(targetYUp);
  m_window = window;
  if (!m_outputTexture) {
    qCDebug(lcRiveD3D11) << "output texture is not initialized.";
    return false;
  }

  if (!ensureContext(window)) {
    return false;
  }

  return updateRenderTarget();
}

rive::Factory* RiveD3D11Bridge::factory() const
{
  return m_sharedContext ? m_sharedContext->renderContext.get() : nullptr;
}

rive::rcp<rive::RenderImage> RiveD3D11Bridge::createRenderImage(
  const QImage& image)
{
  if (!m_sharedContext || !m_sharedContext->renderContext) {
    qCDebug(lcRiveD3D11) << "render context is not initialized.";
    return nullptr;
  }

  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(rgba.width());
  desc.Height = static_cast<UINT>(rgba.height());
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  D3D11_SUBRESOURCE_DATA initialData = {};
  initialData.pSysMem = rgba.constBits();
  initialData.SysMemPitch = static_cast<UINT>(rgba.bytesPerLine());

  ComPtr<ID3D11Texture2D> texture;
  HRESULT hr = m_sharedContext->device->CreateTexture2D(&desc,
    &initialData,
    texture.GetAddressOf());
  if (FAILED(hr)) {
    qCDebug(lcRiveD3D11) << "failed to create a hosted image texture.";
    return nullptr;
  }

  auto* impl = m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextD3DImpl>();
  return rive::make_rcp<rive::RiveRenderImage>(
    impl->adoptImageTexture(texture, static_cast<uint32_t>(rgba.width()), static_cast<uint32_t>(rgba.height())));
}

bool RiveD3D11Bridge::render(rive::ArtboardInstance* artboard,
  rive::Scene* scene,
  const rive::Mat2D& transform)
{
  if (!m_sharedContext || !m_sharedContext->renderContext || !m_renderTarget) {
    qCDebug(lcRiveD3D11) << "render target is not ready.";
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
    .currentFrameNumber = currentFrameNumber,
    .safeFrameNumber = safeFrameNumber });
  return true;
}

void RiveD3D11Bridge::release()
{
  m_renderTarget = nullptr;
  m_sharedContext.reset();
  m_outputTexture = nullptr;
  m_outputPixelSize = {};
}

bool RiveD3D11Bridge::ensureContext(QQuickWindow* window)
{
  if (m_sharedContext && m_sharedContext->renderContext) {
    return true;
  }

  auto* rendererInterface = window->rendererInterface();
  auto* rawDevice = static_cast<ID3D11Device*>(
    rendererInterface->getResource(window, QSGRendererInterface::DeviceResource));
  auto* rawContext = static_cast<ID3D11DeviceContext*>(
    rendererInterface->getResource(window, QSGRendererInterface::DeviceContextResource));

  if (!rawDevice || !rawContext) {
    qCDebug(lcRiveD3D11) << "Qt did not expose a device/context for the scenegraph.";
    return false;
  }

  const quintptr contextKey = reinterpret_cast<quintptr>(rawDevice) ^ (reinterpret_cast<quintptr>(rawContext) << 1);

  {
    std::lock_guard<std::mutex> lock(g_sharedContextMutex);
    auto it = g_sharedContexts.find(contextKey);
    if (it != g_sharedContexts.end()) {
      m_sharedContext = std::static_pointer_cast<SharedContext>(it->second.lock());
    }

    if (!m_sharedContext) {
      auto sharedContext = std::make_shared<SharedContext>();
      sharedContext->device = rawDevice;
      sharedContext->deviceContext = rawContext;

      rive::gpu::D3DContextOptions options;
      sharedContext->renderContext = rive::gpu::RenderContextD3DImpl::MakeContext(
        sharedContext->device,
        sharedContext->deviceContext,
        options);
      if (!sharedContext->renderContext) {
        qCDebug(lcRiveD3D11) << "failed to create the official render context.";
        return false;
      }

      g_sharedContexts[contextKey] = sharedContext;
      m_sharedContext = std::move(sharedContext);
    }
  }

  if (!m_sharedContext || !m_sharedContext->renderContext) {
    return false;
  }

  return true;
}

bool RiveD3D11Bridge::updateRenderTarget()
{
  if (!m_renderTarget || m_renderTarget->width() != static_cast<uint32_t>(m_outputPixelSize.width()) || m_renderTarget->height() != static_cast<uint32_t>(m_outputPixelSize.height())) {
    auto* impl = m_sharedContext->renderContext->static_impl_cast<rive::gpu::RenderContextD3DImpl>();
    m_renderTarget = impl->makeRenderTarget(
      static_cast<uint32_t>(m_outputPixelSize.width()),
      static_cast<uint32_t>(m_outputPixelSize.height()));
  }

  auto nativeTexture = m_outputTexture->nativeTexture();
  auto* rawTexture = reinterpret_cast<ID3D11Texture2D*>(
    static_cast<quintptr>(nativeTexture.object));
  if (!rawTexture) {
    qCDebug(lcRiveD3D11) << "Qt did not expose an output texture.";
    return false;
  }

  ComPtr<ID3D11Texture2D> texture;
  texture = rawTexture;
  m_renderTarget->setTargetTexture(texture);
  return true;
}

#endif
