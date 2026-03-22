#include "marketplacecatalog.h"

#if defined(Q_OS_IOS)
#include "marketplaceiosnetwork.h"
#endif

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>

namespace
{
constexpr auto kCachedApiBase = "https://api-cached.rive.app";

bool entrySupportsPreview(const MarketplaceCatalogEntry& entry)
{
    return !entry.cachedRiveSource.isEmpty() ||
           entry.availability == QStringLiteral("local");
}

bool entryCanDownload(const MarketplaceCatalogEntry& entry)
{
    if (entry.isDownloading || !entry.cachedRiveSource.isEmpty())
    {
        return false;
    }

    const QUrl url = QUrl::fromUserInput(entry.riveSource);
    if (!url.isValid())
    {
        return false;
    }

    return url.scheme() == QStringLiteral("http") ||
           url.scheme() == QStringLiteral("https");
}

QString normalizedString(const QString& value)
{
    return value.trimmed().toLower();
}

QVariantMap emptyMetadata()
{
    return {
        {QStringLiteral("title"), QString()},
        {QStringLiteral("author"), QString()},
        {QStringLiteral("license"), QString()},
        {QStringLiteral("sourceUrl"), QString()},
        {QStringLiteral("previewImageUrl"), QString()},
        {QStringLiteral("isForHire"), false},
        {QStringLiteral("tags"), QStringList()},
        {QStringLiteral("description"), QString()},
        {QStringLiteral("availability"), QString()},
        {QStringLiteral("canDownload"), false},
        {QStringLiteral("isDownloading"), false},
        {QStringLiteral("downloadError"), QString()},
        {QStringLiteral("downloadBytes"), qint64(0)},
        {QStringLiteral("downloadTotalBytes"), qint64(0)},
        {QStringLiteral("localFilePath"), QString()},
        {QStringLiteral("hasPreview"), false},
        {QStringLiteral("reactionCount"), 0},
    };
}

QList<QPair<QByteArray, QByteArray>> defaultHeaders(const QByteArray& userAgent,
                                                    const QByteArray& accept)
{
    return {
        {QByteArrayLiteral("User-Agent"), userAgent},
        {QByteArrayLiteral("accept"), accept},
    };
}

} // namespace

MarketplaceCatalogModel::MarketplaceCatalogModel(QObject* parent) :
    QAbstractListModel(parent)
{}

int MarketplaceCatalogModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_visibleIndexes.size();
}

QVariant MarketplaceCatalogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return {};
    }
    const MarketplaceCatalogEntry* entry = entryAtVisibleIndex(index.row());
    if (!entry)
    {
        return {};
    }

    switch (role)
    {
        case Qt::DisplayRole:
        case TitleRole:
            return entry->title;
        case IdRole:
            return entry->id;
        case DetailUrlRole:
            return entry->detailUrl;
        case AuthorRole:
            return entry->author;
        case IsForHireRole:
            return entry->isForHire;
        case LicenseRole:
            return entry->license;
        case TagsRole:
            return entry->tags;
        case DescriptionRole:
            return entry->description;
        case PreviewImageUrlRole:
            return previewImageUrlForEntry(*entry);
        case RiveSourceRole:
            return sourceUrlForEntry(*entry);
        case AvailabilityRole:
            return entry->availability;
        case ReactionCountRole:
            return entry->reactionCount;
        case CurrentRole:
            return index.row() == m_currentIndex;
        case HasPreviewRole:
            return sourceUrlForEntry(*entry).isValid();
        default:
            return {};
    }
}

QHash<int, QByteArray> MarketplaceCatalogModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {TitleRole, "title"},
        {DetailUrlRole, "detailUrl"},
        {AuthorRole, "author"},
        {IsForHireRole, "isForHire"},
        {LicenseRole, "license"},
        {TagsRole, "tags"},
        {DescriptionRole, "description"},
        {PreviewImageUrlRole, "previewImageUrl"},
        {RiveSourceRole, "riveSource"},
        {AvailabilityRole, "availability"},
        {ReactionCountRole, "reactionCount"},
        {CurrentRole, "current"},
        {HasPreviewRole, "hasPreview"},
    };
}

QString MarketplaceCatalogModel::searchText() const
{
    return m_searchText;
}

void MarketplaceCatalogModel::setSearchText(const QString& searchText)
{
    if (m_searchText == searchText)
    {
        return;
    }

    m_searchText = searchText;
    emit searchTextChanged();
    rebuildVisibleEntries();
}

int MarketplaceCatalogModel::currentIndex() const
{
    return m_currentIndex;
}

void MarketplaceCatalogModel::setCurrentIndex(int currentIndex)
{
    if (currentIndex < 0 || currentIndex >= m_visibleIndexes.size())
    {
        currentIndex = -1;
    }

    if (m_currentIndex == currentIndex)
    {
        return;
    }

    QModelIndex previousIndex = index(m_currentIndex, 0);
    QModelIndex nextIndex = index(currentIndex, 0);
    const bool previousValid = previousIndex.isValid();
    const bool nextValid = nextIndex.isValid();

    m_currentIndex = currentIndex;
    m_currentId = currentIndex >= 0
                      ? m_entries.at(m_visibleIndexes.at(currentIndex)).id
                      : QString();

    if (previousValid)
    {
        emit dataChanged(previousIndex, previousIndex, {CurrentRole});
    }
    if (nextValid)
    {
        emit dataChanged(nextIndex, nextIndex, {CurrentRole});
    }

    emit currentIndexChanged();
    emit currentMetadataChanged();
    emit currentRiveSourceChanged();
    emit currentHasPreviewChanged();
}

QVariantMap MarketplaceCatalogModel::currentMetadata() const
{
    const MarketplaceCatalogEntry* entry = entryAtVisibleIndex(m_currentIndex);
    return entry ? metadataForEntry(*entry) : emptyMetadata();
}

QUrl MarketplaceCatalogModel::currentRiveSource() const
{
    const MarketplaceCatalogEntry* entry = entryAtVisibleIndex(m_currentIndex);
    return entry ? sourceUrlForEntry(*entry) : QUrl();
}

bool MarketplaceCatalogModel::currentHasPreview() const
{
    return currentRiveSource().isValid();
}

bool MarketplaceCatalogModel::isLoadingMore() const
{
    return m_isLoadingMore;
}

bool MarketplaceCatalogModel::hasMoreRemoteEntries() const
{
    return m_hasMoreRemoteEntries;
}

QString MarketplaceCatalogModel::loadError() const
{
    return m_loadError;
}

void MarketplaceCatalogModel::initialize(const QUrl& sourceBaseUrl)
{
    beginResetModel();
    m_entries.clear();
    m_visibleIndexes.clear();
    m_currentIndex = -1;
    m_currentId.clear();
    m_sourceBaseUrl = sourceBaseUrl;
    m_nextFeaturedPage = 0;
    m_isLoadingMore = false;
    m_hasMoreRemoteEntries = true;
    m_loadError.clear();
    if (m_loadReply)
    {
        m_loadReply->abort();
        m_loadReply->deleteLater();
        m_loadReply = nullptr;
    }
    if (m_downloadReply)
    {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
#if defined(Q_OS_IOS)
    if (m_iosLoadRequest)
    {
        m_iosLoadRequest->cancel();
        m_iosLoadRequest->deleteLater();
        m_iosLoadRequest = nullptr;
    }
    if (m_iosDownloadRequest)
    {
        m_iosDownloadRequest->cancel();
        m_iosDownloadRequest->deleteLater();
        m_iosDownloadRequest = nullptr;
    }
    for (auto it = m_previewImageRequests.begin();
         it != m_previewImageRequests.end();
         ++it)
    {
        if (*it)
        {
            (*it)->cancel();
            (*it)->deleteLater();
        }
    }
    m_previewImageRequests.clear();
#endif
    endResetModel();

    emit currentIndexChanged();
    emit currentMetadataChanged();
    emit currentRiveSourceChanged();
    emit currentHasPreviewChanged();
    emit isLoadingMoreChanged();
    emit hasMoreRemoteEntriesChanged();
    emit loadErrorChanged();

    loadMore();
}

bool MarketplaceCatalogModel::loadFromFile(const QString& catalogPath,
                                           const QUrl& sourceBaseUrl)
{
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray())
    {
        return false;
    }

    beginResetModel();
    m_entries.clear();
    m_visibleIndexes.clear();
    m_currentIndex = -1;
    m_currentId.clear();
    m_sourceBaseUrl = sourceBaseUrl;
    m_nextFeaturedPage = 0;
    m_isLoadingMore = false;
    m_hasMoreRemoteEntries = true;
    m_loadError.clear();
    if (m_loadReply)
    {
        m_loadReply->abort();
        m_loadReply->deleteLater();
        m_loadReply = nullptr;
    }
    if (m_downloadReply)
    {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
#if defined(Q_OS_IOS)
    if (m_iosLoadRequest)
    {
        m_iosLoadRequest->cancel();
        m_iosLoadRequest->deleteLater();
        m_iosLoadRequest = nullptr;
    }
    if (m_iosDownloadRequest)
    {
        m_iosDownloadRequest->cancel();
        m_iosDownloadRequest->deleteLater();
        m_iosDownloadRequest = nullptr;
    }
    for (auto it = m_previewImageRequests.begin();
         it != m_previewImageRequests.end();
         ++it)
    {
        if (*it)
        {
            (*it)->cancel();
            (*it)->deleteLater();
        }
    }
    m_previewImageRequests.clear();
#endif

    const QJsonArray entries = document.array();
    m_entries.reserve(entries.size());
    for (const QJsonValue& value : entries)
    {
        if (value.isObject())
        {
            MarketplaceCatalogEntry entry = entryFromJson(value.toObject());
            hydrateCachedEntry(&entry);
            m_entries.append(entry);
        }
    }
    endResetModel();

    rebuildVisibleEntries();
    return true;
}

void MarketplaceCatalogModel::loadMore()
{
    if (m_isLoadingMore || !m_hasMoreRemoteEntries)
    {
        return;
    }

    QUrl url(QString::fromLatin1(kCachedApiBase) +
             QStringLiteral("/api/community-posts/featured"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
    query.addQueryItem(QStringLiteral("page"),
                       QString::number(m_nextFeaturedPage));
    url.setQuery(query);

#if defined(Q_OS_IOS)
    setLoadError(QString());
    setLoadingMore(true);
    m_iosLoadRequest = marketplaceAppleGet(
        url,
        defaultHeaders(QByteArrayLiteral("QtQuickRivePlugin2 marketplace browser"),
                       QByteArrayLiteral("application/json, text/plain, */*")),
        30000,
        this,
        [this, url](const MarketplaceAppleResponse& response) {
            if (m_iosLoadRequest)
            {
                m_iosLoadRequest->deleteLater();
                m_iosLoadRequest = nullptr;
            }

            if (!response.errorText.isEmpty())
            {
                qWarning() << "Marketplace featured feed request failed for"
                           << url << ":" << response.errorText;
                setLoadError(response.errorText);
                setLoadingMore(false);
                return;
            }

            const QJsonDocument document =
                QJsonDocument::fromJson(response.payload);
            if (!document.isArray())
            {
                qWarning() << "Marketplace featured feed returned unexpected data"
                           << "for" << url;
                setLoadError(
                    QStringLiteral("Featured feed returned unexpected data."));
                setLoadingMore(false);
                return;
            }

            const QJsonArray entries = document.array();
            QVector<MarketplaceCatalogEntry> parsedEntries;
            parsedEntries.reserve(entries.size());
            for (const QJsonValue& value : entries)
            {
                if (!value.isObject())
                {
                    continue;
                }
                parsedEntries.append(entryFromApiJson(value.toObject()));
            }

            qInfo() << "Marketplace featured feed returned"
                    << parsedEntries.size() << "entries for page"
                    << m_nextFeaturedPage << "via NSURLSession";

            appendEntries(parsedEntries);
            ++m_nextFeaturedPage;
            setHasMoreRemoteEntries(!entries.isEmpty());
            setLoadError(QString());
            setLoadingMore(false);
        });
    return;
#else
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("QtQuickRivePlugin2 marketplace browser"));
    request.setRawHeader("accept", "application/json, text/plain, */*");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    setLoadError(QString());
    setLoadingMore(true);
    QNetworkReply* reply = m_networkAccessManager.get(request);
    m_loadReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        const QPointer<QNetworkReply> guard(reply);
        if (reply->error() != QNetworkReply::NoError)
        {
            const QString errorText = reply->errorString();
            qWarning() << "Marketplace featured feed request failed for" << url
                       << ":" << errorText;
            setLoadError(errorText);
            setLoadingMore(false);
            if (m_loadReply == reply)
            {
                m_loadReply = nullptr;
            }
            reply->deleteLater();
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if (!document.isArray())
        {
            qWarning() << "Marketplace featured feed returned unexpected data"
                       << "for" << url;
            setLoadError(QStringLiteral("Featured feed returned unexpected data."));
            setLoadingMore(false);
            if (m_loadReply == reply)
            {
                m_loadReply = nullptr;
            }
            reply->deleteLater();
            return;
        }

        const QJsonArray entries = document.array();
        QVector<MarketplaceCatalogEntry> parsedEntries;
        parsedEntries.reserve(entries.size());
        for (const QJsonValue& value : entries)
        {
            if (!value.isObject())
            {
                continue;
            }
            parsedEntries.append(entryFromApiJson(value.toObject()));
        }

        qInfo() << "Marketplace featured feed returned" << parsedEntries.size()
                << "entries for page" << m_nextFeaturedPage;

        appendEntries(parsedEntries);
        ++m_nextFeaturedPage;
        setHasMoreRemoteEntries(!entries.isEmpty());
        setLoadError(QString());
        setLoadingMore(false);

        if (m_loadReply == guard)
        {
            m_loadReply = nullptr;
        }
        reply->deleteLater();
    });
#endif
}

void MarketplaceCatalogModel::downloadCurrent()
{
    const int entryIndex = currentEntryIndex();
    if (entryIndex < 0 || entryIndex >= m_entries.size() || m_downloadReply
#if defined(Q_OS_IOS)
        || m_iosDownloadRequest
#endif
    )
    {
        return;
    }

    MarketplaceCatalogEntry& entry = m_entries[entryIndex];
    if (!entryCanDownload(entry))
    {
        if (entry.cachedRiveSource.isEmpty())
        {
            entry.downloadError = QStringLiteral(
                "This entry does not expose a downloadable runtime file.");
            notifyEntryUpdated(entryIndex, {});
        }
        return;
    }

    const QUrl remoteUrl = QUrl::fromUserInput(entry.riveSource);
    entry.isDownloading = true;
    entry.downloadError.clear();
    entry.downloadBytes = 0;
    entry.downloadTotalBytes = 0;
    notifyEntryUpdated(entryIndex, {});

#if defined(Q_OS_IOS)
    m_iosDownloadRequest = marketplaceAppleGet(
        remoteUrl,
        defaultHeaders(
            QByteArrayLiteral("QtQuickRivePlugin2 marketplace downloader"),
            QByteArrayLiteral("*/*")),
        30000,
        this,
        [this, entryIndex](const MarketplaceAppleResponse& response) {
            if (m_iosDownloadRequest)
            {
                m_iosDownloadRequest->deleteLater();
                m_iosDownloadRequest = nullptr;
            }

            if (entryIndex < 0 || entryIndex >= m_entries.size())
            {
                return;
            }

            MarketplaceCatalogEntry& entry = m_entries[entryIndex];
            entry.isDownloading = false;
            entry.downloadBytes = 0;
            entry.downloadTotalBytes = 0;

            if (!response.errorText.isEmpty())
            {
                entry.downloadError = response.errorText;
                notifyEntryUpdated(entryIndex, {});
                return;
            }

            if (response.payload.isEmpty())
            {
                entry.downloadError = QStringLiteral(
                    "Downloaded runtime file was empty.");
                notifyEntryUpdated(entryIndex, {});
                return;
            }

            const QString targetPath = cacheFilePathForEntry(entry);
            QDir().mkpath(QFileInfo(targetPath).absolutePath());

            QSaveFile targetFile(targetPath);
            if (!targetFile.open(QIODevice::WriteOnly) ||
                targetFile.write(response.payload) != response.payload.size() ||
                !targetFile.commit())
            {
                entry.downloadError = targetFile.errorString();
                notifyEntryUpdated(entryIndex, {});
                return;
            }

            entry.cachedRiveSource = QUrl::fromLocalFile(targetPath).toString();
            entry.availability = QStringLiteral("downloaded");
            entry.downloadError.clear();
            notifyEntryUpdated(entryIndex, {});
        });
    return;
#else
    QNetworkRequest request(remoteUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("QtQuickRivePlugin2 marketplace downloader"));
    request.setRawHeader("accept", "*/*");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_networkAccessManager.get(request);
    reply->setProperty("entryIndex", entryIndex);
    m_downloadReply = reply;

    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            [this, reply](qint64 received, qint64 total) {
                const int currentEntry = reply->property("entryIndex").toInt();
                if (currentEntry < 0 || currentEntry >= m_entries.size())
                {
                    return;
                }

                MarketplaceCatalogEntry& entry = m_entries[currentEntry];
                entry.downloadBytes = received;
                entry.downloadTotalBytes = total;
                notifyEntryUpdated(currentEntry, {});
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const int entryIndex = reply->property("entryIndex").toInt();
        const bool validEntry =
            entryIndex >= 0 && entryIndex < m_entries.size();

        auto finalizeReply = [this, reply]() {
            if (m_downloadReply == reply)
            {
                m_downloadReply = nullptr;
            }
            reply->deleteLater();
        };

        if (!validEntry)
        {
            finalizeReply();
            return;
        }

        MarketplaceCatalogEntry& entry = m_entries[entryIndex];
        entry.isDownloading = false;
        entry.downloadBytes = 0;
        entry.downloadTotalBytes = 0;

        if (reply->error() != QNetworkReply::NoError)
        {
            entry.downloadError = reply->errorString();
            notifyEntryUpdated(entryIndex, {});
            finalizeReply();
            return;
        }

        const QByteArray payload = reply->readAll();
        if (payload.isEmpty())
        {
            entry.downloadError = QStringLiteral(
                "Downloaded runtime file was empty.");
            notifyEntryUpdated(entryIndex, {});
            finalizeReply();
            return;
        }

        const QString targetPath = cacheFilePathForEntry(entry);
        QDir().mkpath(QFileInfo(targetPath).absolutePath());

        QSaveFile targetFile(targetPath);
        if (!targetFile.open(QIODevice::WriteOnly) ||
            targetFile.write(payload) != payload.size() || !targetFile.commit())
        {
            entry.downloadError = targetFile.errorString();
            notifyEntryUpdated(entryIndex, {});
            finalizeReply();
            return;
        }

        entry.cachedRiveSource = QUrl::fromLocalFile(targetPath).toString();
        entry.availability = QStringLiteral("downloaded");
        entry.downloadError.clear();
        notifyEntryUpdated(entryIndex, {});
        finalizeReply();
    });
#endif
}

void MarketplaceCatalogModel::appendEntries(
    const QVector<MarketplaceCatalogEntry>& entries)
{
    QVector<int> appendedVisibleIndexes;
    const int previousVisibleCount = m_visibleIndexes.size();

    for (MarketplaceCatalogEntry entry : entries)
    {
        if (entry.id.isEmpty() || indexOfEntryId(entry.id) >= 0)
        {
            continue;
        }

        hydrateCachedEntry(&entry);
        const int entryIndex = m_entries.size();
        m_entries.append(entry);
        if (matchesFilter(entry))
        {
            appendedVisibleIndexes.append(entryIndex);
        }
    }

    if (!appendedVisibleIndexes.isEmpty())
    {
        beginInsertRows(QModelIndex(),
                        previousVisibleCount,
                        previousVisibleCount + appendedVisibleIndexes.size() - 1);
        for (int entryIndex : appendedVisibleIndexes)
        {
            m_visibleIndexes.append(entryIndex);
        }
        endInsertRows();

        if (m_currentIndex < 0)
        {
            setCurrentIndex(0);
        }
    }

#if defined(Q_OS_IOS)
    for (int entryIndex : appendedVisibleIndexes)
    {
        fetchPreviewImage(entryIndex);
    }
#endif
}

void MarketplaceCatalogModel::setLoadingMore(bool loading)
{
    if (m_isLoadingMore == loading)
    {
        return;
    }
    m_isLoadingMore = loading;
    emit isLoadingMoreChanged();
}

void MarketplaceCatalogModel::setHasMoreRemoteEntries(bool hasMore)
{
    if (m_hasMoreRemoteEntries == hasMore)
    {
        return;
    }
    m_hasMoreRemoteEntries = hasMore;
    emit hasMoreRemoteEntriesChanged();
}

void MarketplaceCatalogModel::setLoadError(const QString& error)
{
    if (m_loadError == error)
    {
        return;
    }

    m_loadError = error;
    emit loadErrorChanged();
}

int MarketplaceCatalogModel::indexOfEntryId(const QString& id) const
{
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (m_entries.at(i).id == id)
        {
            return i;
        }
    }
    return -1;
}

int MarketplaceCatalogModel::visibleRowForEntryIndex(int entryIndex) const
{
    for (int row = 0; row < m_visibleIndexes.size(); ++row)
    {
        if (m_visibleIndexes.at(row) == entryIndex)
        {
            return row;
        }
    }

    return -1;
}

int MarketplaceCatalogModel::currentEntryIndex() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_visibleIndexes.size())
    {
        return -1;
    }

    return m_visibleIndexes.at(m_currentIndex);
}

void MarketplaceCatalogModel::notifyEntryUpdated(int entryIndex,
                                                 const QVector<int>& roles)
{
    const int visibleRow = visibleRowForEntryIndex(entryIndex);
    if (visibleRow >= 0)
    {
        emit dataChanged(index(visibleRow, 0), index(visibleRow, 0), roles);
    }

    if (entryIndex == currentEntryIndex())
    {
        emit currentMetadataChanged();
        emit currentRiveSourceChanged();
        emit currentHasPreviewChanged();
    }
}

void MarketplaceCatalogModel::rebuildVisibleEntries()
{
    const QString previousId = m_currentId;

    beginResetModel();
    m_visibleIndexes.clear();
    for (int i = 0; i < m_entries.size(); ++i)
    {
        if (matchesFilter(m_entries.at(i)))
        {
            m_visibleIndexes.append(i);
        }
    }
    endResetModel();

    m_currentIndex = -1;
    if (!previousId.isEmpty())
    {
        setCurrentEntryById(previousId);
    }

    if (m_currentIndex < 0 && !m_visibleIndexes.isEmpty())
    {
        setCurrentIndex(0);
    }
    else
    {
        emit currentMetadataChanged();
        emit currentRiveSourceChanged();
        emit currentHasPreviewChanged();
    }
}

bool MarketplaceCatalogModel::matchesFilter(
    const MarketplaceCatalogEntry& entry) const
{
    const QString query = normalizedString(m_searchText);
    if (query.isEmpty())
    {
        return true;
    }

    if (normalizedString(entry.title).contains(query) ||
        normalizedString(entry.author).contains(query) ||
        normalizedString(entry.license).contains(query) ||
        normalizedString(entry.description).contains(query))
    {
        return true;
    }

    for (const QString& tag : entry.tags)
    {
        if (normalizedString(tag).contains(query))
        {
            return true;
        }
    }

    return false;
}

void MarketplaceCatalogModel::setCurrentEntryById(const QString& id)
{
    for (int i = 0; i < m_visibleIndexes.size(); ++i)
    {
        if (m_entries.at(m_visibleIndexes.at(i)).id == id)
        {
            setCurrentIndex(i);
            return;
        }
    }
}

void MarketplaceCatalogModel::hydrateCachedEntry(MarketplaceCatalogEntry* entry) const
{
    if (!entry || entry->id.isEmpty())
    {
        return;
    }

    const QString cachedFilePath = cacheFilePathForEntry(*entry);
    if (!QFileInfo::exists(cachedFilePath))
    {
        return;
    }

    entry->cachedRiveSource = QUrl::fromLocalFile(cachedFilePath).toString();
    if (entry->availability != QStringLiteral("local"))
    {
        entry->availability = QStringLiteral("downloaded");
    }

    const QString cachedPreviewPath = previewImageCachePathForEntry(*entry);
    if (QFileInfo::exists(cachedPreviewPath))
    {
        entry->cachedPreviewImageSource =
            QUrl::fromLocalFile(cachedPreviewPath).toString();
    }
}

QString MarketplaceCatalogModel::cacheFilePathForEntry(
    const MarketplaceCatalogEntry& entry) const
{
    const QString baseDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    return QDir(baseDir).filePath(QStringLiteral("marketplace-cache/%1.riv")
                                      .arg(entry.id));
}

QString MarketplaceCatalogModel::previewImageCachePathForEntry(
    const MarketplaceCatalogEntry& entry) const
{
    QString suffix =
        QFileInfo(QUrl(entry.previewImageUrl).path()).suffix().toLower();
    if (suffix.isEmpty())
    {
        suffix = QStringLiteral("png");
    }

    const QString baseDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    return QDir(baseDir).filePath(
        QStringLiteral("marketplace-preview-cache/%1.%2")
            .arg(entry.id, suffix));
}

const MarketplaceCatalogEntry* MarketplaceCatalogModel::entryAtVisibleIndex(
    int index) const
{
    if (index < 0 || index >= m_visibleIndexes.size())
    {
        return nullptr;
    }
    return &m_entries.at(m_visibleIndexes.at(index));
}

QVariantMap MarketplaceCatalogModel::metadataForEntry(
    const MarketplaceCatalogEntry& entry) const
{
    return {
        {QStringLiteral("title"), entry.title},
        {QStringLiteral("author"), entry.author},
        {QStringLiteral("license"), entry.license},
        {QStringLiteral("sourceUrl"), entry.detailUrl},
        {QStringLiteral("previewImageUrl"),
         previewImageUrlForEntry(entry).toString()},
        {QStringLiteral("isForHire"), entry.isForHire},
        {QStringLiteral("tags"), entry.tags},
        {QStringLiteral("description"), entry.description},
        {QStringLiteral("availability"), entry.availability},
        {QStringLiteral("canDownload"), entryCanDownload(entry)},
        {QStringLiteral("isDownloading"), entry.isDownloading},
        {QStringLiteral("downloadError"), entry.downloadError},
        {QStringLiteral("downloadBytes"), entry.downloadBytes},
        {QStringLiteral("downloadTotalBytes"), entry.downloadTotalBytes},
        {QStringLiteral("localFilePath"),
         entry.cachedRiveSource.isEmpty()
             ? QString()
             : QUrl(entry.cachedRiveSource).toLocalFile()},
        {QStringLiteral("hasPreview"), entrySupportsPreview(entry)},
        {QStringLiteral("reactionCount"), entry.reactionCount},
    };
}

QUrl MarketplaceCatalogModel::sourceUrlForEntry(
    const MarketplaceCatalogEntry& entry) const
{
    if (!entry.cachedRiveSource.isEmpty())
    {
        return resolveSourceUrl(entry.cachedRiveSource);
    }

    if (!entrySupportsPreview(entry) || entry.riveSource.isEmpty())
    {
        return {};
    }

    const QUrl url = resolveSourceUrl(entry.riveSource);
    if (!url.isValid())
    {
        return {};
    }

    if (!url.isLocalFile() && !url.scheme().isEmpty() && url.scheme() != QStringLiteral("qrc"))
    {
        return {};
    }

    return url;
}

QUrl MarketplaceCatalogModel::previewImageUrlForEntry(
    const MarketplaceCatalogEntry& entry) const
{
    if (!entry.cachedPreviewImageSource.isEmpty())
    {
        return resolveSourceUrl(entry.cachedPreviewImageSource);
    }

    const QUrl url = resolveSourceUrl(entry.previewImageUrl);
#if defined(Q_OS_IOS)
    if (url.isValid() && !url.isLocalFile() &&
        url.scheme() != QStringLiteral("qrc"))
    {
        return {};
    }
#endif
    return url;
}

QUrl MarketplaceCatalogModel::resolveSourceUrl(const QString& source) const
{
    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty())
    {
        return {};
    }

    const QFileInfo sourceInfo(trimmedSource);
    if (sourceInfo.isAbsolute())
    {
        return QUrl::fromLocalFile(sourceInfo.absoluteFilePath());
    }

    const QUrl directUrl(trimmedSource);
    if (directUrl.isValid() &&
        (directUrl.scheme() == QStringLiteral("qrc") || !directUrl.isRelative()))
    {
        return directUrl;
    }

    if (m_sourceBaseUrl.isValid())
    {
        const QUrl resolvedUrl = m_sourceBaseUrl.resolved(QUrl(trimmedSource));
        if (resolvedUrl.isValid())
        {
            return resolvedUrl;
        }
    }

    const QUrl userInputUrl =
        QUrl::fromUserInput(trimmedSource, QString(), QUrl::AssumeLocalFile);
    return userInputUrl.isValid() ? userInputUrl : QUrl();
}

#if defined(Q_OS_IOS)
void MarketplaceCatalogModel::fetchPreviewImage(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= m_entries.size())
    {
        return;
    }

    MarketplaceCatalogEntry& entry = m_entries[entryIndex];
    if (!entry.cachedPreviewImageSource.isEmpty() ||
        m_previewImageRequests.contains(entryIndex))
    {
        return;
    }

    const QUrl url = resolveSourceUrl(entry.previewImageUrl);
    if (!url.isValid() || url.isLocalFile() ||
        url.scheme() == QStringLiteral("qrc"))
    {
        return;
    }

    auto* request = marketplaceAppleGet(
        url,
        defaultHeaders(
            QByteArrayLiteral("QtQuickRivePlugin2 marketplace preview"),
            QByteArrayLiteral("image/*,*/*")),
        30000,
        this,
        [this, entryIndex](const MarketplaceAppleResponse& response) {
            if (auto it = m_previewImageRequests.find(entryIndex);
                it != m_previewImageRequests.end())
            {
                if (*it)
                {
                    (*it)->deleteLater();
                }
                m_previewImageRequests.erase(it);
            }

            if (entryIndex < 0 || entryIndex >= m_entries.size() ||
                !response.errorText.isEmpty() || response.payload.isEmpty())
            {
                return;
            }

            MarketplaceCatalogEntry& entry = m_entries[entryIndex];
            const QString targetPath = previewImageCachePathForEntry(entry);
            QDir().mkpath(QFileInfo(targetPath).absolutePath());

            QSaveFile targetFile(targetPath);
            if (!targetFile.open(QIODevice::WriteOnly) ||
                targetFile.write(response.payload) != response.payload.size() ||
                !targetFile.commit())
            {
                return;
            }

            entry.cachedPreviewImageSource =
                QUrl::fromLocalFile(targetPath).toString();
            notifyEntryUpdated(entryIndex, {PreviewImageUrlRole});
        });
    m_previewImageRequests.insert(entryIndex, request);
}
#endif

MarketplaceCatalogEntry MarketplaceCatalogModel::entryFromJson(
    const QJsonObject& object)
{
    MarketplaceCatalogEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString();
    entry.title = object.value(QStringLiteral("title")).toString();
    entry.detailUrl = object.value(QStringLiteral("detailUrl")).toString();
    entry.author = object.value(QStringLiteral("author")).toString();
    entry.isForHire = object.value(QStringLiteral("isForHire")).toBool();
    entry.license = object.value(QStringLiteral("license")).toString();
    entry.description = object.value(QStringLiteral("description")).toString();
    entry.previewImageUrl =
        object.value(QStringLiteral("previewImageUrl")).toString();
    entry.riveSource = object.value(QStringLiteral("riveSource")).toString();
    entry.availability =
        object.value(QStringLiteral("availability")).toString();
    entry.reactionCount =
        object.value(QStringLiteral("reactionCount")).toInt();

    const QJsonArray tags = object.value(QStringLiteral("tags")).toArray();
    for (const QJsonValue& value : tags)
    {
        entry.tags.append(value.toString());
    }

    return entry;
}

MarketplaceCatalogEntry MarketplaceCatalogModel::entryFromApiJson(
    const QJsonObject& object)
{
    MarketplaceCatalogEntry entry;

    const QJsonObject owner = object.value(QStringLiteral("owner")).toObject();
    const QJsonArray revisions =
        object.value(QStringLiteral("community_revisions")).toArray();
    if (revisions.isEmpty())
    {
        return entry;
    }

    const QJsonObject latestRevision = revisions.at(0).toObject();
    const QJsonArray communityFiles =
        latestRevision.value(QStringLiteral("community_files")).toArray();
    const QJsonObject latestFile =
        communityFiles.isEmpty() ? QJsonObject() : communityFiles.at(0).toObject();

    entry.id = latestRevision.value(QStringLiteral("slug")).toString();
    entry.title = latestRevision.value(QStringLiteral("title")).toString();
    entry.detailUrl = QStringLiteral("https://rive.app/marketplace/%1/")
                          .arg(entry.id);
    entry.author = owner.value(QStringLiteral("username")).toString();
    entry.isForHire = owner.value(QStringLiteral("is_for_hire")).toBool();
    entry.license = QStringLiteral("Unknown");
    entry.description =
        latestRevision.value(QStringLiteral("description")).toString();
    entry.previewImageUrl =
        latestRevision.value(QStringLiteral("watermark_thumbnail_url")).toString();
    if (entry.previewImageUrl.isEmpty())
    {
        entry.previewImageUrl =
            latestRevision.value(QStringLiteral("thumbnail_url")).toString();
    }
    entry.riveSource = latestFile.value(QStringLiteral("file_url")).toString();
    entry.availability = QStringLiteral("remote");
    entry.reactionCount = object.value(QStringLiteral("reaction_count")).toInt();

    const QJsonArray tags = latestRevision.value(QStringLiteral("tags")).toArray();
    for (const QJsonValue& value : tags)
    {
        entry.tags.append(value.toString());
    }

    return entry;
}
