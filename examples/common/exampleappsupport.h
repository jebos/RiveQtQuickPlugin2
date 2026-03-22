#pragma once

#include <QString>
#include <QUrl>

class QQmlApplicationEngine;

namespace RiveQtExampleSupport {

void configureGraphicsApi();
void configureEngine(QQmlApplicationEngine& engine);
QString assetPath(const QString& relativePath);
QUrl assetUrl(const QString& relativePath);
QUrl sourceRootUrl();
void loadMainQml(QQmlApplicationEngine& engine,
                 const QString& iosModuleUri,
                 const QString& localMainFile);

} // namespace RiveQtExampleSupport
