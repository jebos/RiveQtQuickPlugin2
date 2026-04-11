#include "rivefileassetloader.h"

#include <QFile>
#include <QImage>

#include "rendering/rivebackendbridge.h"

#include "rive/assets/file_asset.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/simple_array.hpp"

namespace {
void appendCandidate(QList<QUrl>* urls, const QUrl& url)
{
  if (!url.isValid()) {
    return;
  }
  if (!urls->contains(url)) {
    urls->append(url);
  }
}
} // namespace

RiveFileAssetLoader::RiveFileAssetLoader(QUrl baseUrl,
  RiveBackendBridge* bridge)
    : m_baseUrl(std::move(baseUrl))
    , m_bridge(bridge)
{
}

bool RiveFileAssetLoader::loadContents(rive::FileAsset& asset,
  rive::Span<const uint8_t> inBandBytes,
  rive::Factory* factory)
{
  QByteArray bytes;
  if (!inBandBytes.empty()) {
    bytes = QByteArray(reinterpret_cast<const char*>(inBandBytes.data()),
      static_cast<qsizetype>(inBandBytes.size()));
  } else {
    const QList<QUrl> candidateUrls = resolveUrls(asset);
    for (const QUrl& candidateUrl : candidateUrls) {
      if (!candidateUrl.isLocalFile() && !candidateUrl.scheme().isEmpty() && candidateUrl.scheme() != "qrc") {
        continue;
      }

      QString path;
      if (candidateUrl.scheme() == "qrc") {
        path = ":" + candidateUrl.path();
      } else if (candidateUrl.isLocalFile()) {
        path = candidateUrl.toLocalFile();
      } else {
        path = candidateUrl.toString();
      }

      QFile file(path);
      if (!file.open(QIODevice::ReadOnly)) {
        continue;
      }

      bytes = file.readAll();
      if (!bytes.isEmpty()) {
        break;
      }
    }

    if (bytes.isEmpty()) {
      return false;
    }
  }

  if (asset.is<rive::ImageAsset>()) {
    auto* imageAsset = static_cast<rive::ImageAsset*>(&asset);
    QImage image;
    image.loadFromData(bytes);
    if (image.isNull()) {
      return false;
    }

    auto renderImage = m_bridge->createRenderImage(image);
    if (!renderImage) {
      return false;
    }
    imageAsset->renderImage(renderImage);
    return true;
  }

  if (asset.is<rive::FontAsset>()) {
    auto* fontAsset = static_cast<rive::FontAsset*>(&asset);
    rive::SimpleArray<uint8_t> array(reinterpret_cast<const uint8_t*>(bytes.constData()),
      static_cast<size_t>(bytes.size()));
    return fontAsset->decode(array, factory);
  }

  return false;
}

QList<QUrl> RiveFileAssetLoader::resolveUrls(const rive::FileAsset& asset) const
{
  QList<QUrl> urls;
  QString name = QString::fromStdString(asset.name());
  QString uniqueFilename = QString::fromStdString(asset.uniqueFilename());

  if (isLocalLikeBaseUrl()) {
    if (!uniqueFilename.isEmpty()) {
      appendCandidate(&urls, m_baseUrl.resolved(QUrl(uniqueFilename)));
      appendCandidate(&urls,
        m_baseUrl.resolved(QUrl("../hosted/" + uniqueFilename)));
    }
    if (!name.isEmpty()) {
      appendCandidate(&urls, m_baseUrl.resolved(QUrl(name)));
      appendCandidate(&urls,
        m_baseUrl.resolved(QUrl("../hosted/" + name)));
    }
  }

  if (!isLocalLikeBaseUrl()) {
    if (!uniqueFilename.isEmpty()) {
      appendCandidate(&urls, m_baseUrl.resolved(QUrl(uniqueFilename)));
    }
    if (!name.isEmpty()) {
      appendCandidate(&urls, m_baseUrl.resolved(QUrl(name)));
    }
  }

  return urls;
}

bool RiveFileAssetLoader::isLocalLikeBaseUrl() const
{
  return m_baseUrl.isLocalFile() || m_baseUrl.scheme().isEmpty() || m_baseUrl.scheme() == "qrc";
}
