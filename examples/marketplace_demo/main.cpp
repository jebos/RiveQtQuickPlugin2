#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

#include "exampleappsupport.h"
#include "marketplacecatalog.h"

extern void qml_register_types_RiveQtQuick();

Q_LOGGING_CATEGORY(lcMarketplaceDemo, "rive.marketplaceDemo")

int main(int argc, char* argv[])
{
    RiveQtExampleSupport::configureGraphicsApi();
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    qml_register_types_RiveQtQuick();

    MarketplaceCatalogModel catalog;
    const QUrl sourceRoot = RiveQtExampleSupport::sourceRootUrl();
    auto scheduleFeaturedFeedLoad = [&catalog]() {
        QMetaObject::invokeMethod(&catalog,
                                  &MarketplaceCatalogModel::loadMore,
                                  Qt::QueuedConnection);
    };
    const QString starterCatalogPath = RiveQtExampleSupport::assetPath(
        QStringLiteral("examples/marketplace_demo/starter_catalog.json"));
    if (!catalog.loadFromFile(starterCatalogPath, sourceRoot))
    {
        qCWarning(lcMarketplaceDemo).noquote()
            << "failed to load bundled starter catalog from"
            << starterCatalogPath;
        catalog.initialize(sourceRoot);
    }
    else
    {
        qCInfo(lcMarketplaceDemo).noquote()
            << "loaded bundled starter catalog from" << starterCatalogPath
            << "with" << catalog.rowCount() << "entries";
        QTimer::singleShot(0, &catalog, scheduleFeaturedFeedLoad);
    }

#if defined(Q_OS_IOS)
    QObject::connect(&app,
                     &QGuiApplication::applicationStateChanged,
                     &catalog,
                     [&catalog, scheduleFeaturedFeedLoad](Qt::ApplicationState state) {
                         if (state == Qt::ApplicationActive)
                         {
                             scheduleFeaturedFeedLoad();
                         }
                     });
#endif

    QQmlApplicationEngine engine;
    RiveQtExampleSupport::configureEngine(engine);
    QObject::connect(&engine,
                     &QQmlApplicationEngine::warnings,
                     &app,
                     [](const QList<QQmlError>& warnings) {
                         for (const QQmlError& warning : warnings)
                         {
                             qCWarning(lcMarketplaceDemo).noquote()
                                 << "qml warning:" << warning.toString();
                         }
                     });
    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreationFailed,
                     &app,
                     [](const QUrl& url) {
                         qCWarning(lcMarketplaceDemo).noquote()
                             << "object creation failed for" << url.toString();
                     });
    engine.rootContext()->setContextProperty(QStringLiteral("catalog"), &catalog);
    RiveQtExampleSupport::loadMainQml(
        engine,
        QStringLiteral("RiveQtQuickExamples.MarketplaceDemo"),
        QStringLiteral("Main.qml"));
    if (engine.rootObjects().isEmpty())
    {
        qCWarning(lcMarketplaceDemo) << "failed to load Main.qml";
        return 1;
    }

    return app.exec();
}
