#include "riveqtquick.h"

#include <QThread>
#include <QVariant>
#include <QtQuick/QQuickWindow>
#include <rhi/qrhi.h>

#include "private/rendering/rivebackendbridge.h"
#include "private/rivelogging.h"

namespace {
struct PreloadState {
  std::unique_ptr<RiveBackendBridge> bridge;
  bool attempted { false };
};
}

void RiveQtQuick::preload(QQuickWindow* window)
{
  if (!window) {
    return;
  }
  Q_ASSERT(QThread::currentThread() == window->thread());
  if (window->property("_riveqt_preload").toBool()) {
    return;
  }
  window->setProperty("_riveqt_preload", true);

  auto state = std::make_shared<PreloadState>();
  QObject::connect(window, &QQuickWindow::beforeRendering, window,
    [window, state]() {
      if (state->attempted) {
        return;
      }
      state->attempted = true;

      auto* renderer = window->rendererInterface();
      if (renderer->graphicsApi() == QSGRendererInterface::Software) {
        return;
      }
      state->bridge = RiveBackendBridge::create(renderer->graphicsApi());
      if (!state->bridge) {
        qCWarning(lcRiveBackend) << "Renderer preload is unavailable for this graphics API.";
        return;
      }

      window->beginExternalCommands();
      auto* commandBuffer = window->swapChain()
        ? window->swapChain()->currentFrameCommandBuffer()
        : static_cast<QRhiCommandBuffer*>(renderer->getResource(
            window, QSGRendererInterface::RhiRedirectCommandBuffer));
      if (!state->bridge->preload(window, commandBuffer)) {
        state->bridge->release();
        state->bridge.reset();
        qCWarning(lcRiveBackend) << "Failed to preload the Rive renderer.";
      }
      window->endExternalCommands();
    }, Qt::DirectConnection);

  // Release GPU resources on the render thread before Qt destroys the device.
  QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window,
    [state]() {
      if (state->bridge) {
        state->bridge->release();
        state->bridge.reset();
      }
      state->attempted = false;
    }, Qt::DirectConnection);
  window->update();
}
