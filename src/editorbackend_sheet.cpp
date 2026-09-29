#include "editorbackend.h"

#include <QImageReader>
#include <QtGlobal>
#include <climits>

namespace {

struct SheetGeometry {
    int width = 1;
    int height = 1;
    int layers = 1;
    int patternX = 1;
    int patternY = 1;
    int patternZ = 1;
    int frames = 1;
    std::vector<uint32_t> *spriteIds = nullptr;
};

bool geometryFor(ClientItem &item, int category, int groupIndex, SheetGeometry &out)
{
    if (category == 1 && !item.frame_groups.empty()) {
        if (groupIndex < 0 || groupIndex >= int(item.frame_groups.size()))
            return false;
        auto &group = item.frame_groups[size_t(groupIndex)];
        out = {group.width, group.height, group.layers, group.pattern_x,
               group.pattern_y, group.pattern_z, group.frames, &group.sprite_ids};
        return true;
    }
    if (groupIndex != 0)
        return false;
    out = {item.width, item.height, item.layers, item.pattern_x,
           item.pattern_y, item.pattern_z, item.frames, &item.sprite_ids};
    return true;
}

qint64 sheetSpriteCount(const SheetGeometry &g)
{
    return qint64(g.width) * g.height * g.layers * g.patternX
         * g.patternY * g.patternZ * g.frames;
}

int spriteIndex(const SheetGeometry &g, int frame, int pattern, int layer, int x, int y)
{
    if (frame < 0 || frame >= g.frames || pattern < 0
        || pattern >= g.patternX * g.patternY * g.patternZ
        || layer < 0 || layer >= g.layers || x < 0 || x >= g.width
        || y < 0 || y >= g.height)
        return -1;
    return (((frame * g.patternZ * g.patternY * g.patternX + pattern)
             * g.layers + layer) * g.height + y) * g.width + x;
}

QSize groupSheetSize(const SheetGeometry &g, int tileSize)
{
    const qint64 width = qint64(g.patternZ) * g.patternX * g.layers * g.width * tileSize;
    const qint64 height = qint64(g.frames) * g.patternY * g.height * tileSize;
    if (width > INT_MAX || height > INT_MAX)
        return {};
    return QSize(int(width), int(height));
}

void syncDefaultGroup(ClientItem &item)
{
    if (item.frame_groups.empty())
        return;
    const auto &group = item.frame_groups.front();
    item.width = group.width;
    item.height = group.height;
    item.layers = group.layers;
    item.pattern_x = group.pattern_x;
    item.pattern_y = group.pattern_y;
    item.pattern_z = group.pattern_z;
    item.frames = group.frames;
    item.sprite_ids = group.sprite_ids;
    item.animation_data = group.animation_data;
}

void removeMagenta(QImage &image)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto *pixels = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((pixels[x] & 0x00ffffffu) == 0x00ff00ffu)
                pixels[x] = 0;
        }
    }
}

} // namespace

quint64 EditorBackend::textureEditKey() const
{
    return (quint64(quint32(m_category)) << 32) | quint32(m_selected);
}

bool EditorBackend::textureEditPending() const
{
    return loaded() && m_selected >= 0 && m_textureEdits.contains(textureEditKey());
}

void EditorBackend::stageTextureEdit(const ClientItem &before)
{
    if (!textureEditPending())
        m_textureEdits.insert(textureEditKey(), before);
    m_redo.clear();
    refresh();
}

bool EditorBackend::saveTextureEdit()
{
    if (!textureEditPending())
        return false;
    const auto *current = m_project.dat()->objectAt(m_category, m_selected);
    if (!current)
        return false;
    m_undo.push_back({m_category, m_selected, m_textureEdits.take(textureEditKey()), *current});
    m_textureImportedSprites.remove(textureEditKey());
    if (m_undo.size() > 100)
        m_undo.erase(m_undo.begin());
    refresh();
    message(QStringLiteral("Saved texture changes for object %1.").arg(current->id));
    return true;
}

bool EditorBackend::resetTextureEdit()
{
    if (!textureEditPending())
        return false;
    const ClientItem before = m_textureEdits.take(textureEditKey());
    const QSet<quint32> imported = m_textureImportedSprites.take(textureEditKey());
    m_project.dat()->restoreObject(m_category, m_selected, before);
    m_project.sprites()->discardImportedSprites(imported, m_project.dat()->usedSpriteIds());
    refresh();
    message(QStringLiteral("Reset texture changes for object %1.").arg(before.id));
    return true;
}

void EditorBackend::commitTextureEdits()
{
    if (m_textureEdits.isEmpty())
        return;
    for (auto it = m_textureEdits.cbegin(); it != m_textureEdits.cend(); ++it) {
        const int category = int(it.key() >> 32);
        const int row = int(quint32(it.key()));
        const auto *current = m_project.dat()->objectAt(category, row);
        if (current && current->id == it.value().id)
            m_undo.push_back({category, row, it.value(), *current});
    }
    if (m_undo.size() > 100)
        m_undo.erase(m_undo.begin(), m_undo.end() - 100);
    m_textureEdits.clear();
    m_textureImportedSprites.clear();
    refresh();
}

bool EditorBackend::assignSpriteToCell(int groupIndex, int frame, int pattern,
                                       int layer, int tileX, int tileY, int spriteId)
{
    if (!loaded() || m_selected < 0 || spriteId < 0 || spriteId > spriteCount())
        return false;
    const auto *current = m_project.dat()->objectAt(m_category, m_selected);
    if (!current)
        return false;
    const ClientItem before = *current;
    ClientItem edited = before;
    SheetGeometry geometry;
    if (!geometryFor(edited, m_category, groupIndex, geometry))
        return false;
    const int index = spriteIndex(geometry, frame, pattern, layer, tileX, tileY);
    if (index < 0 || index >= int(geometry.spriteIds->size()))
        return false;
    if ((*geometry.spriteIds)[size_t(index)] == uint32_t(spriteId))
        return true;

    (*geometry.spriteIds)[size_t(index)] = uint32_t(spriteId);
    if (m_category == 1 && groupIndex == 0)
        syncDefaultGroup(edited);
    edited.modified = true;
    m_project.dat()->restoreObject(m_category, m_selected, edited);
    stageTextureEdit(before);
    message(QStringLiteral("Assigned sprite %1 to object %2").arg(spriteId).arg(edited.id));
    return true;
}

bool EditorBackend::importObjectImage(const QString &fileUrl, int groupIndex, int frame,
                                      int pattern, int layer, int tileX, int tileY)
{
    if (!loaded() || m_selected < 0) {
        message(QStringLiteral("Select an object before dropping an image."));
        return false;
    }
    const auto *current = m_project.dat()->objectAt(m_category, m_selected);
    if (!current)
        return false;

    QImageReader reader(path(fileUrl));
    reader.setAutoTransform(true);
    const QSize headerSize = reader.size();
    constexpr qint64 maxPixels = 16ll * 1024 * 1024;
    if (headerSize.isValid() && qint64(headerSize.width()) * headerSize.height() > maxPixels) {
        message(QStringLiteral("Image is too large to import (limit: 16 million pixels)."));
        return false;
    }
    QImage image = reader.read();
    if (image.isNull() || qint64(image.width()) * image.height() > maxPixels) {
        message(QStringLiteral("Cannot read the dropped image or it is too large: %1")
                    .arg(reader.errorString()));
        return false;
    }
    const int tileSize = m_project.spriteSize();
    if (image.width() % tileSize || image.height() % tileSize) {
        message(QStringLiteral("Image dimensions must be multiples of %1 pixels.").arg(tileSize));
        return false;
    }
    removeMagenta(image);

    const ClientItem before = *current;
    ClientItem edited = before;
    SheetGeometry active;
    if (!geometryFor(edited, m_category, groupIndex, active))
        return false;

    // A single tile targets the cell under the pointer, including its pattern and frame.
    if (image.size() == QSize(tileSize, tileSize)) {
        const int index = spriteIndex(active, frame, pattern, layer, tileX, tileY);
        if (index < 0 || index >= int(active.spriteIds->size()))
            return false;
        const int id = m_project.sprites()->addSprite(image);
        if (id < 1) {
            message(m_project.sprites()->errorString());
            return false;
        }
        (*active.spriteIds)[size_t(index)] = uint32_t(id);
        if (m_category == 1 && groupIndex == 0)
            syncDefaultGroup(edited);
        edited.modified = true;
        m_project.dat()->restoreObject(m_category, m_selected, edited);
        stageTextureEdit(before);
        m_textureImportedSprites[textureEditKey()].insert(quint32(id));
        message(QStringLiteral("Imported image into object %1, sprite %2").arg(edited.id).arg(id));
        return true;
    }

    const QSize activeSize = groupSheetSize(active, tileSize);
    bool combined = false;
    int combinedWidth = 0;
    int combinedHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    int totalX = 0;
    if (m_category == 1 && edited.frame_groups.size() > 1) {
        qint64 totalY = 0;
        for (int g = 0; g < int(edited.frame_groups.size()); ++g) {
            SheetGeometry part;
            geometryFor(edited, m_category, g, part);
            maxWidth = qMax(maxWidth, part.width);
            maxHeight = qMax(maxHeight, part.height);
            totalX = qMax(totalX, part.patternZ * part.patternX * part.layers);
            totalY += qint64(part.frames) * part.patternY;
        }
        const qint64 w = qint64(totalX) * maxWidth * tileSize;
        const qint64 h = totalY * maxHeight * tileSize;
        if (w <= INT_MAX && h <= INT_MAX) {
            combinedWidth = int(w);
            combinedHeight = int(h);
            combined = image.size() == QSize(combinedWidth, combinedHeight);
        }
    }

    bool detectedSize = false;
    if (image.size() != activeSize && !combined
        && active.layers == 1 && active.patternX == 1 && active.patternY == 1
        && active.patternZ == 1 && active.frames == 1) {
        const int width = image.width() / tileSize;
        const int height = image.height() / tileSize;
        if (width >= 1 && width <= 32 && height >= 1 && height <= 32) {
            if (m_category == 1 && !edited.frame_groups.empty()) {
                auto &group = edited.frame_groups[size_t(groupIndex)];
                group.width = uint8_t(width);
                group.height = uint8_t(height);
            } else {
                edited.width = uint8_t(width);
                edited.height = uint8_t(height);
            }
            geometryFor(edited, m_category, groupIndex, active);
            detectedSize = true;
        }
    }
    if (!combined && image.size() != groupSheetSize(active, tileSize)) {
        message(QStringLiteral("Image is %1×%2; expected %3×%4 for this group%5.")
                    .arg(image.width()).arg(image.height())
                    .arg(activeSize.width()).arg(activeSize.height())
                    .arg(combinedWidth > 0
                             ? QStringLiteral(" or %1×%2 for all groups")
                                   .arg(combinedWidth).arg(combinedHeight)
                             : QString()));
        return false;
    }

    QVector<QImage> tiles;
    const int firstGroup = combined ? 0 : groupIndex;
    const int lastGroup = combined ? int(edited.frame_groups.size()) : groupIndex + 1;
    qint64 tileCount = 0;
    for (int g = firstGroup; g < lastGroup; ++g) {
        SheetGeometry part;
        geometryFor(edited, m_category, g, part);
        tileCount += sheetSpriteCount(part);
    }
    if (tileCount < 1 || tileCount > 4096) {
        message(QStringLiteral("The sheet must contain between 1 and 4096 sprites."));
        return false;
    }
    tiles.reserve(int(tileCount));
    int groupY = 0;
    for (int g = firstGroup; g < lastGroup; ++g) {
        SheetGeometry part;
        geometryFor(edited, m_category, g, part);
        const int columns = combined ? totalX : part.patternZ * part.patternX * part.layers;
        const int cellWidth = (combined ? maxWidth : part.width) * tileSize;
        const int cellHeight = (combined ? maxHeight : part.height) * tileSize;
        for (int f = 0; f < part.frames; ++f)
            for (int z = 0; z < part.patternZ; ++z)
                for (int y = 0; y < part.patternY; ++y)
                    for (int x = 0; x < part.patternX; ++x)
                        for (int l = 0; l < part.layers; ++l) {
                            const int texture = ((((f * part.patternZ + z) * part.patternY + y)
                                                    * part.patternX + x) * part.layers + l);
                            const int baseX = (texture % columns) * cellWidth;
                            const int baseY = groupY + (texture / columns) * cellHeight;
                            for (int h = 0; h < part.height; ++h)
                                for (int w = 0; w < part.width; ++w) {
                                    const int px = baseX + (part.width - w - 1) * tileSize;
                                    const int py = baseY + (part.height - h - 1) * tileSize;
                                    tiles.append(image.copy(px, py, tileSize, tileSize));
                                }
                        }
        if (combined)
            groupY += part.frames * part.patternY * maxHeight * tileSize;
    }

    const int firstId = m_project.sprites()->addSprites(tiles);
    if (firstId < 1) {
        message(m_project.sprites()->errorString());
        return false;
    }
    int offset = 0;
    for (int g = firstGroup; g < lastGroup; ++g) {
        SheetGeometry part;
        geometryFor(edited, m_category, g, part);
        part.spriteIds->resize(size_t(sheetSpriteCount(part)));
        for (auto &id : *part.spriteIds)
            id = uint32_t(firstId + offset++);
    }
    if (m_category == 1 && firstGroup == 0)
        syncDefaultGroup(edited);
    edited.modified = true;
    m_project.dat()->restoreObject(m_category, m_selected, edited);
    stageTextureEdit(before);
    auto &imported = m_textureImportedSprites[textureEditKey()];
    for (int id = firstId; id < firstId + tiles.size(); ++id)
        imported.insert(quint32(id));
    message(QStringLiteral("Imported %1 sprites into object %2%3.")
                .arg(tiles.size()).arg(edited.id)
                .arg(detectedSize ? QStringLiteral(" (dimensions detected from image)") : QString()));
    return true;
}

bool EditorBackend::beginPixelEdit(int group,int frame,int pattern,int layer,int x,int y)
{
    if (!loaded() || m_selected<0) return false;
    const ClientItem *item=m_project.dat()->objectAt(m_category,m_selected);
    if (!item) return false;
    ClientItem geometry=*item;
    SheetGeometry shape;
    if (!geometryFor(geometry,m_category,group,shape)) return false;
    const int slot=spriteIndex(shape,frame,pattern,qMax(0,layer),x,y);
    if (slot<0 || slot>=int(shape.spriteIds->size())) return false;
    const int spriteId=int((*shape.spriteIds)[size_t(slot)]);
    if (spriteId<0 || spriteId>spriteCount()) return false;
    QImage source;
    if (spriteId>0) source=m_project.sprites()->spriteImage(spriteId);
    if (source.isNull()) {
        source=QImage(m_project.spriteSize(),m_project.spriteSize(),QImage::Format_RGBA8888);
        source.fill(Qt::transparent);
    }
    m_pixelEditImage=source.convertToFormat(QImage::Format_RGBA8888);
    m_pixelEditOriginal=m_pixelEditImage;
    m_pixelEditSpriteId=spriteId;
    m_pixelEditSlot=slot;
    m_pixelEditCategory=m_category;
    m_pixelEditRow=m_selected;
    m_pixelEditGroup=group;
    ++m_pixelEditRevision; emit pixelEditChanged();
    return true;
}

bool EditorBackend::paintPixel(int x,int y,const QColor &color)
{
    if (m_pixelEditImage.isNull() || x<0 || y<0 || x>=m_pixelEditImage.width() || y>=m_pixelEditImage.height() || !color.isValid()) return false;
    m_pixelEditImage.setPixelColor(x,y,color);
    ++m_pixelEditRevision;emit pixelEditChanged();
    return true;
}

void EditorBackend::resetPixelEdit()
{
    if (m_pixelEditOriginal.isNull()) return;
    m_pixelEditImage=m_pixelEditOriginal;
    ++m_pixelEditRevision;emit pixelEditChanged();
}

void EditorBackend::cancelPixelEdit()
{
    m_pixelEditImage=QImage();m_pixelEditOriginal=QImage();m_pixelEditSpriteId=0;m_pixelEditSlot=-1;
    m_pixelEditCategory=-1;m_pixelEditRow=-1;
    ++m_pixelEditRevision;emit pixelEditChanged();
}

bool EditorBackend::savePixelEdit()
{
    if (m_pixelEditImage.isNull() || !loaded() || m_pixelEditSlot<0) return false;
    const ClientItem *current=m_project.dat()->objectAt(m_pixelEditCategory,m_pixelEditRow);
    if (!current) return false;
    if (m_pixelEditSpriteId>0) {
        if (!m_project.sprites()->replaceSprite(m_pixelEditSpriteId,m_pixelEditImage)) return false;
    } else {
        const int spriteId=m_project.sprites()->addSprite(m_pixelEditImage);
        if (spriteId<1) return false;
        const ClientItem before=*current;
        ClientItem edited=before;
        SheetGeometry shape;
        if (!geometryFor(edited,m_pixelEditCategory,m_pixelEditGroup,shape) || m_pixelEditSlot>=int(shape.spriteIds->size())) return false;
        (*shape.spriteIds)[size_t(m_pixelEditSlot)]=uint32_t(spriteId);
        if (m_pixelEditCategory==1 && m_pixelEditGroup==0) syncDefaultGroup(edited);
        edited.modified=true;
        m_project.dat()->restoreObject(m_pixelEditCategory,m_pixelEditRow,edited);
        if (m_category==m_pixelEditCategory && m_selected==m_pixelEditRow) {
            stageTextureEdit(before);
            m_textureImportedSprites[textureEditKey()].insert(quint32(spriteId));
        }
    }
    const int savedId=m_pixelEditSpriteId;
    cancelPixelEdit();
    ++m_revision;emit changed();
    message(QString("Edited pixels for sprite %1").arg(savedId>0?savedId:spriteCount()));
    return true;
}
