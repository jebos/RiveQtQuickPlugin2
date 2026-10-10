#pragma once

#include <QtCore/qglobal.h>

#ifdef RiveQtQuick_EXPORTS
#define RIVEQTQUICK_EXPORT Q_DECL_EXPORT
#else
#define RIVEQTQUICK_EXPORT Q_DECL_IMPORT
#endif

class QQuickWindow;

namespace RiveQtQuick {
RIVEQTQUICK_EXPORT void preload(QQuickWindow* window);
}
