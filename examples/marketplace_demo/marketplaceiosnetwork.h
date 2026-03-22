#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QUrl>

#include <functional>
#include <memory>

struct MarketplaceAppleResponse
{
    QByteArray payload;
    QString errorText;
    int statusCode { 0 };
};

class MarketplaceAppleRequest : public QObject
{
public:
    explicit MarketplaceAppleRequest(QObject* parent = nullptr);
    ~MarketplaceAppleRequest() override;

    MarketplaceAppleRequest(const MarketplaceAppleRequest&) = delete;
    MarketplaceAppleRequest& operator=(const MarketplaceAppleRequest&) = delete;

    void cancel();

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;

    friend MarketplaceAppleRequest* marketplaceAppleGet(
        const QUrl& url,
        const QList<QPair<QByteArray, QByteArray>>& headers,
        int timeoutMs,
        QObject* parent,
        std::function<void(const MarketplaceAppleResponse&)> completion);
};

MarketplaceAppleRequest* marketplaceAppleGet(
    const QUrl& url,
    const QList<QPair<QByteArray, QByteArray>>& headers,
    int timeoutMs,
    QObject* parent,
    std::function<void(const MarketplaceAppleResponse&)> completion);
