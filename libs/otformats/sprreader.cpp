#include "sprreader.h"

#include <QFile>
#include <QPainter>
#include <QMutexLocker>
#include <QSaveFile>
#include <QDataStream>
#include <QStringList>
#include <algorithm>
#include <limits>

namespace {
void appendU16(QByteArray &data, quint16 value)
{
    data.append(static_cast<char>(value & 0xff));
    data.append(static_cast<char>((value >> 8) & 0xff));
}

QByteArray encodeSprite(const QImage &source, bool useAlpha, bool *tooLarge = nullptr)
{
    if (source.isNull()) return {};
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    bool hasPixels = false;
    for (int y = 0; y < image.height() && !hasPixels; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) != 0) { hasPixels = true; break; }
        }
    }
    if (!hasPixels) return {};

    QByteArray payload;
    int pixel = 0;
    const int spriteSize = image.width();
    const int pixelCount = spriteSize * image.height();
    while (pixel < pixelCount) {
        const int transparentStart = pixel;
        while (pixel < pixelCount && pixel - transparentStart < 65535
               && qAlpha(image.pixel(pixel % spriteSize, pixel / spriteSize)) == 0) ++pixel;
        const int transparent = pixel - transparentStart;
        const int coloredStart = pixel;
        while (pixel < pixelCount && pixel - coloredStart < 65535
               && qAlpha(image.pixel(pixel % spriteSize, pixel / spriteSize)) != 0) ++pixel;
        const int colored = pixel - coloredStart;
        appendU16(payload, static_cast<quint16>(transparent));
        appendU16(payload, static_cast<quint16>(colored));
        for (int i = coloredStart; i < pixel; ++i) {
            const QRgb rgba = image.pixel(i % spriteSize, i / spriteSize);
            payload.append(static_cast<char>(qRed(rgba)));
            payload.append(static_cast<char>(qGreen(rgba)));
            payload.append(static_cast<char>(qBlue(rgba)));
            if (useAlpha) payload.append(static_cast<char>(qAlpha(rgba)));
        }
    }
    if (payload.size() > 65535) {
        if (tooLarge) *tooLarge = true;
        return {};
    }

    QByteArray block;
    block.append(char(0xff));
    block.append(char(0x00));
    block.append(char(0xff));
    appendU16(block, static_cast<quint16>(payload.size()));
    block.append(payload);
    return block;
}
}

bool SpriteData::decode(const uchar *encodedData, qsizetype encodedSize,
                        int spriteSize, bool useAlpha)
{
    image = QImage(spriteSize, spriteSize, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    if (!encodedData || encodedSize == 0) {
        is_empty = true;
        return true;
    }

    const qint64 dataSize = encodedSize;
    const uchar *raw = encodedData;
    qint64 pos = 0;

    if (pos + 3 > dataSize) { is_empty = true; return false; }
    pos += 3;

    if (pos + 2 > dataSize) { is_empty = true; return false; }
    uint16_t compressedSize = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
    pos += 2;

    const qint64 dataStart = pos;
    const qint64 dataEnd = qMin(dataStart + static_cast<qint64>(compressedSize), dataSize);

    const int totalPixels = spriteSize * spriteSize;
    int pixelIndex = 0;

    uchar *dst = image.bits();
    const qsizetype stride = image.bytesPerLine();

    while (pos + 4 <= dataEnd && pixelIndex < totalPixels) {
        uint16_t transparentCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;
        uint16_t coloredCount = static_cast<uint16_t>(raw[pos]) | (static_cast<uint16_t>(raw[pos + 1]) << 8);
        pos += 2;

        pixelIndex = qMin(pixelIndex + static_cast<int>(transparentCount), totalPixels);

        const int bpp = useAlpha ? 4 : 3;
        for (int i = 0; i < coloredCount && pixelIndex < totalPixels; ++i) {
            if (pos + bpp > dataEnd) break;

            const int x = pixelIndex % spriteSize;
            const int y = pixelIndex / spriteSize;
            uchar *px = dst + static_cast<qsizetype>(y) * stride + static_cast<qsizetype>(x) * 4;
            px[0] = raw[pos];
            px[1] = raw[pos + 1];
            px[2] = raw[pos + 2];
            px[3] = useAlpha ? raw[pos + 3] : 255;
            pos += bpp;

            ++pixelIndex;
        }
    }

    is_empty = false;
    return true;
}

SprReader::SprReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

SprReader::~SprReader() = default;

void SprReader::reset()
{
    beginResetModel();
    if (m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
    if (m_file.isOpen()) m_file.close();
    m_file.setFileName(QString());
    m_bulkAccessDepth = 0;
    m_fileSize = 0;
    m_offsets.clear();
    m_signature = 0;
    m_spriteCount = 0;
    m_extended = false;
    m_cache.clear();
    m_cacheLru.clear();
    m_cacheBytes = 0;
    m_itemImageCache.clear();
    m_itemImageCacheLru.clear();
    m_itemImageCacheBytes = 0;
    m_modifiedSprites.clear();
    m_compactedBlocks.clear();
    m_loaded = false;
    m_dirty = false;
    m_filePath.clear();
    endResetModel();

    emit spriteCountChanged();
    emit loadedChanged();
    emit dirtyChanged();
}

void SprReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool SprReader::loadFile(const QString &path, quint32 expectedSignature, bool extended, bool useAlpha)
{
    reset();
    if (!m_errorString.isEmpty()) {
        m_errorString.clear();
        emit errorChanged();
    }
    m_extended = extended;
    m_useAlpha = useAlpha;

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }
    m_fileSize = m_file.size();

    const int headerCountSize = m_extended ? 4 : 2;
    const qint64 minHeaderSize = 4 + headerCountSize;

    if (m_fileSize < minHeaderSize) {
        setError(QStringLiteral("The file is too small to be a valid .spr file"));
        m_file.close();
        m_fileSize = 0;
        return false;
    }
    const QByteArray header = m_file.read(minHeaderSize);
    if (header.size() != minHeaderSize) {
        setError(QStringLiteral("Cannot read the .spr header"));
        m_file.close();
        m_fileSize = 0;
        return false;
    }
    const uchar *raw = reinterpret_cast<const uchar *>(header.constData());

    m_signature = static_cast<uint32_t>(raw[0])
                | (static_cast<uint32_t>(raw[1]) << 8)
                | (static_cast<uint32_t>(raw[2]) << 16)
                | (static_cast<uint32_t>(raw[3]) << 24);

    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Invalid .spr signature (expected 0x%1, got 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        reset();
        return false;
    }

    qint64 pos = 4;

    if (m_extended) {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8)
                       | (static_cast<uint32_t>(raw[pos + 2]) << 16)
                       | (static_cast<uint32_t>(raw[pos + 3]) << 24);
        pos += 4;
    } else {
        m_spriteCount = static_cast<uint32_t>(raw[pos])
                       | (static_cast<uint32_t>(raw[pos + 1]) << 8);
        pos += 2;
    }

    const qint64 offsetsStart = pos;
    const qint64 offsetsBytes = static_cast<qint64>(m_spriteCount) * 4;

    if (offsetsStart + offsetsBytes > m_fileSize) {
        setError(QStringLiteral("The file is corrupt: the offset table exceeds the file size"));
        reset();
        return false;
    }
    if (!m_file.seek(offsetsStart)) {
        setError(QStringLiteral("Cannot seek to the .spr offset table"));
        reset();
        return false;
    }
    const QByteArray offsetData = m_file.read(offsetsBytes);
    if (offsetData.size() != offsetsBytes) {
        setError(QStringLiteral("Cannot read the complete .spr offset table"));
        reset();
        return false;
    }
    const uchar *offsetRaw = reinterpret_cast<const uchar *>(offsetData.constData());

    m_offsets.reserve(static_cast<int>(m_spriteCount));

    for (uint32_t i = 0; i < m_spriteCount; ++i) {
        const qint64 p = static_cast<qint64>(i) * 4;
        uint32_t offset = static_cast<uint32_t>(offsetRaw[p])
                         | (static_cast<uint32_t>(offsetRaw[p + 1]) << 8)
                         | (static_cast<uint32_t>(offsetRaw[p + 2]) << 16)
                         | (static_cast<uint32_t>(offsetRaw[p + 3]) << 24);
        m_offsets.append(offset);
    }

    m_loaded = true;
    m_filePath = path;
    m_dirty = false;
    emit spriteCountChanged();
    emit loadedChanged();
    emit dirtyChanged();

    if (m_spriteCount > 0) {
        beginInsertRows(QModelIndex(), 0, static_cast<int>(m_spriteCount) - 1);
        endInsertRows();
    }

    return true;
}

std::shared_ptr<SpriteData> SprReader::loadSprite(uint32_t spriteId)
{
    return loadSpriteImpl(spriteId, true);
}

std::shared_ptr<SpriteData> SprReader::loadSpriteUncached(uint32_t spriteId)
{
    return loadSpriteImpl(spriteId, false);
}

void SprReader::beginBulkAccess()
{
    ++m_bulkAccessDepth;
    if (m_bulkAccessDepth == 1 && m_file.isOpen() && m_fileSize > 0)
        m_mappedData = m_file.map(0, m_fileSize);
}

void SprReader::endBulkAccess()
{
    if (m_bulkAccessDepth <= 0) return;
    --m_bulkAccessDepth;
    if (m_bulkAccessDepth == 0 && m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
}

std::shared_ptr<SpriteData> SprReader::loadSpriteImpl(uint32_t spriteId, bool cacheResult)
{
    if (spriteId < 1 || spriteId > m_spriteCount) {
        auto empty = std::make_shared<SpriteData>();
        empty->id = spriteId;
        empty->decode(nullptr, 0, m_spriteSize, m_useAlpha);
        return empty;
    }

    auto modified = m_modifiedSprites.constFind(spriteId);
    if (modified != m_modifiedSprites.cend()) {
        auto sprite = std::make_shared<SpriteData>();
        sprite->id = spriteId;
        sprite->image = modified.value();
        sprite->is_empty = sprite->image.isNull();
        if (sprite->image.isNull()) sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
        return sprite;
    }

    auto it = m_cache.find(spriteId);
    if (it != m_cache.end()) {
        m_cacheLru.splice(m_cacheLru.begin(), m_cacheLru, it->lru);
        return it->sprite;
    }

    auto sprite = decodeSprite(spriteId);
    if (cacheResult) cacheSprite(spriteId, sprite);
    return sprite;
}

void SprReader::clearImageCaches()
{
    m_cache.clear();
    m_cacheLru.clear();
    m_cacheBytes = 0;
    m_itemImageCache.clear();
    m_itemImageCacheLru.clear();
    m_itemImageCacheBytes = 0;
}

QByteArray SprReader::rawSpriteBlock(uint32_t spriteId)
{
    if(!m_compactedBlocks.isEmpty())
        return spriteId>=1 && spriteId<=uint32_t(m_compactedBlocks.size()) ? m_compactedBlocks.at(int(spriteId)-1) : QByteArray();
    if (spriteId < 1 || spriteId > static_cast<uint32_t>(m_offsets.size())) return {};
    const uint32_t offset = m_offsets.at(static_cast<int>(spriteId) - 1);
    if (offset == 0 || static_cast<qint64>(offset) + 5 > m_fileSize) return {};
    if (!m_file.seek(offset)) return {};
    QByteArray header = m_file.read(5);
    if (header.size() != 5) return {};
    const auto *raw = reinterpret_cast<const uchar *>(header.constData());
    const quint16 size = static_cast<quint16>(raw[3])
                       | static_cast<quint16>(raw[4] << 8);
    QByteArray payload = m_file.read(size);
    if (payload.size() != size) return {};
    return header + payload;
}

bool SprReader::replaceSprite(int spriteId, const QImage &image)
{
    if (!m_loaded || spriteId < 1 || spriteId > static_cast<int>(m_spriteCount)
        || image.size() != QSize(m_spriteSize, m_spriteSize)) {
        setError(QStringLiteral("Sprite image must be exactly %1x%1 pixels").arg(m_spriteSize));
        return false;
    }
    m_modifiedSprites.insert(static_cast<uint32_t>(spriteId),
                             image.convertToFormat(QImage::Format_RGBA8888));
    clearImageCaches();
    m_dirty = true;
    emit dirtyChanged();
    emit dataChanged(index(spriteId - 1), index(spriteId - 1));
    return true;
}

int SprReader::addSprite(const QImage &image)
{
    return addSprites(QVector<QImage>{image});
}

int SprReader::addSprites(const QVector<QImage> &images)
{
    if (images.isEmpty()) return 0;
    if (!m_loaded || std::any_of(images.cbegin(), images.cend(), [this](const QImage &image) {
            return image.size() != QSize(m_spriteSize, m_spriteSize);
        })) {
        setError(QStringLiteral("Sprite image must be exactly %1x%1 pixels").arg(m_spriteSize));
        return 0;
    }
    if (!m_extended && quint64(m_spriteCount) + quint64(images.size()) > 0xffffu) {
        setError(QStringLiteral("The non-extended SPR format is limited to 65535 sprites"));
        return 0;
    }
    const int firstId = static_cast<int>(m_spriteCount) + 1;
    beginInsertRows({}, firstId-1, firstId+images.size()-2);
    for (const QImage &image : images) {
        ++m_spriteCount;
        m_offsets.append(0);
        if(!m_compactedBlocks.isEmpty())m_compactedBlocks.append(QByteArray());
        m_modifiedSprites.insert(m_spriteCount, image.convertToFormat(QImage::Format_RGBA8888));
    }
    endInsertRows();
    clearImageCaches();
    m_dirty = true;
    emit spriteCountChanged();
    emit dirtyChanged();
    return firstId;
}

bool SprReader::hasSpriteData(quint32 spriteId) const
{
    if(!m_loaded || spriteId<1 || spriteId>m_spriteCount)return false;
    const auto modified=m_modifiedSprites.constFind(spriteId);
    if(modified!=m_modifiedSprites.cend())return !modified.value().isNull();
    if(!m_compactedBlocks.isEmpty())
        return spriteId<=quint32(m_compactedBlocks.size()) && !m_compactedBlocks.at(int(spriteId)-1).isEmpty();
    return spriteId<=quint32(m_offsets.size()) && m_offsets.at(int(spriteId)-1)!=0;
}

bool SprReader::removeSprite(int spriteId)
{
    if (!m_loaded || spriteId < 1 || spriteId > static_cast<int>(m_spriteCount)) return false;
    m_modifiedSprites.insert(static_cast<uint32_t>(spriteId), QImage());
    clearImageCaches();
    m_dirty = true;
    emit dirtyChanged();
    emit dataChanged(index(spriteId - 1), index(spriteId - 1));
    return true;
}
void SprReader::discardImportedSprites(const QSet<quint32> &ids, const QSet<quint32> &usedIds)
{
    if (!m_loaded || ids.isEmpty()) return;
    const uint32_t oldCount = m_spriteCount;
    uint32_t newCount = oldCount;
    while (newCount > 0 && ids.contains(newCount) && !usedIds.contains(newCount))
        --newCount;
    if (newCount < oldCount) {
        beginRemoveRows({}, int(newCount), int(oldCount) - 1);
        for (uint32_t id = newCount + 1; id <= oldCount; ++id)
            m_modifiedSprites.remove(id);
        m_offsets.resize(int(newCount));
        if (!m_compactedBlocks.isEmpty()) m_compactedBlocks.resize(int(newCount));
        m_spriteCount = newCount;
        endRemoveRows();
        emit spriteCountChanged();
    }
    int firstCleared = std::numeric_limits<int>::max();
    int lastCleared = -1;
    for (quint32 id : ids) {
        if (id < 1 || id > newCount || usedIds.contains(id)) continue;
        m_modifiedSprites.insert(id, QImage());
        firstCleared = qMin(firstCleared, int(id) - 1);
        lastCleared = qMax(lastCleared, int(id) - 1);
    }
    if (newCount == oldCount && lastCleared < 0) return;
    clearImageCaches();
    m_dirty = true;
    emit dirtyChanged();
    if (lastCleared >= 0) emit dataChanged(index(firstCleared), index(lastCleared));
}
int SprReader::clearSprites(const QSet<quint32> &ids) {
    if(!m_loaded)return 0;
    int count=0;
    for(const auto id:ids){
        if(id<1 || id>m_spriteCount)continue;
        m_modifiedSprites.insert(id,QImage());
        ++count;
    }
    if(count){clearImageCaches();m_dirty=true;emit dirtyChanged();}
    return count;
}
bool SprReader::compactSprites(const QList<quint32> &sourceIds) {
    if(!m_loaded || (!m_extended && sourceIds.size()>65535))return false;
    QVector<QByteArray> blocks;
    blocks.reserve(sourceIds.size());
    for(const auto sourceId:sourceIds){
        if(sourceId<1 || sourceId>m_spriteCount)return false;
        const auto modified=m_modifiedSprites.constFind(sourceId);
        if(modified==m_modifiedSprites.cend())blocks.append(rawSpriteBlock(sourceId));
        else {
            bool tooLarge=false;
            blocks.append(encodeSprite(modified.value(),m_useAlpha,&tooLarge));
            if(tooLarge)return false;
        }
    }
    beginResetModel();
    m_compactedBlocks=std::move(blocks);
    m_spriteCount=uint32_t(sourceIds.size());
    m_modifiedSprites.clear();
    clearImageCaches();
    m_dirty=true;
    endResetModel();
    emit spriteCountChanged();emit dirtyChanged();
    return true;
}

bool SprReader::saveFile(const QString &path, const std::function<void(int)> &progress)
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (!m_loaded || target.isEmpty()) {
        setError(QStringLiteral("No SPR target file selected"));
        return false;
    }

    QVector<QByteArray> blocks;
    blocks.reserve(static_cast<int>(m_spriteCount));
    if (progress) progress(0);
    for (uint32_t id = 1; id <= m_spriteCount; ++id) {
        const auto modified = m_modifiedSprites.constFind(id);
        if (modified == m_modifiedSprites.cend()) {
            blocks.append(rawSpriteBlock(id));
        } else {
            bool tooLarge = false;
            blocks.append(encodeSprite(modified.value(), m_useAlpha, &tooLarge));
            if (tooLarge) {
                setError(QStringLiteral("Sprite %1 is too large for the SPR format").arg(id));
                return false;
            }
        }
        if (progress && (id % 512 == 0 || id == m_spriteCount))
            progress(static_cast<int>(45.0 * id / qMax(1u, m_spriteCount)));
    }

    if (m_mappedData) {
        m_file.unmap(m_mappedData);
        m_mappedData = nullptr;
    }
    m_bulkAccessDepth = 0;
    m_file.close();
    const auto reopenSource = [this]() -> bool {
        if (!m_file.isOpen() && !m_filePath.isEmpty()) return m_file.open(QIODevice::ReadOnly);
        return m_file.isOpen();
    };

    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Cannot write SPR file: %1").arg(target));
        (void)reopenSource();
        return false;
    }
    QDataStream stream(&output);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << static_cast<quint32>(m_signature);
    if (m_extended) stream << static_cast<quint32>(m_spriteCount);
    else stream << static_cast<quint16>(m_spriteCount);
    const qint64 offsetsPosition = output.pos();
    for (uint32_t id = 0; id < m_spriteCount; ++id) stream << quint32(0);

    QVector<quint32> offsets(static_cast<int>(m_spriteCount), 0);
    for (int i = 0; i < blocks.size(); ++i) {
        if (progress && ((i + 1) % 512 == 0 || i + 1 == blocks.size()))
            progress(45 + static_cast<int>(50.0 * (i + 1) / qMax(1, blocks.size())));
        if (blocks.at(i).isEmpty()) continue;
        if (output.pos() > std::numeric_limits<quint32>::max()) {
            output.cancelWriting();
            setError(QStringLiteral("SPR file exceeds the 4 GiB offset limit"));
            (void)reopenSource();
            return false;
        }
        offsets[i] = static_cast<quint32>(output.pos());
        if (output.write(blocks.at(i)) != blocks.at(i).size()) {
            output.cancelWriting();
            setError(QStringLiteral("Failed while writing sprite %1").arg(i + 1));
            (void)reopenSource();
            return false;
        }
    }
    if (!output.seek(offsetsPosition)) {
        output.cancelWriting();
        setError(QStringLiteral("Cannot write the SPR offset table"));
        (void)reopenSource();
        return false;
    }
    for (quint32 offset : offsets) stream << offset;
    if (stream.status() != QDataStream::Ok || !output.commit()) {
        setError(QStringLiteral("Failed to commit SPR file: %1").arg(target));
        (void)reopenSource();
        return false;
    }
    const bool reloaded = loadFile(target, m_signature, m_extended, m_useAlpha);
    if (reloaded && progress) progress(100);
    return reloaded;
}

std::shared_ptr<SpriteData> SprReader::decodeSprite(uint32_t spriteId)
{
    auto sprite = std::make_shared<SpriteData>();
    sprite->id = spriteId;

    if(!m_compactedBlocks.isEmpty()){
        const QByteArray block=m_compactedBlocks.value(int(spriteId)-1);
        sprite->decode(reinterpret_cast<const uchar *>(block.constData()),block.size(),m_spriteSize,m_useAlpha);
        return sprite;
    }

    const uint32_t offset = m_offsets.at(static_cast<int>(spriteId) - 1);
    if (offset == 0) {
        sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
        return sprite;
    }

    if (static_cast<qint64>(offset) + 5 > m_fileSize) {
        sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
        return sprite;
    }

    QByteArray encoded;
    const uchar *raw = nullptr;
    if (m_mappedData) {
        raw = m_mappedData + offset;
    } else {
        if (!m_file.seek(static_cast<qint64>(offset))) {
            sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
            return sprite;
        }
        encoded = m_file.read(5);
        if (encoded.size() != 5) {
            sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
            return sprite;
        }
        raw = reinterpret_cast<const uchar *>(encoded.constData());
    }
    const uint16_t compressedSize = static_cast<uint16_t>(raw[3])
                                  | (static_cast<uint16_t>(raw[4]) << 8);
    const qint64 encodedSize = 5 + static_cast<qint64>(compressedSize);
    if (static_cast<qint64>(offset) + encodedSize > m_fileSize) {
        sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
        return sprite;
    }
    if (!m_mappedData) {
        const QByteArray pixels = m_file.read(compressedSize);
        if (pixels.size() != compressedSize) {
            sprite->decode(nullptr, 0, m_spriteSize, m_useAlpha);
            return sprite;
        }
        encoded.append(pixels);
        raw = reinterpret_cast<const uchar *>(encoded.constData());
    }
    sprite->decode(raw, encodedSize, m_spriteSize, m_useAlpha);
    return sprite;
}

void SprReader::cacheSprite(uint32_t spriteId, const std::shared_ptr<SpriteData> &sprite)
{
    const qsizetype bytes = sprite && !sprite->image.isNull()
                                ? sprite->image.sizeInBytes() + static_cast<qsizetype>(sizeof(SpriteData))
                                : static_cast<qsizetype>(sizeof(SpriteData));

    m_cacheLru.push_front(spriteId);
    m_cache.insert(spriteId, SpriteCacheEntry{sprite, m_cacheLru.begin(), bytes});
    m_cacheBytes += bytes;

    while (m_cacheBytes > kMaxSpriteCacheBytes && m_cacheLru.size() > 1) {
        const uint32_t evictedId = m_cacheLru.back();
        auto evicted = m_cache.find(evictedId);
        if (evicted != m_cache.end()) {
            m_cacheBytes -= evicted->bytes;
            m_cache.erase(evicted);
        }
        m_cacheLru.pop_back();
    }
}

QImage SprReader::spriteImage(int spriteId)
{
    auto sprite = loadSprite(static_cast<uint32_t>(spriteId));
    return sprite->image;
}

QString SprReader::spriteImageSource(int spriteId)
{
    return QStringLiteral("image://itempreview/sprite/%1").arg(spriteId);
}

QString SprReader::itemImageSource(const QVariantList &spriteIds,
                                   int itemWidth,
                                   int itemHeight,
                                   int layers)
{
    const int width = qMax(1, itemWidth);
    const int height = qMax(1, itemHeight);
    const int layerCount = qMax(1, layers);

    if (spriteIds.isEmpty()) {
        return spriteImageSource(0);
    }

    const int previewSpriteCount = qMin(spriteIds.size(), width * height * layerCount);
    QStringList previewIds;
    previewIds.reserve(previewSpriteCount);
    for (int i = 0; i < previewSpriteCount; ++i) {
        previewIds.append(QString::number(spriteIds.at(i).toUInt()));
    }

    return QStringLiteral("image://itempreview/item/%1/%2/%3/%4")
        .arg(width)
        .arg(height)
        .arg(layerCount)
        .arg(previewIds.join(QLatin1Char(',')));
}

QImage SprReader::imageForProviderId(const QString &id)
{
    const QMutexLocker locker(&m_imageMutex);

    if (id.startsWith(QStringLiteral("sprite/"))) {
        bool ok = false;
        const uint32_t spriteId = id.mid(7).toUInt(&ok);
        return ok ? spriteImage(static_cast<int>(spriteId)) : QImage();
    }

    auto cached = m_itemImageCache.find(id);
    if (cached != m_itemImageCache.end()) {
        m_itemImageCacheLru.splice(m_itemImageCacheLru.begin(),
                                   m_itemImageCacheLru,
                                   cached->lru);
        return cached->image;
    }

    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 5 || parts.at(0) != QStringLiteral("item")) {
        return QImage();
    }

    bool widthOk = false;
    bool heightOk = false;
    bool layersOk = false;
    const int width = qBound(1, parts.at(1).toInt(&widthOk), 32);
    const int height = qBound(1, parts.at(2).toInt(&heightOk), 32);
    const int layerCount = qBound(1, parts.at(3).toInt(&layersOk), 32);
    if (!widthOk || !heightOk || !layersOk) {
        return QImage();
    }

    const QStringList spriteIdParts = parts.at(4).split(QLatin1Char(','),
                                                        Qt::SkipEmptyParts);
    QImage composite(width * m_spriteSize,
                     height * m_spriteSize,
                     QImage::Format_RGBA8888);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (int layer = 0; layer < layerCount; ++layer) {
        for (int h = 0; h < height; ++h) {
            for (int w = 0; w < width; ++w) {
                const int spriteIndex = ((layer * height) + h) * width + w;
                if (spriteIndex < 0 || spriteIndex >= spriteIdParts.size()) {
                    continue;
                }

                const uint32_t spriteId = spriteIdParts.at(spriteIndex).toUInt();
                if (spriteId == 0) {
                    continue;
                }

                auto sprite = loadSprite(spriteId);
                if (!sprite || sprite->image.isNull()) {
                    continue;
                }

                const int destX = (width - w - 1) * m_spriteSize;
                const int destY = (height - h - 1) * m_spriteSize;
                painter.drawImage(destX, destY, sprite->image);
            }
        }
    }

    painter.end();

    cacheItemImage(id, composite);
    return composite;
}

void SprReader::cacheItemImage(const QString &key, const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    const qsizetype bytes = image.sizeInBytes();
    m_itemImageCacheLru.push_front(key);
    m_itemImageCache.insert(key, ItemImageCacheEntry{image, m_itemImageCacheLru.begin(), bytes});
    m_itemImageCacheBytes += bytes;

    while (m_itemImageCacheBytes > kMaxItemImageCacheBytes
           && m_itemImageCacheLru.size() > 1) {
        const QString evictedKey = m_itemImageCacheLru.back();
        auto evicted = m_itemImageCache.find(evictedKey);
        if (evicted != m_itemImageCache.end()) {
            m_itemImageCacheBytes -= evicted->bytes;
            m_itemImageCache.erase(evicted);
        }
        m_itemImageCacheLru.pop_back();
    }
}

int SprReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_spriteCount);
}

QVariant SprReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_spriteCount)) {
        return QVariant();
    }

    const int spriteId = index.row() + 1;

    auto *self = const_cast<SprReader *>(this);

    switch (role) {
    case SpriteIdRole:
        return spriteId;
    case SpriteImageRole:
        return self->spriteImage(spriteId);
    case SpriteImageSourceRole:
        return self->spriteImageSource(spriteId);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SprReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SpriteIdRole] = "spriteId";
    roles[SpriteImageRole] = "spriteImage";
    roles[SpriteImageSourceRole] = "spriteImageSource";
    return roles;
}
