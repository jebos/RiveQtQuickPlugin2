#include "riveitem.h"

#include <QFile>
#include <QFutureWatcher>
#include <QHoverEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QtConcurrent/QtConcurrentRun>
#include <QtQuick/QQuickWindow>

#include "private/rivelogging.h"
#include "private/riverendernode.h"
#include "rivebinding.h"

namespace {
QByteArray readSourceBytes(const QString& filePath)
{
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }

  return file.readAll();
}
}

RiveItem::RiveItem(QQuickItem* parent)
    : QQuickItem(parent)
{
  setFlag(ItemHasContents);
  setAcceptedMouseButtons(Qt::AllButtons);
  setAcceptHoverEvents(true);
  setAcceptTouchEvents(true);

  m_artboardsModel = new RiveSelectionListModel(this);
  m_stateMachinesModel = new RiveSelectionListModel(this);
  m_inputsModel = new RiveInputListModel(this);
  m_eventsModel = new RiveEventListModel(this);

  connect(&m_sourceWatcher, &QFutureWatcher<QByteArray>::finished, this, [this]() {
    const qint64 requestId = m_sourceWatcher.property("requestId").toLongLong();
    const QUrl requestedSource = m_sourceWatcher.property("sourceUrl").toUrl();
    if (requestId != m_loadRequestId || m_source != requestedSource) {
      return;
    }

    const QByteArray bytes = m_sourceWatcher.result();
    if (bytes.isEmpty()) {
      qCDebug(lcRiveItem) << "failed to read local source" << requestedSource;
      setStatusInternal(Status::Error, "Failed to load the Rive source.");
      return;
    }

    m_sourceBytes = bytes;
    ++m_sourceRevision;
    update();
    if (window()) {
      window()->update();
    }
  });
}

RiveItem::~RiveItem() = default;

QUrl RiveItem::source() const
{
  return m_source;
}

void RiveItem::setSource(const QUrl& source)
{
  if (m_source == source) {
    return;
  }

  m_source = source;
  emit sourceChanged();
  clearDocumentSelection();
  loadSource();
}

QString RiveItem::artboard() const
{
  return m_artboard;
}

void RiveItem::setArtboard(const QString& artboard)
{
  if (m_artboard == artboard && m_artboardIndex < 0) {
    return;
  }

  if (m_artboardIndex >= 0) {
    m_artboardIndex = -1;
    emit artboardIndexChanged();
  }

  m_artboard = artboard;
  emit artboardChanged();
  update();
}

int RiveItem::artboardIndex() const
{
  return m_artboardIndex;
}

void RiveItem::setArtboardIndex(int artboardIndex)
{
  if (artboardIndex < 0) {
    artboardIndex = -1;
  }

  if (m_artboardIndex == artboardIndex && m_artboard.isEmpty()) {
    return;
  }

  if (!m_artboard.isEmpty()) {
    m_artboard.clear();
    emit artboardChanged();
  }

  m_artboardIndex = artboardIndex;
  emit artboardIndexChanged();
  update();
}

QString RiveItem::stateMachine() const
{
  return m_stateMachine;
}

void RiveItem::setStateMachine(const QString& stateMachine)
{
  const bool clearAnimation = !stateMachine.isEmpty() && !m_animation.isEmpty();
  if (m_stateMachine == stateMachine && !clearAnimation) {
    return;
  }

  if (clearAnimation) {
    m_animation.clear();
    emit animationChanged();
  }

  m_stateMachine = stateMachine;
  emit stateMachineChanged();
  update();
  if (window()) {
    window()->update();
  }
}

QString RiveItem::animation() const
{
  return m_animation;
}

void RiveItem::setAnimation(const QString& animation)
{
  const bool clearStateMachine = !animation.isEmpty() && !m_stateMachine.isEmpty();
  if (m_animation == animation && !clearStateMachine) {
    return;
  }

  if (clearStateMachine) {
    m_stateMachine.clear();
    emit stateMachineChanged();
  }

  m_animation = animation;
  emit animationChanged();
  update();
  if (window()) {
    window()->update();
  }
}

QString RiveItem::viewModel() const
{
  return m_viewModel;
}

void RiveItem::setViewModel(const QString& viewModel)
{
  if (m_viewModel == viewModel) {
    return;
  }
  m_viewModel = viewModel;
  emit viewModelChanged();
  update();
}

QString RiveItem::currentArtboard() const
{
  return m_currentArtboard;
}

QString RiveItem::currentStateMachine() const
{
  return m_currentStateMachine;
}

QAbstractItemModel* RiveItem::artboardsModel() const
{
  return m_artboardsModel;
}

QAbstractItemModel* RiveItem::stateMachinesModel() const
{
  return m_stateMachinesModel;
}

QAbstractItemModel* RiveItem::inputsModel() const
{
  return m_inputsModel;
}

QAbstractItemModel* RiveItem::eventsModel() const
{
  return m_eventsModel;
}

QQmlListProperty<RiveBinding> RiveItem::bindings()
{
  return QQmlListProperty<RiveBinding>(this, this, &RiveItem::appendBinding,
    &RiveItem::bindingCount, &RiveItem::bindingAt,
    &RiveItem::clearBindings);
}

RiveItem::Fit RiveItem::fit() const
{
  return m_fit;
}

void RiveItem::setFit(Fit fit)
{
  if (m_fit == fit) {
    return;
  }
  m_fit = fit;
  emit fitChanged();
  update();
}

RiveItem::Alignment RiveItem::alignment() const
{
  return m_alignment;
}

void RiveItem::setAlignment(Alignment alignment)
{
  if (m_alignment == alignment) {
    return;
  }
  m_alignment = alignment;
  emit alignmentChanged();
  update();
}

bool RiveItem::isPlaying() const
{
  return m_playing;
}

void RiveItem::setPlaying(bool playing)
{
  if (m_playing == playing) {
    return;
  }
  m_playing = playing;
  emit playingChanged();
  update();
}

qreal RiveItem::speed() const
{
  return m_speed;
}

void RiveItem::setSpeed(qreal speed)
{
  if (qFuzzyCompare(m_speed, speed)) {
    return;
  }
  m_speed = speed;
  emit speedChanged();
  update();
}

bool RiveItem::isInteractive() const
{
  return m_interactive;
}

void RiveItem::setInteractive(bool interactive)
{
  if (m_interactive == interactive) {
    return;
  }
  m_interactive = interactive;
  emit interactiveChanged();
  update();
}

bool RiveItem::isHovered() const
{
  return m_hovered;
}

void RiveItem::setHovered(bool hovered)
{
  if (m_hovered == hovered) {
    return;
  }

  m_hovered = hovered;
  emit hoveredChanged();

  if (!m_interactive) {
    return;
  }

  if (hovered) {
    enqueueCommand({ .type = RiveCommandType::PointerMove,
      .point = QPointF(width() * 0.5, height() * 0.5) });
  } else {
    enqueueCommand({ .type = RiveCommandType::PointerExit,
      .point = QPointF(width() * 0.5, height() * 0.5) });
  }
}

RiveItem::Status RiveItem::status() const
{
  return m_status;
}

QString RiveItem::errorString() const
{
  return m_errorString;
}

QStringList RiveItem::availableArtboards() const
{
  return m_availableArtboards;
}

QStringList RiveItem::availableStateMachines() const
{
  return m_availableStateMachines;
}

QStringList RiveItem::availableAnimations() const
{
  return m_availableAnimations;
}

void RiveItem::play()
{
  setPlaying(true);
}

void RiveItem::pause()
{
  setPlaying(false);
}

void RiveItem::reload()
{
  enqueueCommand({ .type = RiveCommandType::Reload });
  loadSource();
}

void RiveItem::selectArtboard(const QString& name)
{
  setArtboard(name);
}

void RiveItem::selectStateMachine(const QString& name)
{
  setStateMachine(name);
}

void RiveItem::setBoolean(const QString& name, bool value)
{
  enqueueCommand({ .type = RiveCommandType::SetBoolean, .name = name, .value = value });
}

void RiveItem::setNumber(const QString& name, qreal value)
{
  enqueueCommand({ .type = RiveCommandType::SetNumber, .name = name, .value = value });
}

void RiveItem::setString(const QString& name, const QString& value)
{
  enqueueCommand({ .type = RiveCommandType::SetString, .name = name, .value = value });
}

void RiveItem::fireTrigger(const QString& name)
{
  enqueueCommand({ .type = RiveCommandType::FireTrigger, .name = name });
}

void RiveItem::setViewModelValue(const QString& path, const QVariant& value)
{
  enqueueCommand({ .type = RiveCommandType::SetViewModelValue, .name = path, .value = value });
}

void RiveItem::fireViewModelTrigger(const QString& path)
{
  enqueueCommand({ .type = RiveCommandType::FireViewModelTrigger, .name = path });
}

void RiveItem::clearEventLog()
{
  m_eventsModel->clear();
}

QSGNode* RiveItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
  auto* node = static_cast<RiveRenderNode*>(oldNode);
  if (!node && m_sourceBytes.isEmpty()) {
    return nullptr;
  }

  if (!node) {
    node = new RiveRenderNode(this);
  }

  node->sync(takeRenderState());
  return node;
}

void RiveItem::mousePressEvent(QMouseEvent* event)
{
  enqueueCommand({ .type = RiveCommandType::PointerDown, .point = event->position() });
  event->accept();
}

void RiveItem::mouseMoveEvent(QMouseEvent* event)
{
  enqueueCommand({ .type = RiveCommandType::PointerMove, .point = event->position() });
  event->accept();
}

void RiveItem::mouseReleaseEvent(QMouseEvent* event)
{
  enqueueCommand({ .type = RiveCommandType::PointerUp, .point = event->position() });
  event->accept();
}

void RiveItem::hoverMoveEvent(QHoverEvent* event)
{
  enqueueCommand({ .type = RiveCommandType::PointerMove, .point = event->position() });
  event->accept();
}

void RiveItem::hoverLeaveEvent(QHoverEvent* event)
{
  Q_UNUSED(event);
  enqueueCommand({ .type = RiveCommandType::PointerExit, .point = QPointF(width() * 0.5, height() * 0.5) });
}

void RiveItem::touchEvent(QTouchEvent* event)
{
  for (const QEventPoint& point : event->points()) {
    RiveCommand command;
    command.point = point.position();
    command.pointerId = point.id();

    switch (point.state()) {
    case QEventPoint::State::Pressed:
      command.type = RiveCommandType::PointerDown;
      break;
    case QEventPoint::State::Released:
      command.type = RiveCommandType::PointerUp;
      break;
    default:
      command.type = RiveCommandType::PointerMove;
      break;
    }

    enqueueCommand(command);
  }
  event->accept();
}

void RiveItem::appendBinding(QQmlListProperty<RiveBinding>* list, RiveBinding* binding)
{
  auto* item = static_cast<RiveItem*>(list->data);
  if (!binding) {
    return;
  }

  item->m_bindings.append(binding);
  binding->setParent(item);
  QObject::connect(binding, &RiveBinding::pathChanged, item, &RiveItem::onBindingMutated);
  QObject::connect(binding, &RiveBinding::valueChanged, item, &RiveItem::onBindingMutated);
  QObject::connect(binding, &QObject::destroyed, item, &RiveItem::onBindingMutated);
  emit item->bindingsChanged();
  item->update();
}

qsizetype RiveItem::bindingCount(QQmlListProperty<RiveBinding>* list)
{
  return static_cast<RiveItem*>(list->data)->m_bindings.count();
}

RiveBinding* RiveItem::bindingAt(QQmlListProperty<RiveBinding>* list, qsizetype index)
{
  return static_cast<RiveItem*>(list->data)->m_bindings.value(index);
}

void RiveItem::clearBindings(QQmlListProperty<RiveBinding>* list)
{
  auto* item = static_cast<RiveItem*>(list->data);
  item->m_bindings.clear();
  emit item->bindingsChanged();
  item->update();
}

void RiveItem::onBindingMutated()
{
  emit bindingsChanged();
  update();
}

void RiveItem::enqueueCommand(const RiveCommand& command)
{
  {
    QMutexLocker locker(&m_commandMutex);
    m_pendingCommands.append(command);
  }
  update();
}

void RiveItem::clearDocumentSelection()
{
  if (!m_artboard.isEmpty()) {
    m_artboard.clear();
    emit artboardChanged();
  }
  if (m_artboardIndex >= 0) {
    m_artboardIndex = -1;
    emit artboardIndexChanged();
  }
  if (!m_stateMachine.isEmpty()) {
    m_stateMachine.clear();
    emit stateMachineChanged();
  }
  if (!m_animation.isEmpty()) {
    m_animation.clear();
    emit animationChanged();
  }
  if (!m_viewModel.isEmpty()) {
    m_viewModel.clear();
    emit viewModelChanged();
  }
}

void RiveItem::loadSource()
{
  ++m_loadRequestId;

  if (!m_source.isValid() || m_source.isEmpty()) {
    m_sourceBytes.clear();
    ++m_sourceRevision;
    clearRuntimeState();
    setStatusInternal(Status::Null);
    update();
    if (window()) {
      window()->update();
    }
    return;
  }

  clearRuntimeState();
  setStatusInternal(Status::Loading);
  m_sourceBytes.clear();
  ++m_sourceRevision;
  update();
  if (window()) {
    window()->update();
  }

  if (m_source.isLocalFile() || m_source.scheme().isEmpty() || m_source.scheme() == "qrc") {
    QUrl requestedSource = m_source;
    const QString filePath = filePathForUrl(requestedSource);
    m_sourceWatcher.setProperty("requestId", m_loadRequestId);
    m_sourceWatcher.setProperty("sourceUrl", requestedSource);
    m_sourceWatcher.setFuture(QtConcurrent::run(readSourceBytes, filePath));
    return;
  }

  qCDebug(lcRiveItem) << "only local Rive sources are supported" << m_source;
  setStatusInternal(Status::Error, "Only local Rive files are supported.");
}

QString RiveItem::filePathForUrl(const QUrl& url) const
{
  if (url.scheme() == "qrc") {
    return ":" + url.path();
  }
  if (url.isLocalFile()) {
    return url.toLocalFile();
  }
  return url.toString();
}

void RiveItem::setStatusInternal(Status status, const QString& errorString)
{
  bool statusChangedNow = m_status != status;
  bool errorChangedNow = m_errorString != errorString;

  m_status = status;
  m_errorString = errorString;

  if (statusChangedNow) {
    emit statusChanged();
  }
  if (errorChangedNow) {
    emit errorStringChanged();
  }
}

RiveRenderState RiveItem::takeRenderState()
{
  RiveRenderState state;
  state.sourceRevision = m_sourceRevision;
  state.sourceUrl = m_source;
  state.sourceBytes = m_sourceBytes;
  state.artboard = m_artboard;
  state.artboardIndex = m_artboardIndex;
  state.animation = m_animation;
  state.stateMachine = m_stateMachine;
  state.viewModel = m_viewModel;
  state.itemSize = QSizeF(width(), height());
  state.fit = static_cast<int>(m_fit);
  state.alignment = static_cast<int>(m_alignment);
  state.playing = m_playing;
  state.interactive = m_interactive;
  state.speed = static_cast<float>(m_speed);

  for (RiveBinding* binding : std::as_const(m_bindings)) {
    if (!binding) {
      continue;
    }
    state.bindings.append({ binding->path(), binding->value() });
  }

  QMutexLocker locker(&m_commandMutex);
  state.commands = std::move(m_pendingCommands);
  m_pendingCommands.clear();
  return state;
}

void RiveItem::setRuntimeError(const QString& errorString)
{
  setStatusInternal(Status::Error, errorString);
}

void RiveItem::setRuntimeReady()
{
  bool shouldEmitLoaded = m_status != Status::Ready;
  setStatusInternal(Status::Ready);
  if (shouldEmitLoaded) {
    emit loaded();
  }
}

void RiveItem::setRuntimeError(qint64 sourceRevision, const QString& errorString)
{
  if (sourceRevision != m_sourceRevision) {
    return;
  }
  setRuntimeError(errorString);
}

void RiveItem::setRuntimeReady(qint64 sourceRevision)
{
  if (sourceRevision != m_sourceRevision) {
    return;
  }
  setRuntimeReady();
}

void RiveItem::updateRuntimeState(
  qint64 sourceRevision,
  const QVector<RiveSelectionEntrySnapshot>& artboards,
  const QVector<RiveSelectionEntrySnapshot>& stateMachines,
  const QVector<RiveInputSnapshot>& inputs,
  const QStringList& animations,
  const QString& currentArtboard,
  const QString& currentStateMachine)
{
  if (sourceRevision != m_sourceRevision) {
    return;
  }

  QStringList artboardNames;
  artboardNames.reserve(artboards.size());
  for (const RiveSelectionEntrySnapshot& artboard : artboards) {
    artboardNames.append(artboard.name);
  }

  QStringList stateMachineNames;
  stateMachineNames.reserve(stateMachines.size());
  for (const RiveSelectionEntrySnapshot& stateMachine : stateMachines) {
    stateMachineNames.append(stateMachine.name);
  }

  m_artboardsModel->setEntries(artboards);
  m_stateMachinesModel->setEntries(stateMachines);
  m_inputsModel->setEntries(inputs);

  if (m_availableArtboards != artboardNames) {
    m_availableArtboards = artboardNames;
    emit availableArtboardsChanged();
  }

  if (m_availableAnimations != animations) {
    m_availableAnimations = animations;
    emit availableAnimationsChanged();
  }

  if (m_availableStateMachines != stateMachineNames) {
    m_availableStateMachines = stateMachineNames;
    emit availableStateMachinesChanged();
  }

  if (m_currentArtboard != currentArtboard) {
    m_currentArtboard = currentArtboard;
    emit currentArtboardChanged();
  }

  if (m_currentStateMachine != currentStateMachine) {
    m_currentStateMachine = currentStateMachine;
    emit currentStateMachineChanged();
  }
}

void RiveItem::appendRuntimeEvent(qint64 sourceRevision,
  const RiveEventSnapshot& eventSnapshot)
{
  if (sourceRevision != m_sourceRevision) {
    return;
  }
  m_eventsModel->appendEntry(eventSnapshot);
}

void RiveItem::clearRuntimeState()
{
  m_artboardsModel->clear();
  m_stateMachinesModel->clear();
  m_inputsModel->clear();
  m_eventsModel->clear();

  if (!m_currentArtboard.isEmpty()) {
    m_currentArtboard.clear();
    emit currentArtboardChanged();
  }

  if (!m_currentStateMachine.isEmpty()) {
    m_currentStateMachine.clear();
    emit currentStateMachineChanged();
  }

  if (!m_availableArtboards.isEmpty()) {
    m_availableArtboards.clear();
    emit availableArtboardsChanged();
  }

  if (!m_availableStateMachines.isEmpty()) {
    m_availableStateMachines.clear();
    emit availableStateMachinesChanged();
  }

  if (!m_availableAnimations.isEmpty()) {
    m_availableAnimations.clear();
    emit availableAnimationsChanged();
  }
}

void RiveItem::emitRiveEvent(const QString& name, const QVariantMap& payload)
{
  emit riveEvent(name, payload);
}
