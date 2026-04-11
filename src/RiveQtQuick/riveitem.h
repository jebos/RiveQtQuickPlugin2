#pragma once

#include <QAbstractItemModel>
#include <QFutureWatcher>
#include <QMutex>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmllist.h>
#include <QtQuick/QQuickItem>

#include "private/riveinspector.h"
#include "private/rendering/riverenderstate.h"

class QHoverEvent;
class QMouseEvent;
class QTouchEvent;

class RiveBinding;
class RiveRenderNode;

class RiveItem : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
  Q_PROPERTY(QString artboard READ artboard WRITE setArtboard NOTIFY artboardChanged FINAL)
  Q_PROPERTY(int artboardIndex READ artboardIndex WRITE setArtboardIndex NOTIFY artboardIndexChanged FINAL)
  Q_PROPERTY(QString stateMachine READ stateMachine WRITE setStateMachine NOTIFY stateMachineChanged FINAL)
  Q_PROPERTY(QString animation READ animation WRITE setAnimation NOTIFY animationChanged FINAL)
  Q_PROPERTY(QString viewModel READ viewModel WRITE setViewModel NOTIFY viewModelChanged FINAL)
  Q_PROPERTY(QString currentArtboard READ currentArtboard NOTIFY currentArtboardChanged FINAL)
  Q_PROPERTY(QString currentStateMachine READ currentStateMachine NOTIFY currentStateMachineChanged FINAL)
  Q_PROPERTY(QAbstractItemModel* artboardsModel READ artboardsModel CONSTANT FINAL)
  Q_PROPERTY(QAbstractItemModel* stateMachinesModel READ stateMachinesModel CONSTANT FINAL)
  Q_PROPERTY(QAbstractItemModel* inputsModel READ inputsModel CONSTANT FINAL)
  Q_PROPERTY(QAbstractItemModel* eventsModel READ eventsModel CONSTANT FINAL)
  Q_PROPERTY(QQmlListProperty<RiveBinding> bindings READ bindings NOTIFY bindingsChanged FINAL)
  Q_PROPERTY(Fit fit READ fit WRITE setFit NOTIFY fitChanged FINAL)
  Q_PROPERTY(Alignment alignment READ alignment WRITE setAlignment NOTIFY alignmentChanged FINAL)
  Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playingChanged FINAL)
  Q_PROPERTY(qreal speed READ speed WRITE setSpeed NOTIFY speedChanged FINAL)
  Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged FINAL)
  Q_PROPERTY(bool hovered READ isHovered WRITE setHovered NOTIFY hoveredChanged FINAL)
  Q_PROPERTY(Status status READ status NOTIFY statusChanged FINAL)
  Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged FINAL)
  Q_PROPERTY(QStringList availableArtboards READ availableArtboards NOTIFY availableArtboardsChanged FINAL)
  Q_PROPERTY(QStringList availableStateMachines READ availableStateMachines NOTIFY availableStateMachinesChanged FINAL)
  Q_PROPERTY(QStringList availableAnimations READ availableAnimations NOTIFY availableAnimationsChanged FINAL)
  QML_ELEMENT

  public:
  enum class Fit {
    Fill,
    Contain,
    Cover,
    FitWidth,
    FitHeight,
    None,
    ScaleDown,
    Layout,
  };
  Q_ENUM(Fit)

  enum class Alignment {
    Center,
    TopCenter,
    TopRight,
    CenterLeft,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
  };
  Q_ENUM(Alignment)

  enum class Status {
    Null,
    Loading,
    Ready,
    Error,
  };
  Q_ENUM(Status)

  explicit RiveItem(QQuickItem* parent = nullptr);
  ~RiveItem() override;

  QUrl source() const;
  void setSource(const QUrl& source);

  QString artboard() const;
  void setArtboard(const QString& artboard);
  int artboardIndex() const;
  void setArtboardIndex(int artboardIndex);

  QString stateMachine() const;
  void setStateMachine(const QString& stateMachine);

  QString animation() const;
  void setAnimation(const QString& animation);

  QString viewModel() const;
  void setViewModel(const QString& viewModel);
  QString currentArtboard() const;
  QString currentStateMachine() const;
  QAbstractItemModel* artboardsModel() const;
  QAbstractItemModel* stateMachinesModel() const;
  QAbstractItemModel* inputsModel() const;
  QAbstractItemModel* eventsModel() const;

  QQmlListProperty<RiveBinding> bindings();

  Fit fit() const;
  void setFit(Fit fit);

  Alignment alignment() const;
  void setAlignment(Alignment alignment);

  bool isPlaying() const;
  void setPlaying(bool playing);

  qreal speed() const;
  void setSpeed(qreal speed);

  bool isInteractive() const;
  void setInteractive(bool interactive);
  bool isHovered() const;
  void setHovered(bool hovered);

  Status status() const;
  QString errorString() const;
  QStringList availableArtboards() const;
  QStringList availableStateMachines() const;
  QStringList availableAnimations() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void reload();
  Q_INVOKABLE void selectArtboard(const QString& name);
  Q_INVOKABLE void selectStateMachine(const QString& name);
  Q_INVOKABLE void setBoolean(const QString& name, bool value);
  Q_INVOKABLE void setNumber(const QString& name, qreal value);
  Q_INVOKABLE void setString(const QString& name, const QString& value);
  Q_INVOKABLE void fireTrigger(const QString& name);
  Q_INVOKABLE void setViewModelValue(const QString& path, const QVariant& value);
  Q_INVOKABLE void fireViewModelTrigger(const QString& path);
  Q_INVOKABLE void clearEventLog();

  signals:
  void sourceChanged();
  void artboardChanged();
  void artboardIndexChanged();
  void stateMachineChanged();
  void animationChanged();
  void viewModelChanged();
  void currentArtboardChanged();
  void currentStateMachineChanged();
  void bindingsChanged();
  void fitChanged();
  void alignmentChanged();
  void playingChanged();
  void speedChanged();
  void interactiveChanged();
  void hoveredChanged();
  void statusChanged();
  void errorStringChanged();
  void availableArtboardsChanged();
  void availableStateMachinesChanged();
  void availableAnimationsChanged();
  void loaded();
  void riveEvent(const QString& name, const QVariantMap& payload);

  protected:
  QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void hoverMoveEvent(QHoverEvent* event) override;
  void hoverLeaveEvent(QHoverEvent* event) override;
  void touchEvent(QTouchEvent* event) override;

  private:
  friend class RiveRenderNode;

  static void appendBinding(QQmlListProperty<RiveBinding>* list, RiveBinding* binding);
  static qsizetype bindingCount(QQmlListProperty<RiveBinding>* list);
  static RiveBinding* bindingAt(QQmlListProperty<RiveBinding>* list, qsizetype index);
  static void clearBindings(QQmlListProperty<RiveBinding>* list);

  void onBindingMutated();
  void enqueueCommand(const RiveCommand& command);
  void clearDocumentSelection();
  void loadSource();
  QString filePathForUrl(const QUrl& url) const;
  void setStatusInternal(Status status, const QString& errorString = {});
  RiveRenderState takeRenderState();
  void setRuntimeError(const QString& errorString);
  void setRuntimeReady();
  void setRuntimeError(qint64 sourceRevision, const QString& errorString);
  void setRuntimeReady(qint64 sourceRevision);
  void updateRuntimeState(qint64 sourceRevision,
    const QVector<RiveSelectionEntrySnapshot>& artboards,
    const QVector<RiveSelectionEntrySnapshot>& stateMachines,
    const QVector<RiveInputSnapshot>& inputs,
    const QStringList& animations,
    const QString& currentArtboard,
    const QString& currentStateMachine);
  void appendRuntimeEvent(qint64 sourceRevision,
    const RiveEventSnapshot& eventSnapshot);
  void clearRuntimeState();
  void emitRiveEvent(const QString& name, const QVariantMap& payload);

  QUrl m_source;
  QString m_artboard;
  int m_artboardIndex { -1 };
  QString m_stateMachine;
  QString m_animation;
  QString m_viewModel;
  QString m_currentArtboard;
  QString m_currentStateMachine;
  QList<RiveBinding*> m_bindings;
  Fit m_fit { Fit::Contain };
  Alignment m_alignment { Alignment::Center };
  bool m_playing { true };
  qreal m_speed { 1.0 };
  bool m_interactive { true };
  bool m_hovered { false };
  Status m_status { Status::Null };
  QString m_errorString;
  QStringList m_availableArtboards;
  QStringList m_availableStateMachines;
  QStringList m_availableAnimations;
  QByteArray m_sourceBytes;
  qint64 m_sourceRevision { 0 };
  qint64 m_loadRequestId { 0 };
  RiveSelectionListModel* m_artboardsModel { nullptr };
  RiveSelectionListModel* m_stateMachinesModel { nullptr };
  RiveInputListModel* m_inputsModel { nullptr };
  RiveEventListModel* m_eventsModel { nullptr };
  mutable QMutex m_commandMutex;
  QVector<RiveCommand> m_pendingCommands;
  QFutureWatcher<QByteArray> m_sourceWatcher;
};
