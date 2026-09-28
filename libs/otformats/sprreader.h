#ifndef SPRREADER_H
#define SPRREADER_H

#include <QAbstractListModel>
#include <QImage>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QList>
#include <QMutex>
#include <QString>
#include <QFile>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <list>
#include <memory>
#include <cstdint>
#include <functional>

struct SpriteData {
    uint32_t id = 0;
    bool is_empty = true;
    QImage image;

    bool decode(const uchar *encodedData, qsizetype encodedSize,
                int spriteSize, bool useAlpha = false);
};

class SprReader : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY spriteCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY loadedChanged)

public:
    enum SpriteRoles {
        SpriteIdRole = Qt::UserRole + 1,
        SpriteImageRole,
        SpriteImageSourceRole
    };

    explicit SprReader(QObject *parent = nullptr);
    ~SprReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int spriteCount() const { return static_cast<int>(m_spriteCount); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }
    bool isDirty() const { return m_dirty; }
    QString filePath() const { return m_filePath; }
    quint32 signature() const { return m_signature; }
    bool extended() const { return m_extended; }
    bool usesAlpha() const { return m_useAlpha; }
    int spriteSize() const { return m_spriteSize; }
    void setSpriteSize(int size) { if (size == 32 || size == 64 || size == 128 || size == 256) m_spriteSize = size; }

    Q_INVOKABLE bool loadFile(const QString &path,
                               quint32 expectedSignature = 0,
                               bool extended = false,
                               bool useAlpha = false);
    bool saveFile(const QString &path = QString(), const std::function<void(int)> &progress = {});
    int clearSprites(const QSet<quint32> &ids);
    bool compactSprites(const QList<quint32> &sourceIds);
    Q_INVOKABLE bool replaceSprite(int spriteId, const QImage &image);
    Q_INVOKABLE int addSprite(const QImage &image);
    int addSprites(const QVector<QImage> &images);
    bool hasSpriteData(quint32 spriteId) const;
    // Keeps IDs stable: the entry becomes an empty sprite instead of shifting IDs.
    Q_INVOKABLE bool removeSprite(int spriteId);
    void discardImportedSprites(const QSet<quint32> &ids, const QSet<quint32> &usedIds);

    Q_INVOKABLE QString toLocalFile(const QUrl &url) const { return url.toLocalFile(); }

    std::shared_ptr<SpriteData> loadSprite(uint32_t spriteId);
    std::shared_ptr<SpriteData> loadSpriteUncached(uint32_t spriteId);
    void beginBulkAccess();
    void endBulkAccess();

    Q_INVOKABLE QImage spriteImage(int spriteId);

    Q_INVOKABLE QString spriteImageSource(int spriteId);

    Q_INVOKABLE QString itemImageSource(const QVariantList &spriteIds,
                                        int itemWidth,
                                        int itemHeight,
                                        int layers);

    QImage imageForProviderId(const QString &id);

signals:
    void spriteCountChanged();
    void loadedChanged();
    void errorChanged();
    void dirtyChanged();

private:
    int m_spriteSize = 32;

    static constexpr qsizetype kMaxSpriteCacheBytes = 8 * 1024 * 1024;
    static constexpr qsizetype kMaxItemImageCacheBytes = 32 * 1024 * 1024;

    void setError(const QString &message);
    void reset();
    std::shared_ptr<SpriteData> loadSpriteImpl(uint32_t spriteId, bool cacheResult);
    std::shared_ptr<SpriteData> decodeSprite(uint32_t spriteId);
    void cacheSprite(uint32_t spriteId, const std::shared_ptr<SpriteData> &sprite);
    void cacheItemImage(const QString &key, const QImage &image);
    QByteArray rawSpriteBlock(uint32_t spriteId);
    void clearImageCaches();

    QFile m_file;
    uchar *m_mappedData = nullptr;
    int m_bulkAccessDepth = 0;
    qint64 m_fileSize = 0;
    QVector<uint32_t> m_offsets;
    uint32_t m_signature = 0;
    uint32_t m_spriteCount = 0;
    bool m_extended = false;
    bool m_useAlpha = false;
    bool m_loaded = false;
    bool m_dirty = false;
    QString m_errorString;
    QString m_filePath;
    QHash<uint32_t, QImage> m_modifiedSprites;
    QVector<QByteArray> m_compactedBlocks;

    struct SpriteCacheEntry {
        std::shared_ptr<SpriteData> sprite;
        std::list<uint32_t>::iterator lru;
        qsizetype bytes = 0;
    };

    QHash<uint32_t, SpriteCacheEntry> m_cache;
    std::list<uint32_t> m_cacheLru;
    qsizetype m_cacheBytes = 0;

    struct ItemImageCacheEntry {
        QImage image;
        std::list<QString>::iterator lru;
        qsizetype bytes = 0;
    };

    QHash<QString, ItemImageCacheEntry> m_itemImageCache;
    std::list<QString> m_itemImageCacheLru;
    qsizetype m_itemImageCacheBytes = 0;
    QMutex m_imageMutex;
};

#endif
