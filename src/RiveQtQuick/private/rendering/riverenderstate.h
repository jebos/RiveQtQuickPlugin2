#pragma once

#include <QByteArray>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QUrl>
#include <QVariant>
#include <QVector>

struct RiveBindingSnapshot {
  QString path;
  QVariant value;
};

inline bool operator==(const RiveBindingSnapshot& lhs, const RiveBindingSnapshot& rhs)
{
  return lhs.path == rhs.path && lhs.value == rhs.value;
}

enum class RiveCommandType {
  Reload,
  SetBoolean,
  SetNumber,
  SetString,
  FireTrigger,
  SetViewModelValue,
  FireViewModelTrigger,
  PointerDown,
  PointerMove,
  PointerUp,
  PointerExit,
};

struct RiveCommand {
  RiveCommandType type { RiveCommandType::Reload };
  QString name;
  QVariant value;
  QPointF point;
  int pointerId { 0 };
};

struct RiveRenderState {
  qint64 sourceRevision { 0 };
  QUrl sourceUrl;
  QByteArray sourceBytes;
  QString artboard;
  int artboardIndex { -1 };
  QString animation;
  QString stateMachine;
  QString viewModel;
  QVector<RiveBindingSnapshot> bindings;
  QVector<RiveCommand> commands;
  QSizeF itemSize;
  int fit { 0 };
  int alignment { 0 };
  bool playing { true };
  bool interactive { true };
  float speed { 1.0f };
};
