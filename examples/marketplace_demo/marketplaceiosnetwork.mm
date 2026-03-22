#include "marketplaceiosnetwork.h"

#import <Foundation/Foundation.h>

#include <QString>

#include <atomic>
#include <algorithm>
#include <memory>

namespace
{

QString qtString(NSString* string)
{
    return string ? QString::fromUtf8(string.UTF8String) : QString();
}

NSString* nativeString(const QString& string)
{
    const QByteArray utf8 = string.toUtf8();
    return [NSString stringWithUTF8String:utf8.constData()];
}

MarketplaceAppleResponse responseFrom(NSURLResponse* urlResponse,
                                      NSData* data,
                                      NSError* error)
{
    MarketplaceAppleResponse response;

    if (NSHTTPURLResponse* httpResponse =
            [urlResponse isKindOfClass:[NSHTTPURLResponse class]]
                ? (NSHTTPURLResponse*)urlResponse
                : nil)
    {
        response.statusCode = static_cast<int>(httpResponse.statusCode);
    }

    if (data)
    {
        response.payload = QByteArray(static_cast<const char*>(data.bytes),
                                      static_cast<qsizetype>(data.length));
    }

    if (error && error.code != NSURLErrorCancelled)
    {
        response.errorText = qtString(error.localizedDescription);
        return response;
    }

    if (response.statusCode < 200 || response.statusCode >= 300)
    {
        response.errorText =
            QStringLiteral("HTTP %1").arg(response.statusCode);
    }

    return response;
}

NSMutableURLRequest* buildRequest(
    const QUrl& url,
    const QList<QPair<QByteArray, QByteArray>>& headers,
    int timeoutMs)
{
    NSMutableURLRequest* request = [NSMutableURLRequest
        requestWithURL:[NSURL URLWithString:nativeString(url.toString())]];
    request.HTTPMethod = @"GET";
    request.cachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    request.timeoutInterval = std::max(1.0, timeoutMs / 1000.0);

    for (const auto& header : headers)
    {
        [request setValue:[NSString stringWithUTF8String:header.second.constData()]
       forHTTPHeaderField:[NSString stringWithUTF8String:header.first.constData()]];
    }

    return request;
}

} // namespace

struct MarketplaceAppleRequest::Impl
{
    std::atomic_bool cancelled { false };
    NSURLSession* session { nil };
    NSURLSessionDataTask* task { nil };
};

MarketplaceAppleRequest::MarketplaceAppleRequest(QObject* parent) :
    QObject(parent),
    m_impl(std::make_shared<Impl>())
{}

MarketplaceAppleRequest::~MarketplaceAppleRequest()
{
    cancel();
}

void MarketplaceAppleRequest::cancel()
{
    if (!m_impl || m_impl->cancelled.exchange(true))
    {
        return;
    }

    if (m_impl->task)
    {
        [m_impl->task cancel];
        m_impl->task = nil;
    }

    if (m_impl->session)
    {
        [m_impl->session invalidateAndCancel];
        m_impl->session = nil;
    }
}

MarketplaceAppleRequest* marketplaceAppleGet(
    const QUrl& url,
    const QList<QPair<QByteArray, QByteArray>>& headers,
    int timeoutMs,
    QObject* parent,
    std::function<void(const MarketplaceAppleResponse&)> completion)
{
    auto* request = new MarketplaceAppleRequest(parent);
    const auto state = request->m_impl;

    NSURLSessionConfiguration* configuration =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    configuration.timeoutIntervalForRequest =
        std::max(1.0, timeoutMs / 1000.0);
    configuration.timeoutIntervalForResource =
        std::max(30.0, timeoutMs / 1000.0);
    configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
    if (@available(iOS 11.0, *))
    {
        configuration.waitsForConnectivity = YES;
    }

    NSMutableURLRequest* nativeRequest = buildRequest(url, headers, timeoutMs);
    NSURLSession* session =
        [NSURLSession sessionWithConfiguration:configuration];
    state->session = session;

    state->task = [session
        dataTaskWithRequest:nativeRequest
          completionHandler:^(NSData* data,
                              NSURLResponse* response,
                              NSError* error) {
              const auto responseData =
                  std::make_shared<MarketplaceAppleResponse>(
                      responseFrom(response, data, error));

              [session finishTasksAndInvalidate];
              state->task = nil;
              state->session = nil;

              dispatch_async(dispatch_get_main_queue(), ^{
                  if (state->cancelled.load())
                  {
                      return;
                  }
                  completion(*responseData);
              });
          }];
    [state->task resume];

    return request;
}
