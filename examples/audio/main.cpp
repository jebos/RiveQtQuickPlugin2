#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "exampleappsupport.h"

extern void qml_register_types_RiveQtQuick();

int main(int argc, char* argv[])
{
    RiveQtExampleSupport::configureGraphicsApi();
    QGuiApplication app(argc, argv);
    RiveQtExampleSupport::configureAudioPlaybackSession();
    qml_register_types_RiveQtQuick();

    const QUrl assetUrl = RiveQtExampleSupport::assetUrl(
        QStringLiteral("tests/assets/rive/26986-50750-scripted-audio-play-stop-volume.riv"));

    QQmlApplicationEngine engine;
    RiveQtExampleSupport::configureEngine(engine);
    engine.rootContext()->setContextProperty(QStringLiteral("exampleRiveUrl"),
                                             assetUrl);
    RiveQtExampleSupport::loadMainQml(
        engine,
        QStringLiteral("RiveQtQuickExamples.Audio"),
        QStringLiteral("Main.qml"));
    if (engine.rootObjects().isEmpty())
    {
        return 1;
    }

    return app.exec();
}
