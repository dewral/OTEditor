#include "editorbackend.h"

#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QTransform>
#include <algorithm>

namespace {
constexpr int MaxTiles = 100000;

QString tileFileName(int row, int column) {
    return QStringLiteral("tile_%1_%2.png")
        .arg(row, 4, 10, QChar('0'))
        .arg(column, 4, 10, QChar('0'));
}

bool hasVisiblePixels(const QImage &image) {
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) return true;
        }
    }
    return false;
}
} // namespace

int EditorBackend::sliceImage(const QString &sourceUrl, const QString &folderUrl, int tileSize) {
    if (tileSize < 1 || tileSize > 256) return 0;
    const QImage source(path(sourceUrl));
    if (source.isNull()) {
        message(QStringLiteral("Cannot read image to slice"));
        return 0;
    }
    const QDir folder(path(folderUrl));
    if (!folder.exists()) {
        message(QStringLiteral("Output folder does not exist"));
        return 0;
    }
    const int columns = (source.width() + tileSize - 1) / tileSize;
    const int rows = (source.height() + tileSize - 1) / tileSize;
    if (quint64(columns) * rows > MaxTiles) {
        message(QStringLiteral("Too many tiles"));
        return 0;
    }
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const QString name = tileFileName(row, column);
            if (QFileInfo::exists(folder.filePath(name))) {
                message(QStringLiteral("Output already contains %1; choose an empty folder").arg(name));
                return 0;
            }
        }
    }
    int count = 0;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            QImage tile(tileSize, tileSize, QImage::Format_RGBA8888);
            tile.fill(Qt::transparent);
            QPainter painter(&tile);
            painter.drawImage(0, 0, source, column * tileSize, row * tileSize, tileSize, tileSize);
            painter.end();
            const QString name = tileFileName(row, column);
            if (!tile.save(folder.filePath(name), "PNG")) {
                message(QStringLiteral("Failed to write tile %1").arg(name));
                return count;
            }
            ++count;
        }
    }
    message(QStringLiteral("Sliced image into %1 tile(s)").arg(count));
    return count;
}
int EditorBackend::importSheetSprites(const QString &sourceUrl, bool skipEmpty) {
    if (!loaded() || m_compiling) return 0;
    const QImage source(path(sourceUrl));
    if (source.isNull()) {
        message(QStringLiteral("Cannot read sprite sheet"));
        return 0;
    }
    const int size = m_project.spriteSize();
    const int columns = (source.width() + size - 1) / size;
    const int rows = (source.height() + size - 1) / size;
    if (quint64(columns) * rows > MaxTiles) {
        message(QStringLiteral("Too many sprites in sheet"));
        return 0;
    }
    int count = 0;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            QImage tile(size, size, QImage::Format_RGBA8888);
            tile.fill(Qt::transparent);
            QPainter painter(&tile);
            painter.drawImage(0, 0, source, column * size, row * size, size, size);
            painter.end();
            if (skipEmpty && !hasVisiblePixels(tile)) continue;
            if (m_project.sprites()->addSprite(tile) < 1) {
                message(m_project.sprites()->errorString());
                return count;
            }
            ++count;
        }
    }
    if (count) {
        ++m_revision;
        emit changed();
    }
    message(QStringLiteral("Imported %1 sprite(s) from sheet").arg(count));
    return count;
}
bool EditorBackend::slicerOpen(const QString &sourceUrl) {
    QImage source(path(sourceUrl));
    if (source.isNull()) {
        message(QStringLiteral("Cannot read slicer image"));
        return false;
    }
    m_slicerImage = source.convertToFormat(QImage::Format_ARGB32);
    m_slicerTiles.clear();
    m_slicerLastCut.clear();
    ++m_slicerRevision;
    emit slicerChanged();
    message(QStringLiteral("Slicer opened %1 x %2 image").arg(source.width()).arg(source.height()));
    return true;
}
bool EditorBackend::slicerTransform(const QString &operation) {
    if (m_slicerImage.isNull()) return false;
    if (operation == QLatin1String("rotateRight")) {
        m_slicerImage = m_slicerImage.transformed(QTransform().rotate(90));
    } else if (operation == QLatin1String("rotateLeft")) {
        m_slicerImage = m_slicerImage.transformed(QTransform().rotate(-90));
    } else if (operation == QLatin1String("flipHorizontal")) {
        m_slicerImage = m_slicerImage.flipped(Qt::Horizontal);
    } else if (operation == QLatin1String("flipVertical")) {
        m_slicerImage = m_slicerImage.flipped(Qt::Vertical);
    } else {
        return false;
    }
    m_slicerLastCut.clear();
    ++m_slicerRevision;
    emit slicerChanged();
    return true;
}
int EditorBackend::slicerCut(int offsetX, int offsetY, int columns, int rows,
                             int tileSize, bool includeEmpty) {
    if (m_slicerImage.isNull() || tileSize < 1 || tileSize > 256 ||
        columns < 1 || columns > 20 || rows < 1 || rows > 20 ||
        offsetX < 0 || offsetY < 0 ||
        offsetX + columns * tileSize > m_slicerImage.width() ||
        offsetY + rows * tileSize > m_slicerImage.height()) {
        message(QStringLiteral("Slicer selection must fit inside the image"));
        return 0;
    }
    const QString key = QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(offsetX).arg(offsetY).arg(columns).arg(rows).arg(tileSize).arg(includeEmpty);
    if (!m_slicerTiles.isEmpty() && m_slicerLastCut == key) return 0;
    QVector<QImage> tiles;
    tiles.reserve(columns * rows);
    // ObjectBuilder walks columns first, then rows; this is also the import order.
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            QImage tile = m_slicerImage.copy(offsetX + column * tileSize,
                                              offsetY + row * tileSize, tileSize, tileSize);
            bool visible = false;
            for (int y = 0; y < tileSize; ++y) {
                QRgb *pixels = reinterpret_cast<QRgb *>(tile.scanLine(y));
                for (int x = 0; x < tileSize; ++x) {
                    if (pixels[x] == qRgba(255, 0, 255, 255)) {
                        pixels[x] = qRgba(0, 0, 0, 0);
                    } else if (qAlpha(pixels[x]) > 0) {
                        visible = true;
                    }
                }
            }
            if (includeEmpty || visible) tiles.append(std::move(tile));
        }
    }
    const int added = tiles.size();
    m_slicerTiles += tiles;
    m_slicerLastCut = key;
    ++m_slicerRevision;
    emit slicerChanged();
    message(QStringLiteral("Cut %1 sprite(s); %2 in slicer list").arg(added).arg(m_slicerTiles.size()));
    return added;
}
void EditorBackend::slicerClear() {
    m_slicerTiles.clear();
    m_slicerLastCut.clear();
    ++m_slicerRevision;
    emit slicerChanged();
}
int EditorBackend::slicerImport() {
    if (!loaded() || m_compiling || m_slicerTiles.isEmpty()) return 0;
    const QSize spriteSize(m_project.spriteSize(), m_project.spriteSize());
    const bool wrongSize = std::any_of(m_slicerTiles.cbegin(), m_slicerTiles.cend(),
                                       [spriteSize](const QImage &image) { return image.size() != spriteSize; });
    if (wrongSize) {
        message(QStringLiteral("Cut sprites must match the client sprite dimension (%1 x %1)")
                    .arg(m_project.spriteSize()));
        return 0;
    }
    if (!m_project.sprites()->addSprites(m_slicerTiles)) {
        message(m_project.sprites()->errorString());
        return 0;
    }
    const int count = m_slicerTiles.size();
    ++m_revision;
    emit changed();
    message(QStringLiteral("Imported %1 slicer sprite(s)").arg(count));
    return count;
}
