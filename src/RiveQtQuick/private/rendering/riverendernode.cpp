#include "riverendernode.h"

#include <mutex>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <QtCore/qscopeguard.h>
#include <QtMath>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRenderNode>
#include <QtQuick/QSGSimpleTextureNode>
#include <rhi/qrhi.h>

#include "../riveitem.h"
#include "rivebackendbridge.h"
#include "rivefileassetloader.h"
#include "rivelogging.h"

#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/component.hpp"
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/custom_property_trigger.hpp"
#include "rive/event.hpp"
#include "rive/event_report.hpp"
#include "rive/renderer.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"

namespace {
quint64 hashSourceBytes(const QByteArray& bytes)
{
  quint64 hash = 1469598103934665603ULL;
  for (unsigned char byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return hash;
}

struct SharedDocumentKey {
  quintptr factoryKey { 0 };
  QString sourceKey;
  quint64 sourceHash { 0 };
  qsizetype sourceSize { 0 };
};

bool operator==(const SharedDocumentKey& lhs, const SharedDocumentKey& rhs)
{
  return lhs.factoryKey == rhs.factoryKey && lhs.sourceKey == rhs.sourceKey && lhs.sourceHash == rhs.sourceHash && lhs.sourceSize == rhs.sourceSize;
}

struct SharedDocumentKeyHasher {
  size_t operator()(const SharedDocumentKey& key) const noexcept
  {
    size_t hash = std::hash<quintptr>()(key.factoryKey);
    hash ^= std::hash<quint64>()(key.sourceHash) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<qsizetype>()(key.sourceSize) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= qHash(key.sourceKey) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
  }
};

std::mutex g_sharedDocumentMutex;

bool isLocalLikeSourceUrl(const QUrl& url)
{
  return url.isLocalFile() || url.scheme().isEmpty() || url.scheme() == "qrc";
}

std::optional<QColor> clearColorFromObject(QObject* object)
{
  if (!object) {
    return std::nullopt;
  }

  const QVariant colorVariant = object->property("color");
  if (colorVariant.isValid() && colorVariant.canConvert<QColor>()) {
    const QColor color = colorVariant.value<QColor>();
    if (color.isValid() && color.alpha() > 0) {
      return color;
    }
  }

  const QVariant backgroundVariant = object->property("background");
  if (backgroundVariant.isValid()) {
    if (QObject* backgroundObject = backgroundVariant.value<QObject*>()) {
      if (auto backgroundColor = clearColorFromObject(backgroundObject)) {
        return backgroundColor;
      }
    }
  }

  return std::nullopt;
}

std::vector<rive::CustomProperty*> scriptedObjectCustomProperties(rive::ScriptedObject* object)
{
  if (!object) {
    return {};
  }

  if constexpr (std::is_base_of_v<rive::CustomPropertyContainer, rive::ScriptedObject>) {
    std::vector<rive::CustomProperty*> properties;
    auto* container = static_cast<const rive::CustomPropertyContainer*>(object);
    for (rive::Component* child : container->containerChildren()) {
      if (!child || !child->is<rive::CustomProperty>()) {
        continue;
      }
      properties.push_back(static_cast<rive::CustomProperty*>(child));
    }
    return properties;
  } else {
    return {};
  }
}

void appendViewModelInputs(const rive::rcp<rive::ViewModelInstance>& viewModelInstance,
  const QString& prefix,
  QVector<RiveInputSnapshot>* inputs,
  std::unordered_set<const rive::ViewModelInstance*>* visited)
{
  if (!viewModelInstance || !inputs || !visited) {
    return;
  }

  if (!visited->insert(viewModelInstance.get()).second) {
    return;
  }

  auto propertyValues = viewModelInstance->propertyValues();
  for (const rive::rcp<rive::ViewModelInstanceValue>& propertyValue : propertyValues) {
    if (!propertyValue) {
      continue;
    }

    QString propertyName = QString::fromStdString(propertyValue->name());
    QString propertyPath = prefix.isEmpty() ? propertyName : prefix + "." + propertyName;

    RiveInputSnapshot snapshot;
    snapshot.name = propertyName;
    snapshot.path = propertyPath;
    snapshot.displayName = propertyPath;
    snapshot.source = "ViewModel";

    if (propertyValue->is<rive::ViewModelInstanceBoolean>()) {
      auto* boolean = static_cast<rive::ViewModelInstanceBoolean*>(propertyValue.get());
      snapshot.kind = "Boolean";
      snapshot.value = boolean->propertyValue();
      inputs->append(snapshot);
      continue;
    }

    if (propertyValue->is<rive::ViewModelInstanceNumber>()) {
      auto* number = static_cast<rive::ViewModelInstanceNumber*>(propertyValue.get());
      snapshot.kind = "Number";
      snapshot.value = number->propertyValue();
      inputs->append(snapshot);
      continue;
    }

    if (propertyValue->is<rive::ViewModelInstanceString>()) {
      auto* string = static_cast<rive::ViewModelInstanceString*>(propertyValue.get());
      snapshot.kind = "String";
      snapshot.value = QString::fromStdString(string->propertyValue());
      inputs->append(snapshot);
      continue;
    }

    if (propertyValue->is<rive::ViewModelInstanceTrigger>()) {
      snapshot.kind = "Trigger";
      inputs->append(snapshot);
      continue;
    }

    if (propertyValue->is<rive::ViewModelInstanceViewModel>()) {
      auto* nested = static_cast<rive::ViewModelInstanceViewModel*>(propertyValue.get());
      appendViewModelInputs(nested->referenceViewModelInstance(),
        propertyPath,
        inputs,
        visited);
    }
  }
}

void appendCustomPropertyInputs(const std::vector<rive::CustomProperty*>& properties,
  QVector<RiveInputSnapshot>* inputs)
{
  if (!inputs) {
    return;
  }

  for (rive::CustomProperty* property : properties) {
    if (!property) {
      continue;
    }

    RiveInputSnapshot snapshot;
    snapshot.name = QString::fromStdString(property->name());
    snapshot.path = snapshot.name;
    snapshot.displayName = snapshot.name;
    snapshot.source = "Script";

    if (property->is<rive::CustomPropertyBoolean>()) {
      auto* boolean = static_cast<rive::CustomPropertyBoolean*>(property);
      snapshot.kind = "Boolean";
      snapshot.value = boolean->propertyValue();
    } else if (property->is<rive::CustomPropertyNumber>()) {
      auto* number = static_cast<rive::CustomPropertyNumber*>(property);
      snapshot.kind = "Number";
      snapshot.value = number->propertyValue();
    } else if (property->is<rive::CustomPropertyString>()) {
      auto* string = static_cast<rive::CustomPropertyString*>(property);
      snapshot.kind = "String";
      snapshot.value = QString::fromStdString(string->propertyValue());
    } else if (property->is<rive::CustomPropertyTrigger>()) {
      snapshot.kind = "Trigger";
    } else {
      continue;
    }

    inputs->append(snapshot);
  }
}

template <typename Callback>
bool visitScriptInputs(rive::StateMachineInstance* stateMachineInstance,
  const QString& inputName,
  Callback&& callback)
{
  if (!stateMachineInstance || !stateMachineInstance->stateMachine()) {
    return false;
  }

  const std::string targetName = inputName.toStdString();
  for (rive::ScriptedObject* definition :
    stateMachineInstance->stateMachine()->scriptedObjects()) {
    if (!definition) {
      continue;
    }

    rive::ScriptedObject* runtimeObject = stateMachineInstance->scriptedObject(definition);
    if (!runtimeObject) {
      continue;
    }

    const auto properties = scriptedObjectCustomProperties(runtimeObject);
    if (properties.empty()) {
      continue;
    }

    for (rive::CustomProperty* property : properties) {
      if (!property || property->name() != targetName) {
        continue;
      }

      if (callback(runtimeObject, property)) {
        return true;
      }
    }
  }

  return false;
}

rive::Fit toRiveFit(int fit)
{
  switch (fit) {
  case 1:
    return rive::Fit::contain;
  case 2:
    return rive::Fit::cover;
  case 3:
    return rive::Fit::fitWidth;
  case 4:
    return rive::Fit::fitHeight;
  case 5:
    return rive::Fit::none;
  case 6:
    return rive::Fit::scaleDown;
  case 7:
    return rive::Fit::layout;
  default:
    return rive::Fit::fill;
  }
}

rive::Alignment toRiveAlignment(int alignment)
{
  switch (alignment) {
  case 1:
    return rive::Alignment::topCenter;
  case 2:
    return rive::Alignment::topRight;
  case 3:
    return rive::Alignment::centerLeft;
  case 4:
    return rive::Alignment::center;
  case 5:
    return rive::Alignment::centerRight;
  case 6:
    return rive::Alignment::bottomLeft;
  case 7:
    return rive::Alignment::bottomCenter;
  case 8:
    return rive::Alignment::bottomRight;
  default:
    return rive::Alignment::center;
  }
}

} // namespace

struct RiveRenderNode::SharedDocument {
  rive::rcp<RiveFileAssetLoader> assetLoader;
  rive::rcp<rive::File> file;
};

namespace {
std::unordered_map<SharedDocumentKey,
  std::weak_ptr<void>,
  SharedDocumentKeyHasher>
  g_sharedDocuments;
}

class RiveRenderNode::CommandNode : public QSGRenderNode {
  public:
  explicit CommandNode(RiveRenderNode* owner)
      : m_owner(owner)
  {
  }

  StateFlags changedStates() const override
  {
    return DepthState | StencilState | ScissorState | ColorState | BlendState | CullState | ViewportState;
  }

  RenderingFlags flags() const override { return BoundedRectRendering; }

  QRectF rect() const override { return m_owner ? m_owner->itemRect() : QRectF(); }

  void render(const RenderState* state) override
  {
    if (m_owner) {
      m_owner->render(state);
    }
  }

  void releaseResources() override
  {
    if (m_owner) {
      m_owner->releaseResources();
    }
  }

  private:
  RiveRenderNode* m_owner = nullptr;
};

RiveRenderNode::RiveRenderNode(RiveItem* item)
    : QObject()
    , m_item(item)
{
  static bool registeredTypes = []() {
    qRegisterMetaType<RiveSelectionEntrySnapshot>();
    qRegisterMetaType<QVector<RiveSelectionEntrySnapshot>>();
    qRegisterMetaType<RiveInputSnapshot>();
    qRegisterMetaType<QVector<RiveInputSnapshot>>();
    qRegisterMetaType<RiveEventSnapshot>();
    return true;
  }();
  Q_UNUSED(registeredTypes);

  m_commandNode = new CommandNode(this);
  appendChildNode(m_commandNode);

  if (m_item) {
    QObject::connect(
      this,
      &RiveRenderNode::runtimeStatePosted,
      m_item,
      &RiveItem::updateRuntimeState,
      Qt::QueuedConnection);
    QObject::connect(
      this,
      &RiveRenderNode::runtimeEventPosted,
      m_item,
      [item = QPointer<RiveItem>(m_item)](qint64 sourceRevision,
        const RiveEventSnapshot& eventSnapshot) {
        if (!item) {
          return;
        }

        item->appendRuntimeEvent(sourceRevision, eventSnapshot);
        item->emitRiveEvent(eventSnapshot.name, eventSnapshot.payload);
      },
      Qt::QueuedConnection);
    QObject::connect(
      this,
      &RiveRenderNode::updateScheduled,
      m_item,
      [item = QPointer<RiveItem>(m_item)](qint64 sourceRevision) {
        if (item && item->m_sourceRevision == sourceRevision) {
          item->update();
        }
      },
      Qt::QueuedConnection);
    QObject::connect(
      this,
      &RiveRenderNode::runtimeReadyPosted,
      m_item,
      static_cast<void (RiveItem::*)(qint64)>(&RiveItem::setRuntimeReady),
      Qt::QueuedConnection);
  }
}

RiveRenderNode::~RiveRenderNode()
{
  releaseResources();
}

void RiveRenderNode::sync(const RiveRenderState& state)
{
  bool sourceChanged = m_loadedRevision >= 0 && m_loadedRevision != state.sourceRevision;
  m_dirtyBindings = m_state.bindings != state.bindings;
  m_dirtySelection = m_loadedRevision != state.sourceRevision || m_state.artboard != state.artboard || m_state.artboardIndex != state.artboardIndex || m_state.animation != state.animation || m_state.stateMachine != state.stateMachine || m_state.viewModel != state.viewModel;
  m_state = state;

  if (m_item) {
    const QPointF origin = m_item->mapToScene(QPointF(0.0, 0.0));
    const QPointF unitX = m_item->mapToScene(QPointF(1.0, 0.0));
    const QPointF unitY = m_item->mapToScene(QPointF(0.0, 1.0));
    const QPointF xAxis = unitX - origin;
    const QPointF yAxis = unitY - origin;
    m_itemToSceneTransform = QTransform(xAxis.x(),
      xAxis.y(),
      yAxis.x(),
      yAxis.y(),
      origin.x(),
      origin.y());
  } else {
    m_itemToSceneTransform.reset();
  }

  if (sourceChanged) {
    releaseResources();
  }

  if (m_textureNode) {
    m_textureNode->setRect(itemRect());
  }

  if (m_commandNode) {
    m_commandNode->markDirty(QSGNode::DirtyMaterial | QSGNode::DirtyForceUpdate);
  }

  if (!m_item || !m_item->window() || m_state.sourceBytes.isEmpty()) {
    releaseResources();
    return;
  }

  if (!ensureBridge() || !ensurePresentationTexture()) {
    clearSceneGraphTexture();
    qCDebug(lcRiveRenderNode) << "Failed to initialize the rendering backend.";
    QPointer<RiveItem> item = m_item;
    qint64 sourceRevision = m_state.sourceRevision;
    QMetaObject::invokeMethod(
      item,
      [item, sourceRevision]() {
        if (item) {
          item->setRuntimeError(sourceRevision,
            "Failed to initialize the rendering backend.");
        }
      },
      Qt::QueuedConnection);
  }
}

void RiveRenderNode::render(const QSGRenderNode::RenderState*)
{
  if (!m_item || !m_item->window()) {
    return;
  }

  if (m_state.sourceBytes.isEmpty()) {
    auto* rendererInterface = m_item->window()->rendererInterface();
    if (rendererInterface &&
        rendererInterface->graphicsApi() == QSGRendererInterface::Software) {
      if (auto* painter = static_cast<QPainter*>(rendererInterface->getResource(
            m_item->window(),
            QSGRendererInterface::PainterResource))) {
        clearSoftwarePresentation(painter);
      }
    }
    return;
  }

  if (!ensureBridge() || !ensurePresentationTexture()) {
    qCDebug(lcRiveRenderNode) << "Failed to initialize the rendering backend.";
    QPointer<RiveItem> item = m_item;
    qint64 sourceRevision = m_state.sourceRevision;
    QMetaObject::invokeMethod(
      item,
      [item, sourceRevision]() {
        if (item) {
          item->setRuntimeError(sourceRevision,
            "Failed to initialize the rendering backend.");
        }
      },
      Qt::QueuedConnection);
    return;
  }

  QRhiCommandBuffer* cb = m_commandNode->commandBuffer();
  QPainter* painter = nullptr;
  bool restorePainterState = false;
  if (m_bridge->targetKind() == RiveBackendBridge::TargetKind::Painter) {
    auto* rendererInterface = m_item->window()->rendererInterface();
    painter = rendererInterface
      ? static_cast<QPainter*>(rendererInterface->getResource(
          m_item->window(),
          QSGRendererInterface::PainterResource))
      : nullptr;
    if (!painter) {
      qCDebug(lcRiveRenderNode) << "Qt did not provide a software painter.";
      QPointer<RiveItem> item = m_item;
      qint64 sourceRevision = m_state.sourceRevision;
      QMetaObject::invokeMethod(
        item,
        [item, sourceRevision]() {
          if (item) {
            item->setRuntimeError(sourceRevision,
              "Qt did not provide a software painter.");
          }
        },
        Qt::QueuedConnection);
      return;
    }

    painter->save();
    restorePainterState = true;
    clearSoftwarePresentation(painter);
    const QRectF sceneRect = m_itemToSceneTransform.mapRect(itemRect());
    painter->setWorldTransform(QTransform(), false);
    painter->setClipRect(sceneRect, Qt::IntersectClip);
    painter->setWorldTransform(m_itemToSceneTransform, false);
    if (m_commandNode) {
      painter->setOpacity(m_commandNode->inheritedOpacity());
    } else {
      painter->setOpacity(1.0);
    }
  } else if (!cb) {
    qCDebug(lcRiveRenderNode) << "Qt did not provide a QRhi command buffer.";
    QPointer<RiveItem> item = m_item;
    qint64 sourceRevision = m_state.sourceRevision;
    QMetaObject::invokeMethod(
      item,
      [item, sourceRevision]() {
        if (item) {
          item->setRuntimeError(sourceRevision,
            "Qt did not provide a QRhi command buffer.");
        }
      },
      Qt::QueuedConnection);
    return;
  }

  auto restorePainter = qScopeGuard([painter, restorePainterState]() {
    if (restorePainterState && painter) {
      painter->restore();
    }
  });

  if (!m_bridge->beginFrame(m_item->window(),
        cb,
        painter,
        m_targetPixelSize,
        m_targetYUp)
    || !ensureDocument()) {
    qCDebug(lcRiveRenderNode) << "Failed to prepare the Rive frame.";
    QPointer<RiveItem> item = m_item;
    qint64 sourceRevision = m_state.sourceRevision;
    QMetaObject::invokeMethod(
      item,
      [item, sourceRevision]() {
        if (item) {
          item->setRuntimeError(sourceRevision,
            "Failed to prepare the Rive frame.");
        }
      },
      Qt::QueuedConnection);
    return;
  }

  applyCommands();
  applyBindings();

  float elapsedSeconds = 0.0f;
  if (m_frameTimer.isValid()) {
    elapsedSeconds = static_cast<float>(m_frameTimer.restart()) / 1000.0f;
  } else {
    m_frameTimer.start();
  }

  float delta = m_state.playing ? elapsedSeconds * m_state.speed : 0.0f;
  bool keepGoing = false;
  if (m_scene) {
    keepGoing = m_scene->advanceAndApply(delta);
    emitSceneEvents();
    QVector<RiveInputSnapshot> inputSnapshots = buildInputSnapshots();
    postRuntimeState(m_state.sourceRevision,
      m_lastArtboards,
      m_lastStateMachines,
      inputSnapshots,
      m_lastAnimations,
      m_lastCurrentArtboard,
      m_lastCurrentStateMachine);
  } else if (m_artboard) {
    m_artboard->advance(0.0f);
  }

  rive::Mat2D transform = itemToTargetTransform() * artboardToItemTransform();
  if (!m_bridge->render(m_artboard.get(), m_scene.get(), transform)) {
    qCDebug(lcRiveRenderNode) << "Failed to render the Rive scene.";
    QPointer<RiveItem> item = m_item;
    qint64 sourceRevision = m_state.sourceRevision;
    QMetaObject::invokeMethod(
      item,
      [item, sourceRevision]() {
        if (item) {
          item->setRuntimeError(sourceRevision,
            "Failed to render the Rive scene.");
        }
      },
        Qt::QueuedConnection);
    return;
  }
  postReady(m_state.sourceRevision);
  if (m_state.playing && keepGoing) {
    scheduleUpdate(m_state.sourceRevision);
  }
}

void RiveRenderNode::clearSoftwarePresentation(QPainter* painter) const
{
  if (!painter) {
    return;
  }

  QColor clearColor = m_item && m_item->window()
    ? m_item->window()->color()
    : QColor(Qt::transparent);
  for (QQuickItem* item = m_item ? m_item->parentItem() : nullptr; item;
    item = item->parentItem()) {
    if (auto ancestorColor = clearColorFromObject(item)) {
      clearColor = *ancestorColor;
      break;
    }
  }

  QPainterPath itemPath;
  itemPath.addRect(itemRect());

  painter->save();
  painter->setWorldTransform(QTransform(), false);
  painter->setOpacity(1.0);
  painter->setCompositionMode(QPainter::CompositionMode_Source);
  const QPainterPath scenePath = m_itemToSceneTransform.map(itemPath);
  painter->setClipPath(scenePath, Qt::IntersectClip);
  painter->fillPath(scenePath, clearColor);
  painter->restore();
}

void RiveRenderNode::releaseResources()
{
  clearSceneGraphTexture();
  m_scene.reset();
  m_artboard.reset();
  m_file = nullptr;
  m_viewModelInstance = nullptr;
  m_sharedDocument.reset();
  if (m_bridge) {
    m_bridge->release();
    m_bridge.reset();
  }
  m_loadedRevision = -1;
  m_dirtySelection = true;
  m_dirtyBindings = true;
  m_frameTimer.invalidate();
  m_lastArtboards.clear();
  m_lastStateMachines.clear();
  m_lastInputs.clear();
  m_lastAnimations.clear();
  m_lastCurrentArtboard.clear();
  m_lastCurrentStateMachine.clear();
  m_lastPostedRevision = -1;
  m_targetPixelSize = {};
  m_targetYUp = false;
}

bool RiveRenderNode::ensureBridge()
{
  auto api = m_item->window()->rendererInterface()->graphicsApi();
  if (!m_bridge || m_bridge->api() != api) {
    clearSceneGraphTexture();
    if (m_bridge) {
      m_bridge->release();
    }
    m_bridge = RiveBackendBridge::create(api);
    if (!m_bridge) {
      return false;
    }
    m_loadedRevision = -1;
    m_dirtySelection = true;
  }

  return true;
}

bool RiveRenderNode::ensurePresentationTexture()
{
  if (!m_bridge || !m_item || !m_item->window()) {
    return false;
  }

  const QSize pixelSize = presentationPixelSize();
  if (pixelSize.isEmpty()) {
    clearSceneGraphTexture();
    return true;
  }

  if (!m_bridge->syncPresentation(m_item->window(), pixelSize)) {
    return false;
  }

  m_targetPixelSize = pixelSize;
  if (m_bridge->targetKind() == RiveBackendBridge::TargetKind::Painter) {
    m_targetYUp = false;
    clearSceneGraphTexture();
    return true;
  }

  m_targetYUp = m_item->window()->rhi() && m_item->window()->rhi()->isYUpInFramebuffer();

  auto* outputTexture = m_bridge->outputTexture();
  if (!outputTexture) {
    clearSceneGraphTexture();
    return true;
  }

  if (m_presentedTexture != outputTexture || !m_texture) {
    clearSceneGraphTexture();
    QQuickWindow::CreateTextureOptions textureOptions = QQuickWindow::TextureHasAlphaChannel;
    if (m_bridge->api() == QSGRendererInterface::Vulkan) {
      textureOptions = QQuickWindow::TextureIsOpaque;
    }
    m_texture = m_item->window()->createTextureFromRhiTexture(
      outputTexture,
      textureOptions);
    if (!m_texture) {
      qCDebug(lcRiveRenderNode).noquote()
        << "failed to create a scenegraph texture for"
        << RiveBackendBridge::graphicsApiName(m_bridge->api());
      return false;
    }

    m_textureNode = new QSGSimpleTextureNode;
    m_textureNode->setOwnsTexture(true);
    m_textureNode->setFiltering(QSGTexture::Linear);
    m_textureNode->setRect(itemRect());
    m_textureNode->setTexture(m_texture);
    appendChildNode(m_textureNode);
    m_presentedTexture = outputTexture;
  }

  return true;
}

bool RiveRenderNode::ensureDocument()
{
  if (m_loadedRevision != m_state.sourceRevision) {
    return rebuildDocument();
  }

  if (m_dirtySelection) {
    return selectArtboardAndScene();
  }

  return true;
}

bool RiveRenderNode::rebuildDocument()
{
  m_scene.reset();
  m_artboard.reset();
  m_viewModelInstance = nullptr;
  m_file = nullptr;
  m_sharedDocument.reset();
  m_staticArtboard = false;

  const SharedDocumentKey cacheKey {
    .factoryKey = reinterpret_cast<quintptr>(m_bridge->factory()),
    .sourceKey = m_state.sourceUrl.toString(),
    .sourceHash = hashSourceBytes(m_state.sourceBytes),
    .sourceSize = m_state.sourceBytes.size(),
  };

  {
    std::lock_guard<std::mutex> lock(g_sharedDocumentMutex);
    auto it = g_sharedDocuments.find(cacheKey);
    if (it != g_sharedDocuments.end()) {
      m_sharedDocument = std::static_pointer_cast<SharedDocument>(it->second.lock());
    }
  }

  if (!m_sharedDocument) {
    QUrl baseUrl = m_state.sourceUrl.adjusted(QUrl::RemoveFilename);
    auto assetLoader = rive::make_rcp<RiveFileAssetLoader>(baseUrl, m_bridge.get());

    rive::ImportResult importResult = rive::ImportResult::success;
    auto file = rive::File::import(
      rive::Span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(m_state.sourceBytes.constData()),
        static_cast<size_t>(m_state.sourceBytes.size())),
      m_bridge->factory(),
      &importResult,
      assetLoader);

    if (!file || importResult != rive::ImportResult::success) {
      qCDebug(lcRiveRenderNode) << "failed to import the Rive document.";
      return false;
    }

    auto sharedDocument = std::make_shared<SharedDocument>();
    sharedDocument->assetLoader = std::move(assetLoader);
    sharedDocument->file = std::move(file);

    {
      std::lock_guard<std::mutex> lock(g_sharedDocumentMutex);
      g_sharedDocuments[cacheKey] = sharedDocument;
    }

    m_sharedDocument = std::move(sharedDocument);
  }

  m_file = m_sharedDocument->file;

  m_loadedRevision = m_state.sourceRevision;
  m_dirtySelection = true;
  m_dirtyBindings = true;
  m_lastPostedRevision = -1;
  return selectArtboardAndScene();
}

bool RiveRenderNode::selectArtboardAndScene()
{
  if (!m_file) {
    return false;
  }

  if (!m_state.animation.isEmpty() && !m_state.stateMachine.isEmpty()) {
    qCDebug(lcRiveRenderNode) << "animation and stateMachine are mutually exclusive.";
    return false;
  }

  m_scene.reset();
  m_artboard.reset();
  m_viewModelInstance = nullptr;
  m_staticArtboard = false;

  if (!m_state.artboard.isEmpty()) {
    m_artboard = m_file->artboardNamed(m_state.artboard.toStdString());
  } else if (m_state.artboardIndex >= 0 && static_cast<size_t>(m_state.artboardIndex) < m_file->artboardCount()) {
    m_artboard = m_file->artboardAt(static_cast<size_t>(m_state.artboardIndex));
  }

  if (!m_artboard) {
    m_artboard = m_file->artboardDefault();
  }

  if (!m_artboard) {
    if (m_file->artboardCount() > 0) {
      m_artboard = m_file->artboardAt(0);
    }
  }

  if (!m_artboard) {
    qCDebug(lcRiveRenderNode) << "requested artboard was not found.";
    return false;
  }

  QStringList artboards;
  QVector<RiveSelectionEntrySnapshot> artboardEntries;
  for (size_t i = 0; i < m_file->artboardCount(); ++i) {
    QString artboardName = QString::fromStdString(m_file->artboardNameAt(i));
    artboards.append(artboardName);
    artboardEntries.append({ .name = artboardName, .current = false });
  }

  QStringList animations;
  for (size_t i = 0; i < m_artboard->animationCount(); ++i) {
    animations.append(QString::fromStdString(m_artboard->animationNameAt(i)));
  }

  QStringList stateMachines;
  QVector<RiveSelectionEntrySnapshot> stateMachineEntries;
  for (size_t i = 0; i < m_artboard->stateMachineCount(); ++i) {
    QString stateMachineName = QString::fromStdString(m_artboard->stateMachineNameAt(i));
    stateMachines.append(stateMachineName);
    stateMachineEntries.append({ .name = stateMachineName, .current = false });
  }

  if (!m_state.stateMachine.isEmpty()) {
    m_scene = m_artboard->stateMachineNamed(m_state.stateMachine.toStdString());
  }
  if (!m_scene && !m_state.animation.isEmpty()) {
    m_scene = m_artboard->animationNamed(m_state.animation.toStdString());
  }
  if (!m_scene) {
    if (auto defaultMachine = m_artboard->defaultStateMachine()) {
      m_scene = std::move(defaultMachine);
    }
  }
  if (!m_scene && m_artboard->stateMachineCount() > 0) {
    m_scene = m_artboard->stateMachineAt(0);
  }
  if (!m_scene && m_artboard->animationCount() > 0) {
    m_scene = m_artboard->animationAt(0);
  }
  if (!m_scene) {
    m_staticArtboard = true;
  }

  if (!m_state.viewModel.isEmpty()) {
    m_viewModelInstance = m_file->createViewModelInstance(m_state.viewModel.toStdString());
  } else {
    m_viewModelInstance = m_file->createDefaultViewModelInstance(m_artboard.get());
    if (!m_viewModelInstance) {
      m_viewModelInstance = m_file->createViewModelInstance(m_artboard.get());
    }
  }

  if (m_viewModelInstance && m_scene) {
    m_scene->bindViewModelInstance(m_viewModelInstance);
  }

  QString currentArtboard = QString::fromStdString(m_artboard->name());
  QString currentStateMachine;
  if (auto* stateMachine = dynamic_cast<rive::StateMachineInstance*>(m_scene.get())) {
    currentStateMachine = QString::fromStdString(stateMachine->name());
  }

  for (RiveSelectionEntrySnapshot& entry : artboardEntries) {
    entry.current = entry.name == currentArtboard;
  }
  for (RiveSelectionEntrySnapshot& entry : stateMachineEntries) {
    entry.current = entry.name == currentStateMachine;
  }

  const QVector<RiveInputSnapshot> inputs = buildInputSnapshots();
  postRuntimeState(m_state.sourceRevision,
    artboardEntries,
    stateMachineEntries,
    inputs,
    animations,
    currentArtboard,
    currentStateMachine);
  m_dirtySelection = false;
  return true;
}

void RiveRenderNode::applyCommands()
{
  bool allowScriptCustomInputs = isLocalLikeSourceUrl(m_state.sourceUrl);
  for (const RiveCommand& command : m_state.commands) {
    switch (command.type) {
    case RiveCommandType::Reload:
      m_loadedRevision = -1;
      break;
    case RiveCommandType::SetBoolean:
      if (m_scene) {
        if (auto* input = m_scene->getBool(command.name.toStdString())) {
          input->value(command.value.toBool());
        } else if (allowScriptCustomInputs && dynamic_cast<rive::StateMachineInstance*>(m_scene.get())) {
          auto* stateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
          visitScriptInputs(
            stateMachine,
            command.name,
            [&](rive::ScriptedObject*, rive::CustomProperty* property) {
              if (!property->is<rive::CustomPropertyBoolean>()) {
                return false;
              }
              auto* boolean = static_cast<rive::CustomPropertyBoolean*>(property);
              boolean->propertyValue(command.value.toBool());
              return true;
            });
        }
      }
      break;
    case RiveCommandType::SetNumber:
      if (m_scene) {
        if (auto* input = m_scene->getNumber(command.name.toStdString())) {
          input->value(static_cast<float>(command.value.toDouble()));
        } else if (allowScriptCustomInputs && dynamic_cast<rive::StateMachineInstance*>(m_scene.get())) {
          auto* stateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
          visitScriptInputs(
            stateMachine,
            command.name,
            [&](rive::ScriptedObject*, rive::CustomProperty* property) {
              if (!property->is<rive::CustomPropertyNumber>()) {
                return false;
              }
              auto* number = static_cast<rive::CustomPropertyNumber*>(property);
              number->propertyValue(static_cast<float>(command.value.toDouble()));
              return true;
            });
        }
      }
      break;
    case RiveCommandType::SetString:
      if (allowScriptCustomInputs && dynamic_cast<rive::StateMachineInstance*>(m_scene.get())) {
        auto* stateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
        visitScriptInputs(
          stateMachine,
          command.name,
          [&](rive::ScriptedObject*, rive::CustomProperty* property) {
            if (!property->is<rive::CustomPropertyString>()) {
              return false;
            }
            auto* string = static_cast<rive::CustomPropertyString*>(property);
            string->propertyValue(command.value.toString().toStdString());
            return true;
          });
      }
      break;
    case RiveCommandType::FireTrigger:
      if (m_scene) {
        if (auto* input = m_scene->getTrigger(command.name.toStdString())) {
          input->fire();
        } else if (allowScriptCustomInputs && dynamic_cast<rive::StateMachineInstance*>(m_scene.get())) {
          auto* stateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
          visitScriptInputs(
            stateMachine,
            command.name,
            [&](rive::ScriptedObject* object, rive::CustomProperty* property) {
              if (!property->is<rive::CustomPropertyTrigger>()) {
                return false;
              }
              object->trigger(command.name.toStdString());
              return true;
            });
        }
      }
      break;
    case RiveCommandType::SetViewModelValue: {
      if (auto* value = resolveViewModelPath(command.name)) {
        if (value->is<rive::ViewModelInstanceNumber>()) {
          auto* number = static_cast<rive::ViewModelInstanceNumber*>(value);
          number->propertyValue(static_cast<float>(command.value.toDouble()));
        } else if (value->is<rive::ViewModelInstanceBoolean>()) {
          auto* boolean = static_cast<rive::ViewModelInstanceBoolean*>(value);
          boolean->propertyValue(command.value.toBool());
        } else if (value->is<rive::ViewModelInstanceString>()) {
          auto* string = static_cast<rive::ViewModelInstanceString*>(value);
          string->propertyValue(command.value.toString().toStdString());
        }
      }
      break;
    }
    case RiveCommandType::FireViewModelTrigger: {
      if (auto* value = resolveViewModelPath(command.name)) {
        if (value->is<rive::ViewModelInstanceTrigger>()) {
          auto* trigger = static_cast<rive::ViewModelInstanceTrigger*>(value);
          trigger->propertyValue(trigger->propertyValue() + 1);
        }
      }
      break;
    }
    case RiveCommandType::PointerDown:
      if (m_scene && m_state.interactive) {
        m_scene->pointerDown(itemPointToArtboard(command.point), command.pointerId);
      }
      break;
    case RiveCommandType::PointerMove:
      if (m_scene && m_state.interactive) {
        m_scene->pointerMove(itemPointToArtboard(command.point),
          0.0f,
          command.pointerId);
      }
      break;
    case RiveCommandType::PointerUp:
      if (m_scene && m_state.interactive) {
        m_scene->pointerUp(itemPointToArtboard(command.point), command.pointerId);
      }
      break;
    case RiveCommandType::PointerExit:
      if (m_scene && m_state.interactive) {
        m_scene->pointerExit(itemPointToArtboard(command.point), command.pointerId);
      }
      break;
    }
  }
}

void RiveRenderNode::applyBindings()
{
  if (!m_dirtyBindings || !m_viewModelInstance) {
    return;
  }

  for (const RiveBindingSnapshot& binding : std::as_const(m_state.bindings)) {
    if (auto* value = resolveViewModelPath(binding.path)) {
      if (value->is<rive::ViewModelInstanceNumber>()) {
        auto* number = static_cast<rive::ViewModelInstanceNumber*>(value);
        number->propertyValue(static_cast<float>(binding.value.toDouble()));
      } else if (value->is<rive::ViewModelInstanceBoolean>()) {
        auto* boolean = static_cast<rive::ViewModelInstanceBoolean*>(value);
        boolean->propertyValue(binding.value.toBool());
      } else if (value->is<rive::ViewModelInstanceString>()) {
        auto* string = static_cast<rive::ViewModelInstanceString*>(value);
        string->propertyValue(binding.value.toString().toStdString());
      }
    }
  }

  m_dirtyBindings = false;
}

void RiveRenderNode::emitSceneEvents()
{
  auto* stateMachine = dynamic_cast<rive::StateMachineInstance*>(m_scene.get());
  if (!stateMachine) {
    return;
  }

  for (size_t i = 0; i < stateMachine->reportedEventCount(); ++i) {
    auto report = stateMachine->reportedEventAt(i);
    QVariantMap payload;
    payload.insert("delaySeconds", report.secondsDelay());
    QString name = "event";
    if (report.event()) {
      name = QString::fromStdString(report.event()->name());
    }
    const RiveEventSnapshot snapshot {
      .name = name,
      .payload = payload,
      .timestamp = QDateTime::currentMSecsSinceEpoch(),
      .sourceStateMachine = QString::fromStdString(stateMachine->name()),
    };
    emit runtimeEventPosted(m_state.sourceRevision, snapshot);
  }
}

QVector<RiveInputSnapshot> RiveRenderNode::buildInputSnapshots() const
{
  QVector<RiveInputSnapshot> inputs;
  bool allowExtendedInspector = isLocalLikeSourceUrl(m_state.sourceUrl);
  auto* stateMachine = dynamic_cast<rive::StateMachineInstance*>(m_scene.get());
  if (stateMachine) {
    inputs.reserve(static_cast<qsizetype>(stateMachine->inputCount()));
    for (size_t i = 0; i < stateMachine->inputCount(); ++i) {
      auto* input = stateMachine->input(i);
      if (!input) {
        continue;
      }

      RiveInputSnapshot snapshot;
      snapshot.name = QString::fromStdString(input->name());
      snapshot.path = snapshot.name;
      snapshot.displayName = snapshot.name;
      snapshot.source = "StateMachine";

      if (auto* boolean = dynamic_cast<rive::SMIBool*>(input)) {
        snapshot.kind = "Boolean";
        snapshot.value = boolean->value();
      } else if (auto* number = dynamic_cast<rive::SMINumber*>(input)) {
        snapshot.kind = "Number";
        snapshot.value = number->value();
      } else if (dynamic_cast<rive::SMITrigger*>(input)) {
        snapshot.kind = "Trigger";
      } else {
        snapshot.kind = "Unknown";
      }

      inputs.append(snapshot);
    }

    if (allowExtendedInspector) {
      for (rive::ScriptedObject* definition :
        stateMachine->stateMachine()->scriptedObjects()) {
        if (!definition) {
          continue;
        }

        if (rive::ScriptedObject* runtimeObject = stateMachine->scriptedObject(definition)) {
          const auto properties = scriptedObjectCustomProperties(runtimeObject);
          if (!properties.empty()) {
            appendCustomPropertyInputs(properties,
              &inputs);
          }
        }
      }
    }
  }

  if (allowExtendedInspector) {
    std::unordered_set<const rive::ViewModelInstance*> visitedViewModels;
    appendViewModelInputs(m_viewModelInstance,
      QString(),
      &inputs,
      &visitedViewModels);
  }

  return inputs;
}

void RiveRenderNode::postRuntimeState(
  qint64 sourceRevision,
  const QVector<RiveSelectionEntrySnapshot>& artboards,
  const QVector<RiveSelectionEntrySnapshot>& stateMachines,
  const QVector<RiveInputSnapshot>& inputs,
  const QStringList& animations,
  const QString& currentArtboard,
  const QString& currentStateMachine)
{
  if (m_lastPostedRevision == sourceRevision && m_lastArtboards == artboards && m_lastStateMachines == stateMachines && m_lastInputs == inputs && m_lastAnimations == animations && m_lastCurrentArtboard == currentArtboard && m_lastCurrentStateMachine == currentStateMachine) {
    return;
  }

  m_lastArtboards = artboards;
  m_lastStateMachines = stateMachines;
  m_lastInputs = inputs;
  m_lastAnimations = animations;
  m_lastCurrentArtboard = currentArtboard;
  m_lastCurrentStateMachine = currentStateMachine;
  m_lastPostedRevision = sourceRevision;

  emit runtimeStatePosted(sourceRevision,
    artboards,
    stateMachines,
    inputs,
    animations,
    currentArtboard,
    currentStateMachine);
}

rive::ViewModelInstanceValue* RiveRenderNode::resolveViewModelPath(const QString& path) const
{
  if (!m_viewModelInstance || path.isEmpty()) {
    return nullptr;
  }

  rive::ViewModelInstance* current = m_viewModelInstance.get();
  const QStringList segments = path.split(u'.', Qt::SkipEmptyParts);
  if (segments.isEmpty()) {
    return nullptr;
  }

  for (int i = 0; i < segments.size(); ++i) {
    rive::ViewModelInstanceValue* value = current->propertyValue(segments.at(i).toStdString());
    if (!value) {
      return nullptr;
    }

    if (i == segments.size() - 1) {
      return value;
    }

    if (!value->is<rive::ViewModelInstanceViewModel>()) {
      return nullptr;
    }
    auto* nested = static_cast<rive::ViewModelInstanceViewModel*>(value);
    if (!nested->referenceViewModelInstance()) {
      return nullptr;
    }
    current = nested->referenceViewModelInstance().get();
  }

  return nullptr;
}

rive::Mat2D RiveRenderNode::artboardToItemTransform() const
{
  if (!m_artboard) {
    return {};
  }

  const rive::AABB frame(0.0f,
    0.0f,
    static_cast<float>(m_state.itemSize.width()),
    static_cast<float>(m_state.itemSize.height()));
  const rive::AABB content = m_artboard->bounds();
  return rive::computeAlignment(toRiveFit(m_state.fit),
    toRiveAlignment(m_state.alignment),
    frame,
    content);
}

rive::Mat2D RiveRenderNode::itemToTargetTransform() const
{
  if (m_bridge && m_bridge->targetKind() == RiveBackendBridge::TargetKind::Painter) {
    return {};
  }

  if (m_targetPixelSize.isEmpty() || m_state.itemSize.isEmpty()) {
    return {};
  }

  const float sx = static_cast<float>(m_targetPixelSize.width()) / static_cast<float>(m_state.itemSize.width());
  const float sy = static_cast<float>(m_targetPixelSize.height()) / static_cast<float>(m_state.itemSize.height());

  if (m_targetYUp) {
    return rive::Mat2D(sx,
      0.0f,
      0.0f,
      -sy,
      0.0f,
      static_cast<float>(m_targetPixelSize.height()));
  }

  return rive::Mat2D(sx, 0.0f, 0.0f, sy, 0.0f, 0.0f);
}

rive::Vec2D RiveRenderNode::itemPointToArtboard(const QPointF& point) const
{
  const rive::Mat2D inverse = artboardToItemTransform().invertOrIdentity();
  return inverse * rive::Vec2D(static_cast<float>(point.x()), static_cast<float>(point.y()));
}

QSize RiveRenderNode::presentationPixelSize() const
{
  if (!m_item || !m_item->window() || m_state.itemSize.isEmpty()) {
    return {};
  }

  const qreal dpr = m_item->window()->effectiveDevicePixelRatio();
  int width = qCeil(m_state.itemSize.width() * dpr);
  int height = qCeil(m_state.itemSize.height() * dpr);
  if (width <= 0 || height <= 0) {
    return {};
  }

  return QSize(width, height);
}

QRectF RiveRenderNode::itemRect() const
{
  return QRectF(QPointF(0.0, 0.0), m_state.itemSize);
}

void RiveRenderNode::clearSceneGraphTexture()
{
  if (m_textureNode) {
    if (m_textureNode->parent()) {
      removeChildNode(m_textureNode);
    }
    delete m_textureNode;
    m_textureNode = nullptr;
  }
  m_texture = nullptr;
  m_presentedTexture = nullptr;
}

void RiveRenderNode::scheduleUpdate(qint64 sourceRevision)
{
  emit updateScheduled(sourceRevision);
}

void RiveRenderNode::postReady(qint64 sourceRevision)
{
  emit runtimeReadyPosted(sourceRevision);
}
