#include <QGuiApplication>
#include <QAbstractItemModel>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QImage>
#include <QElapsedTimer>
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

bool renderAndValidate(QQmlEngine& engine,
                       const QString& assetFile,
                       const QSize& size,
                       const QColor& background,
                       const char* label)
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
                               .arg(toQmlStringLiteral(assetUrl(assetFile)));

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
    window->requestUpdate();

    waitFor([window]() { return window->isExposed(); }, 5000);
    const bool ready = waitFor(
        [window, riveView]() {
            window->requestUpdate();
            return riveView->property("status").toInt() != 1;
        },
        15000);

    const int status = riveView->property("status").toInt();
    if (!ready || status != 2)
    {
        std::fprintf(stderr,
                     "%s-window: status=%d error=%s\n",
                     label,
                     status,
                     qPrintable(riveView->property("errorString").toString()));
        std::fflush(stderr);
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

    int changedPixels = 0;
    for (int y = 0; y < frame.height(); y += 8)
    {
        for (int x = 0; x < frame.width(); x += 8)
        {
            if (frame.pixelColor(x, y) != background)
            {
                ++changedPixels;
            }
        }
    }

    if (changedPixels < 25)
    {
        std::fprintf(stderr, "%s-window: rendered frame looked blank (%d changed samples)\n", label, changedPixels);
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

    QQuickWindow::setGraphicsApi(graphicsApi);
    QGuiApplication app(argc, argv);
    configureDebugFailureReporting();
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

    if (!interactAndValidateInputs(engine, "input_interactions"))
    {
        return 13;
    }

    return 0;
}
