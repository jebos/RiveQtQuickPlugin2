#include "exampleappsupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

namespace {

QString normalizedResourcePath(const QString& relativePath)
{
    QString path = relativePath;
    while (path.startsWith('/'))
    {
        path.remove(0, 1);
    }
    return path;
}

} // namespace

namespace RiveQtExampleSupport {

void configureGraphicsApi()
{
#if defined(Q_OS_IOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#elif defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (QQuickWindow::graphicsApi() == QSGRendererInterface::OpenGL)
    {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setVersion(4, 2);
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);
    }
#endif
}

void configureEngine(QQmlApplicationEngine& engine)
{
#if defined(Q_OS_IOS)
    Q_UNUSED(engine);
#else
    const QString appDir = QCoreApplication::applicationDirPath();
    engine.addImportPath(QStringLiteral(RIVEQT_BUILD_QML_IMPORT_DIR));
    engine.addImportPath(appDir + QStringLiteral("/qml"));
    engine.addImportPath(appDir);
#endif
}

QString assetPath(const QString& relativePath)
{
#if defined(Q_OS_IOS)
    return QStringLiteral(":/") + normalizedResourcePath(relativePath);
#else
    return QDir(QStringLiteral(RIVEQT_SOURCE_DIR)).absoluteFilePath(relativePath);
#endif
}

QUrl assetUrl(const QString& relativePath)
{
#if defined(Q_OS_IOS)
    return QUrl(QStringLiteral("qrc:/") + normalizedResourcePath(relativePath));
#else
    return QUrl::fromLocalFile(
        QDir(QStringLiteral(RIVEQT_SOURCE_DIR)).absoluteFilePath(relativePath));
#endif
}

QUrl sourceRootUrl()
{
#if defined(Q_OS_IOS)
    return QUrl(QStringLiteral("qrc:/"));
#else
    return QUrl::fromLocalFile(
        QDir(QStringLiteral(RIVEQT_SOURCE_DIR)).absolutePath() + QDir::separator());
#endif
}

void loadMainQml(QQmlApplicationEngine& engine,
                 const QString& iosModuleUri,
                 const QString& localMainFile)
{
#if defined(Q_OS_IOS)
    engine.loadFromModule(iosModuleUri, QStringLiteral("Main"));
#else
    const QString appDir = QCoreApplication::applicationDirPath();
    engine.load(
        QUrl::fromLocalFile(QDir(appDir).absoluteFilePath(localMainFile)));
#endif
}

} // namespace RiveQtExampleSupport
