#include "exampleappsupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>

#if defined(Q_OS_MACOS)
#include <crt_externs.h>
#endif

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

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

QSGRendererInterface::GraphicsApi graphicsApiFromName(const QString& name)
{
    const QString lowered = name.trimmed().toLower();
    if (lowered == QStringLiteral("d3d11"))
    {
        return QSGRendererInterface::Direct3D11;
    }
    if (lowered == QStringLiteral("d3d12"))
    {
        return QSGRendererInterface::Direct3D12;
    }
    if (lowered == QStringLiteral("vulkan"))
    {
        return QSGRendererInterface::Vulkan;
    }
    if (lowered == QStringLiteral("opengl") || lowered == QStringLiteral("gl"))
    {
        return QSGRendererInterface::OpenGL;
    }
    if (lowered == QStringLiteral("metal"))
    {
        return QSGRendererInterface::Metal;
    }
    if (lowered == QStringLiteral("software"))
    {
        return QSGRendererInterface::Software;
    }
    return QSGRendererInterface::Unknown;
}

QStringList processCommandLineArguments()
{
    QStringList arguments;

#if defined(Q_OS_WIN)
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv)
    {
        for (int i = 0; i < argc; ++i)
        {
            arguments.append(QString::fromWCharArray(argv[i]));
        }
        LocalFree(argv);
    }
#elif defined(Q_OS_MACOS)
    int argc = *_NSGetArgc();
    char** argv = *_NSGetArgv();
    for (int i = 0; i < argc; ++i)
    {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }
#else
    QFile cmdline(QStringLiteral("/proc/self/cmdline"));
    if (cmdline.open(QIODevice::ReadOnly))
    {
        const QByteArray raw = cmdline.readAll();
        const QList<QByteArray> parts = raw.split('\0');
        for (const QByteArray& part : parts)
        {
            if (!part.isEmpty())
            {
                arguments.append(QString::fromLocal8Bit(part));
            }
        }
    }
#endif

    return arguments;
}

QSGRendererInterface::GraphicsApi graphicsApiFromProcess()
{
    const QStringList arguments = processCommandLineArguments();
    for (int i = 0; i < arguments.size(); ++i)
    {
        if (arguments.at(i) == QStringLiteral("--graphics-api") && i + 1 < arguments.size())
        {
            const auto api = graphicsApiFromName(arguments.at(i + 1));
            if (api != QSGRendererInterface::Unknown)
            {
                return api;
            }
        }
    }

    const QString envValue = qEnvironmentVariable("RIVEQT_GRAPHICS_API");
    if (!envValue.isEmpty())
    {
        const auto api = graphicsApiFromName(envValue);
        if (api != QSGRendererInterface::Unknown)
        {
            return api;
        }
    }

    return QSGRendererInterface::Unknown;
}

} // namespace

namespace RiveQtExampleSupport {

void configureGraphicsApi()
{
#if defined(Q_OS_IOS)
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
#else
    const auto requestedApi = graphicsApiFromProcess();
    if (requestedApi != QSGRendererInterface::Unknown)
    {
        if (requestedApi == QSGRendererInterface::OpenGL)
        {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
            QSurfaceFormat format;
            format.setRenderableType(QSurfaceFormat::OpenGL);
            format.setProfile(QSurfaceFormat::CoreProfile);
            format.setVersion(4, 2);
            format.setDepthBufferSize(24);
            format.setStencilBufferSize(8);
            QSurfaceFormat::setDefaultFormat(format);
#endif
        }
        QQuickWindow::setGraphicsApi(requestedApi);
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
