#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QColor>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QImage>
#include <QElapsedTimer>
#include <QList>
#include <QLoggingCategory>
#include <QMutex>
#include <QSet>
#include <QSurfaceFormat>
#include <QTimer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QThread>
#include <cstdio>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickItem>
#include <QtQuick/QSGRendererInterface>

#ifdef Q_OS_WIN
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

extern void qml_register_types_RiveQtQuick();

namespace
{
struct LoggedMessage
{
    QByteArray category;
    QString text;
};

class LogCollector
{
public:
    static LogCollector& instance()
    {
        static LogCollector collector;
        return collector;
    }

    void install()
    {
        if (m_installed)
        {
            return;
        }
        m_previousHandler = qInstallMessageHandler(&LogCollector::messageHandler);
        m_installed = true;
    }

    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_messages.size();
    }

    int countSince(int startIndex, const QByteArray& category, const QString& token) const
    {
        QMutexLocker locker(&m_mutex);
        int count = 0;
        for (int index = qMax(0, startIndex); index < m_messages.size(); ++index)
        {
            const LoggedMessage& message = m_messages.at(index);
            if ((category.isEmpty() || message.category == category) &&
                message.text.contains(token))
            {
                ++count;
            }
        }
        return count;
    }

private:
    static void messageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& message)
    {
        LogCollector& collector = instance();
        {
            QMutexLocker locker(&collector.m_mutex);
            collector.m_messages.push_back(
                {context.category ? QByteArray(context.category) : QByteArray(),
                 message});
        }
        if (collector.m_previousHandler)
        {
            collector.m_previousHandler(type, context, message);
        }
    }

    mutable QMutex m_mutex;
    QVector<LoggedMessage> m_messages;
    QtMessageHandler m_previousHandler = nullptr;
    bool m_installed = false;
};

struct FrameSampleStats
{
    int backgroundSamples = 0;
    int changedSamples = 0;
    QSet<QRgb> quantizedChangedColors;
};

QSGRendererInterface::GraphicsApi defaultGraphicsApi()
{
#if defined(Q_OS_WIN)
    return QSGRendererInterface::Direct3D11;
#elif defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    return QSGRendererInterface::Metal;
#else
    return QSGRendererInterface::Unknown;
#endif
}

void configureDebugFailureReporting()
{
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
#endif
}

QSGRendererInterface::GraphicsApi graphicsApiFromName(const QString& name, QString* errorString = nullptr)
{
    const QString lowered = name.trimmed().toLower();
    if (lowered.isEmpty())
    {
        return defaultGraphicsApi();
    }
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

    if (errorString)
    {
        *errorString = QStringLiteral("Unsupported graphics API: %1").arg(name);
    }
    return QSGRendererInterface::Unknown;
}

QSGRendererInterface::GraphicsApi graphicsApiFromCommandLine(int argc,
                                                             char** argv,
                                                             QString* errorString = nullptr)
{
    for (int i = 1; i < argc; ++i)
    {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--graphics-api"))
        {
            if (i + 1 >= argc)
            {
                if (errorString)
                {
                    *errorString = QStringLiteral("--graphics-api requires a value");
                }
                return QSGRendererInterface::Unknown;
            }
            return graphicsApiFromName(QString::fromLocal8Bit(argv[i + 1]), errorString);
        }
    }

    return defaultGraphicsApi();
}

QString toQmlStringLiteral(const QString& value)
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return "\"" + escaped + "\"";
}

QString assetUrl(const QString& filename)
{
    return QUrl::fromLocalFile(QStringLiteral(RIVEQT_TEST_ASSET_DIR) + "/" + filename).toString();
}

QString vendoredAssetUrl(const QString& filename)
{
    return QUrl::fromLocalFile(QStringLiteral(RIVEQT_VENDOR_RIVE_TEST_ASSET_DIR) + "/" + filename).toString();
}

QRgb quantizeColor(const QColor& color)
{
    auto quantizeChannel = [](int value) {
        return (value / 32) * 32;
    };
    return qRgb(quantizeChannel(color.red()),
                quantizeChannel(color.green()),
                quantizeChannel(color.blue()));
}

FrameSampleStats sampleFrame(const QImage& frame,
                             const QRect& sampleRect,
                             const QColor& background,
                             int step = 8)
{
    FrameSampleStats stats;
    const QRect boundedRect = sampleRect.intersected(frame.rect());
    if (boundedRect.isEmpty())
    {
        return stats;
    }

    for (int y = boundedRect.y(); y < boundedRect.y() + boundedRect.height(); y += step)
    {
        for (int x = boundedRect.x(); x < boundedRect.x() + boundedRect.width(); x += step)
        {
            const QColor pixel = frame.pixelColor(x, y);
            if (pixel == background)
            {
                ++stats.backgroundSamples;
            }
            else
            {
                ++stats.changedSamples;
                stats.quantizedChangedColors.insert(quantizeColor(pixel));
            }
        }
    }

    return stats;
}

template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs)
{
    if (predicate())
    {
        return true;
    }

    QEventLoop loop;
    QTimer pollTimer;
    QTimer timeoutTimer;

    pollTimer.setInterval(16);
    pollTimer.callOnTimeout([&]() {
        if (predicate())
        {
            loop.quit();
        }
    });
    timeoutTimer.setSingleShot(true);
    timeoutTimer.callOnTimeout([&]() { loop.quit(); });

    pollTimer.start();
    timeoutTimer.start(timeoutMs);
    loop.exec();
    return predicate();
}

bool createAndValidate(QQmlEngine& engine, const QByteArray& source, const char* label)
{
    QQmlComponent component(&engine);
    component.setData(source, QUrl(QStringLiteral("memory:%1.qml").arg(QString::fromUtf8(label))));
    if (component.isLoading())
    {
        QEventLoop loop;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loop, [&](QQmlComponent::Status status) {
            if (status != QQmlComponent::Loading)
            {
                loop.quit();
            }
        });
        loop.exec();
    }
    if (!component.isReady())
    {
        std::fprintf(stderr, "%s: %s\n", label, qPrintable(component.errorString()));
        std::fflush(stderr);
        return false;
    }

    QScopedPointer<QObject> object(component.create());
    if (!object)
    {
        std::fprintf(stderr, "%s: %s\n", label, qPrintable(component.errorString()));
        std::fflush(stderr);
        return false;
    }

    if (!object->property("playing").toBool())
    {
        std::fprintf(stderr, "%s: expected playing=true\n", label);
        std::fflush(stderr);
        return false;
    }

    return true;
}

bool waitForRiveViewsReady(QQuickWindow* window,
                           const QList<QObject*>& riveViews,
                           const char* label)
{
    if (window)
    {
        window->requestUpdate();
    }
    waitFor(
        [window]() {
            if (!window)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            window->requestUpdate();
            return window->isExposed();
        },
        5000);
    const bool ready = waitFor(
        [window, riveViews]() {
            if (!window)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            window->requestUpdate();
            for (QObject* riveView : riveViews)
            {
                if (!riveView || riveView->property("status").toInt() == 1)
                {
                    return false;
                }
            }
            return true;
        },
        15000);

    bool allReady = ready;
    for (int index = 0; index < riveViews.size(); ++index)
    {
        QObject* riveView = riveViews.at(index);
        const int status = riveView ? riveView->property("status").toInt() : -1;
        if (status != 2)
        {
            std::fprintf(stderr,
                         "%s-window[%d]: status=%d error=%s\n",
                         label,
                         index,
                         status,
                         riveView ? qPrintable(riveView->property("errorString").toString())
                                  : "riveView missing");
            std::fflush(stderr);
            allReady = false;
        }
    }

    return allReady;
}

bool renderAssetAndCollectFrame(QQmlEngine& engine,
                                const QString& sourceUrl,
                                const QSize& size,
                                const QColor& background,
                                const char* label,
                                QImage* frameOut)
{
    const QString source = QString(R"(
        import QtQuick
        import QtQuick.Window
        import RiveQtQuick
        Window {
            width: %1
            height: %2
            visible: true
            color: "%3"
            RiveItem {
                id: riveView
                objectName: "riveView"
                anchors.fill: parent
                anchors.margins: 12
                fit: RiveItem.Contain
                source: %4
            }
        }
    )")
                               .arg(size.width())
                               .arg(size.height())
                               .arg(background.name())
                               .arg(toQmlStringLiteral(sourceUrl));

    QQmlComponent component(&engine);
    component.setData(source.toUtf8(), QUrl(QStringLiteral("memory:%1-window.qml").arg(QString::fromUtf8(label))));
    if (component.isLoading())
    {
        QEventLoop loop;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loop, [&](QQmlComponent::Status status) {
            if (status != QQmlComponent::Loading)
            {
                loop.quit();
            }
        });
        loop.exec();
    }

    if (!component.isReady())
    {
        std::fprintf(stderr, "%s-window: %s\n", label, qPrintable(component.errorString()));
        std::fflush(stderr);
        return false;
    }

    QScopedPointer<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    if (!window)
    {
        std::fprintf(stderr, "%s-window: root is not a QQuickWindow\n", label);
        std::fflush(stderr);
        return false;
    }

    QObject* riveView = window->findChild<QObject*>("riveView");
    if (!riveView)
    {
        std::fprintf(stderr, "%s-window: riveView not found\n", label);
        std::fflush(stderr);
        return false;
    }

    window->show();
    if (!waitForRiveViewsReady(window, {riveView}, label))
    {
        return false;
    }

    waitFor([window]() {
        window->requestUpdate();
        return window->isExposed();
    }, 250);
    QCoreApplication::sendPostedEvents(nullptr, 0);

    const QImage frame = window->grabWindow();
    if (frame.isNull())
    {
        std::fprintf(stderr, "%s-window: grabWindow returned null\n", label);
        std::fflush(stderr);
        return false;
    }

    if (frameOut)
    {
        *frameOut = frame;
    }

    window->hide();
    return true;
}

bool renderAndValidate(QQmlEngine& engine,
                       const QString& assetFile,
                       const QSize& size,
                       const QColor& background,
                       const char* label)
{
    QImage frame;
    if (!renderAssetAndCollectFrame(engine,
                                    assetUrl(assetFile),
                                    size,
                                    background,
                                    label,
                                    &frame))
    {
        return false;
    }

    const FrameSampleStats stats = sampleFrame(frame, frame.rect(), background);
    if (stats.changedSamples < 25)
    {
        std::fprintf(stderr, "%s-window: rendered frame looked blank (%d changed samples)\n", label, stats.changedSamples);
        std::fflush(stderr);
        return false;
    }

    return true;
}

bool renderBlendAndValidate(QQmlEngine& engine,
                            const QSize& size,
                            const QColor& background,
                            const char* label)
{
    QImage frame;
    if (!renderAssetAndCollectFrame(engine,
                                    vendoredAssetUrl(QStringLiteral("group_effect.riv")),
                                    size,
                                    background,
                                    label,
                                    &frame))
    {
        return false;
    }

    const QRect itemRect(12, 12, size.width() - 24, size.height() - 24);
    const FrameSampleStats stats = sampleFrame(frame, itemRect, background, 6);
    if (stats.changedSamples < 300 || stats.backgroundSamples < 100 ||
        stats.quantizedChangedColors.size() < 5)
    {
        const int colorCount = static_cast<int>(stats.quantizedChangedColors.size());
        const QString dumpPath =
            QDir::temp().filePath(QStringLiteral("%1_opengl_blend_failure.png")
                                      .arg(QString::fromUtf8(label)));
        frame.save(dumpPath);
        std::fprintf(stderr,
                     "%s-window: blend regression check failed (changed=%d background=%d colors=%d dump=%s)\n",
                     label,
                     stats.changedSamples,
                     stats.backgroundSamples,
                     colorCount,
                     qPrintable(dumpPath));
        std::fflush(stderr);
        return false;
    }

    return true;
}

bool renderAndValidateSharedContext(QQmlEngine& engine,
                                    const QSize& size,
                                    const QColor& background,
                                    const char* label)
{
    const int logStart = LogCollector::instance().size();
    const QString source = QString(R"(
        import QtQuick
        import QtQuick.Window
        import RiveQtQuick
        Window {
            width: %1
            height: %2
            visible: true
            color: "%3"
            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                RiveItem {
                    objectName: "riveView1"
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    fit: RiveItem.Contain
                    source: %4
                }
                RiveItem {
                    objectName: "riveView2"
                    width: (parent.width - parent.spacing) / 2
                    height: parent.height
                    fit: RiveItem.Contain
                    source: %4
                }
            }
        }
    )")
                               .arg(size.width())
                               .arg(size.height())
                               .arg(background.name())
                               .arg(toQmlStringLiteral(assetUrl(QStringLiteral("hello_world.riv"))));

    QQmlComponent component(&engine);
    component.setData(source.toUtf8(), QUrl(QStringLiteral("memory:%1-shared-context.qml").arg(QString::fromUtf8(label))));
    if (component.isLoading())
    {
        QEventLoop loop;
        QObject::connect(&component, &QQmlComponent::statusChanged, &loop, [&](QQmlComponent::Status status) {
            if (status != QQmlComponent::Loading)
            {
                loop.quit();
            }
        });
        loop.exec();
    }

    if (!component.isReady())
    {
        std::fprintf(stderr, "%s-shared-context: %s\n", label, qPrintable(component.errorString()));
        std::fflush(stderr);
        return false;
    }

    QScopedPointer<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    if (!window)
    {
        std::fprintf(stderr, "%s-shared-context: root is not a QQuickWindow\n", label);
        std::fflush(stderr);
        return false;
    }

    QObject* riveView1 = window->findChild<QObject*>(QStringLiteral("riveView1"));
    QObject* riveView2 = window->findChild<QObject*>(QStringLiteral("riveView2"));
    if (!riveView1 || !riveView2)
    {
        std::fprintf(stderr, "%s-shared-context: rive views not found\n", label);
        std::fflush(stderr);
        return false;
    }

    window->show();
    if (!waitForRiveViewsReady(window, {riveView1, riveView2}, label))
    {
        return false;
    }

    waitFor([window]() {
        window->requestUpdate();
        return window->isExposed();
    }, 250);
    QCoreApplication::sendPostedEvents(nullptr, 0);

    const QImage frame = window->grabWindow();
    if (frame.isNull())
    {
        std::fprintf(stderr, "%s-shared-context: grabWindow returned null\n", label);
        std::fflush(stderr);
        return false;
    }

    const FrameSampleStats stats = sampleFrame(frame, frame.rect(), background);
    if (stats.changedSamples < 25)
    {
        std::fprintf(stderr, "%s-shared-context: rendered frame looked blank\n", label);
        std::fflush(stderr);
        return false;
    }

    const int createdCount = LogCollector::instance().countSince(
        logStart,
        QByteArrayLiteral("rive.backend.gl"),
        QStringLiteral("created shared OpenGL render context"));
    const int reusedCount = LogCollector::instance().countSince(
        logStart,
        QByteArrayLiteral("rive.backend.gl"),
        QStringLiteral("reusing shared OpenGL render context"));
    if (createdCount != 1 || reusedCount < 1)
    {
        std::fprintf(stderr,
                     "%s-shared-context: expected one create and at least one reuse log (create=%d reuse=%d)\n",
                     label,
                     createdCount,
                     reusedCount);
        std::fflush(stderr);
        return false;
    }

    window->hide();
    return true;
}

int roleForName(QAbstractItemModel* model, const QByteArray& roleName)
{
    if (!model)
    {
        return -1;
    }

    const QHash<int, QByteArray> roles = model->roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
    {
        if (it.value() == roleName)
        {
            return it.key();
        }
    }

    return -1;
}

int findInputRow(QAbstractItemModel* model,
                 const QString& path,
                 const QString& source)
{
    const int pathRole = roleForName(model, "path");
    const int sourceRole = roleForName(model, "source");
    if (pathRole < 0 || sourceRole < 0)
    {
        return -1;
    }

    for (int row = 0; row < model->rowCount(); ++row)
    {
        const QModelIndex index = model->index(row, 0);
        if (model->data(index, pathRole).toString() == path &&
            model->data(index, sourceRole).toString() == source)
        {
            return row;
        }
    }

    return -1;
}

bool interactAndValidateInputs(QQmlEngine& engine, const char* label)
{
    const QString source = QString(R"(
        import QtQuick
        import QtQuick.Controls
        import QtQuick.Window
        Window {
            id: root
            width: 400
            height: 260
            visible: true
            color: "#11161d"

            property real lastNumber: controller.lastNumber
            property bool lastBoolean: controller.lastBoolean
            property int triggerCount: controller.triggerCount

            QtObject {
                id: controller
                property real lastNumber: 1.0
                property bool lastBoolean: true
                property int triggerCount: 0

                function setNumber(value) {
                    lastNumber = value
                    inputModel.setProperty(0, "value", value)
                }

                function setBoolean(value) {
                    lastBoolean = value
                    inputModel.setProperty(1, "value", value)
                }

                function fireTrigger() {
                    triggerCount += 1
                }
            }

            function applyNumber(newValue) {
                controller.setNumber(newValue)
                return true
            }

            function applyBoolean() {
                controller.setBoolean(!booleanSwitch.checked)
                return true
            }

            function applyTrigger() {
                controller.fireTrigger()
                return true
            }

            Column {
                anchors.centerIn: parent
                spacing: 16

                Slider {
                    id: numberSlider
                    objectName: "numberSlider"
                    width: 320
                    from: 0
                    to: 10
                    Binding on value {
                        value: root.lastNumber
                        when: !numberSlider.pressed
                    }
                    onMoved: controller.setNumber(value)
                }

                Switch {
                    id: booleanSwitch
                    objectName: "booleanSwitch"
                    Binding on checked {
                        value: root.lastBoolean
                        when: !booleanSwitch.down
                    }
                    onToggled: controller.setBoolean(checked)
                }

                Button {
                    id: triggerButton
                    objectName: "triggerButton"
                    text: "Trigger"
                    onClicked: controller.fireTrigger()
                }
            }
        }
    )");

    QQmlComponent component(&engine);
    component.setData(source.toUtf8(),
                      QUrl(QStringLiteral("memory:%1-inputs.qml")
                               .arg(QString::fromUtf8(label))));
    if (component.isLoading())
    {
        QEventLoop loop;
        QObject::connect(&component,
                         &QQmlComponent::statusChanged,
                         &loop,
                         [&](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading)
                             {
                                 loop.quit();
                             }
                         });
        loop.exec();
    }

    if (!component.isReady())
    {
        std::fprintf(stderr, "%s-inputs: %s\n", label, qPrintable(component.errorString()));
        std::fflush(stderr);
        return false;
    }

    QScopedPointer<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    if (!window)
    {
        std::fprintf(stderr, "%s-inputs: root is not a QQuickWindow\n", label);
        std::fflush(stderr);
        return false;
    }

    window->show();
    waitFor([window]() { return window->isExposed(); }, 5000);

    auto* numberControl = root->findChild<QObject*>(QStringLiteral("numberSlider"));
    auto* booleanControl = root->findChild<QObject*>(QStringLiteral("booleanSwitch"));
    auto* triggerControl = root->findChild<QObject*>(QStringLiteral("triggerButton"));
    if (!numberControl || !booleanControl || !triggerControl)
    {
        std::fprintf(stderr, "%s-inputs: controls were not created\n", label);
        std::fflush(stderr);
        return false;
    }

    const double originalNumber = numberControl->property("value").toDouble();
    const double targetNumber = originalNumber + 1.0;
    if (!QMetaObject::invokeMethod(root.get(),
                                   "applyNumber",
                                   Q_ARG(QVariant, QVariant(targetNumber))))
    {
        std::fprintf(stderr, "%s-inputs: applyNumber failed\n", label);
        std::fflush(stderr);
        return false;
    }

    const bool numberUpdated = waitFor(
        [&]() {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            return qAbs(numberControl->property("value").toDouble() - targetNumber) < 0.01 &&
                   qAbs(root->property("lastNumber").toDouble() - targetNumber) < 0.01;
        },
        5000);
    if (!numberUpdated)
    {
        std::fprintf(stderr,
                     "%s-inputs: number control did not settle to target value\n",
                     label);
        std::fflush(stderr);
        return false;
    }

    const bool originalBoolean = booleanControl->property("checked").toBool();
    if (!QMetaObject::invokeMethod(root.get(), "applyBoolean"))
    {
        std::fprintf(stderr, "%s-inputs: applyBoolean failed\n", label);
        std::fflush(stderr);
        return false;
    }

    const bool booleanUpdated = waitFor(
        [&]() {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            return booleanControl->property("checked").toBool() == !originalBoolean &&
                   root->property("lastBoolean").toBool() == !originalBoolean;
        },
        5000);
    if (!booleanUpdated)
    {
        std::fprintf(stderr,
                     "%s-inputs: boolean control did not settle to toggled value\n",
                     label);
        std::fflush(stderr);
        return false;
    }

    if (!QMetaObject::invokeMethod(root.get(), "applyTrigger"))
    {
        std::fprintf(stderr, "%s-inputs: applyTrigger failed\n", label);
        std::fflush(stderr);
        return false;
    }

    const bool triggerStable = waitFor(
        [&]() {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            return triggerControl != nullptr &&
                   root->property("triggerCount").toInt() == 1;
        },
        2000);
    if (!triggerStable)
    {
        std::fprintf(stderr, "%s-inputs: trigger interaction destabilized the view\n", label);
        std::fflush(stderr);
        return false;
    }

    window->hide();
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QString graphicsApiError;
    const auto graphicsApi = graphicsApiFromCommandLine(argc, argv, &graphicsApiError);
    if (!graphicsApiError.isEmpty())
    {
        std::fprintf(stderr, "qml-tests-startup: %s\n", qPrintable(graphicsApiError));
        return 10;
    }

    if (graphicsApi == QSGRendererInterface::OpenGL)
    {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setVersion(4, 2);
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);
    }

    QQuickWindow::setGraphicsApi(graphicsApi);
    QGuiApplication app(argc, argv);
    configureDebugFailureReporting();
    LogCollector::instance().install();
    if (graphicsApi == QSGRendererInterface::OpenGL)
    {
        QLoggingCategory::setFilterRules(QStringLiteral("rive.backend.gl.debug=true"));
    }
    qml_register_types_RiveQtQuick();
    QQmlEngine engine;
    const QString appDir = QCoreApplication::applicationDirPath();
    engine.addImportPath(QStringLiteral(RIVEQT_BUILD_QML_IMPORT_DIR));
    engine.addImportPath(appDir + "/qml");
    engine.addImportPath(appDir);

    if (!createAndValidate(engine,
                           QByteArrayLiteral(R"(
                               import QtQuick
                               import RiveQtQuick
                               RiveItem {
                                   Component.onCompleted: {
                                       if (!artboardsModel || !stateMachinesModel || !inputsModel || !eventsModel)
                                           throw new Error("missing inspection models")
                                   }
                               }
                           )"),
                           "defaults"))
    {
        return 11;
    }

    if (!createAndValidate(
            engine,
            QString(R"(
                import QtQuick
                import RiveQtQuick
                RiveItem { source: %1 }
            )")
                .arg(toQmlStringLiteral(assetUrl("hello_world.riv")))
                .toUtf8(),
            "hello_world"))
    {
        return 12;
    }

    if (!renderAndValidate(engine,
                           QStringLiteral("hello_world.riv"),
                           QSize(240, 240),
                           QColor(QStringLiteral("#101820")),
                           "hello_world_render"))
    {
        return 14;
    }

    if (!createAndValidate(
            engine,
            QString(R"(
                import QtQuick
                import RiveQtQuick
                RiveItem { source: %1 }
            )")
                .arg(toQmlStringLiteral(assetUrl("hosted_image_file.riv")))
                .toUtf8(),
            "hosted_image_file"))
    {
        return 15;
    }

    if (!renderAndValidate(engine,
                           QStringLiteral("hosted_image_file.riv"),
                           QSize(260, 260),
                           QColor(QStringLiteral("#132033")),
                           "hosted_image_render"))
    {
        return 16;
    }

    if (graphicsApi == QSGRendererInterface::OpenGL)
    {
        if (!renderBlendAndValidate(engine,
                                    QSize(420, 420),
                                    QColor(QStringLiteral("#12324a")),
                                    "opengl_blend"))
        {
            return 17;
        }

        if (!renderAndValidateSharedContext(engine,
                                            QSize(520, 280),
                                            QColor(QStringLiteral("#16202a")),
                                            "opengl_shared_context"))
        {
            return 18;
        }
    }

    if (!interactAndValidateInputs(engine, "input_interactions"))
    {
        return 13;
    }

    return 0;
}
