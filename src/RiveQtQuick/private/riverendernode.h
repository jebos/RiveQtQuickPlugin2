#pragma once

#include <memory>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QSGNode>
#include <QSGRenderNode>
#include <QTransform>

#include "riveinspector.h"
#include "riverenderstate.h"

#include "rive/file.hpp"
#include "rive/refcnt.hpp"

class RiveBackendBridge;
class RiveFileAssetLoader;
class RiveItem;
class QPainter;
class QSGSimpleTextureNode;
class QSGTexture;
class QRhiTexture;

namespace rive {
class ArtboardInstance;
class Scene;
class StateMachineInstance;
class ViewModelInstance;
class ViewModelInstanceValue;
} // namespace rive

class RiveRenderNode : public QObject, public QSGNode {
  Q_OBJECT

  public:
  explicit RiveRenderNode(RiveItem* item);
  ~RiveRenderNode() override;

  void sync(const RiveRenderState& state);

  signals:
  void runtimeStatePosted(qint64 sourceRevision,
    QVector<RiveSelectionEntrySnapshot> artboards,
    QVector<RiveSelectionEntrySnapshot> stateMachines,
    QVector<RiveInputSnapshot> inputs,
    QStringList animations,
    QString currentArtboard,
    QString currentStateMachine);
  void runtimeEventPosted(qint64 sourceRevision,
    RiveEventSnapshot eventSnapshot);
  void updateScheduled(qint64 sourceRevision);
  void runtimeReadyPosted(qint64 sourceRevision);

  private:
  class CommandNode;
  struct SharedDocument;

  bool ensureBridge();
  bool ensurePresentationTexture();
  bool ensureDocument();
  bool rebuildDocument();
  bool selectArtboardAndScene();
  void applyCommands();
  void applyBindings();
  QVector<RiveInputSnapshot> buildInputSnapshots() const;
  void postRuntimeState(qint64 sourceRevision,
    const QVector<RiveSelectionEntrySnapshot>& artboards,
    const QVector<RiveSelectionEntrySnapshot>& stateMachines,
    const QVector<RiveInputSnapshot>& inputs,
    const QStringList& animations,
    const QString& currentArtboard,
    const QString& currentStateMachine);
  void emitSceneEvents();
  rive::ViewModelInstanceValue* resolveViewModelPath(const QString& path) const;
  rive::Mat2D artboardToItemTransform() const;
  rive::Mat2D itemToTargetTransform() const;
  rive::Vec2D itemPointToArtboard(const QPointF& point) const;
  QSize presentationPixelSize() const;
  QRectF itemRect() const;
  void clearSoftwarePresentation(QPainter* painter) const;
  void render(const QSGRenderNode::RenderState* state);
  void releaseResources();
  void clearSceneGraphTexture();
  void scheduleUpdate(qint64 sourceRevision);
  void postReady(qint64 sourceRevision);

  QPointer<RiveItem> m_item;
  CommandNode* m_commandNode { nullptr };
  QSGTexture* m_texture { nullptr };
  QSGSimpleTextureNode* m_textureNode { nullptr };
  QRhiTexture* m_presentedTexture { nullptr };
  RiveRenderState m_state;
  std::unique_ptr<RiveBackendBridge> m_bridge;
  std::shared_ptr<SharedDocument> m_sharedDocument;
  rive::rcp<rive::File> m_file;
  std::unique_ptr<rive::ArtboardInstance> m_artboard;
  std::unique_ptr<rive::Scene> m_scene;
  rive::rcp<rive::ViewModelInstance> m_viewModelInstance;
  qint64 m_loadedRevision { -1 };
  bool m_staticArtboard { false };
  bool m_dirtySelection { true };
  bool m_dirtyBindings { true };
  QElapsedTimer m_frameTimer;
  QVector<RiveSelectionEntrySnapshot> m_lastArtboards;
  QVector<RiveSelectionEntrySnapshot> m_lastStateMachines;
  QVector<RiveInputSnapshot> m_lastInputs;
  QStringList m_lastAnimations;
  QString m_lastCurrentArtboard;
  QString m_lastCurrentStateMachine;
  qint64 m_lastPostedRevision { -1 };
  QSize m_targetPixelSize;
  bool m_targetYUp { false };
  QTransform m_itemToSceneTransform;
};
