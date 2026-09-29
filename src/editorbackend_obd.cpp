#include "editorbackend.h"
#include "obdcodec.h"

#include <QFile>

bool EditorBackend::importObd(const QString &fileUrl)
{
    if (!loaded() || m_selected<0 || m_compiling || m_project.spriteSize()!=32) return false;
    QFile file(path(fileUrl));
    if (!file.open(QIODevice::ReadOnly) || file.size()>128ll*1024*1024) {
        message("Cannot open OBD file or file is too large");return false;
    }
    ObdObject source;
    QString error;
    if (!ObdCodec::decode(file.readAll(),source,&error)) {message(error);return false;}
    if (source.category!=m_category) {message("OBD category does not match the selected object");return false;}
    if (source.item.frame_groups.size()>1 && !m_project.frameGroups()) {
        message("This OBD outfit needs target DAT frame-group support");return false;
    }
    const ClientItem *current=m_project.dat()->objectAt(m_category,m_selected);
    if (!current) return false;
    std::vector<uint32_t> ids;
    if (m_category==1 && !source.item.frame_groups.empty()) {
        for (const auto &group:source.item.frame_groups)
            ids.insert(ids.end(),group.sprite_ids.begin(),group.sprite_ids.end());
    } else ids=source.item.sprite_ids;
    if (ids.size()!=size_t(source.sprites.size())) {message("OBD sprite count does not match its patterns");return false;}
    QHash<quint32,quint32> mapping;
    QVector<QImage> pictures;
    QVector<quint32> sourceIds;
    for (size_t slot=0;slot<ids.size();++slot) {
        const quint32 id=ids[slot];
        if (!id || mapping.contains(id)) continue;
        mapping.insert(id,0);
        sourceIds.append(id);pictures.append(source.sprites[int(slot)]);
    }
    if (!m_project.extended() && pictures.size()>65535-spriteCount()) {
        message("The target client has too few free sprite IDs for this OBD object");return false;
    }
    QSet<quint32> imported;
    for (int first=0;first<pictures.size();first+=256) {
        const int count=qMin(256,pictures.size()-first);
        const int targetFirst=m_project.sprites()->addSprites(pictures.mid(first,count));
        if (targetFirst<1) {
            m_project.sprites()->discardImportedSprites(imported,m_project.dat()->usedSpriteIds());
            message(m_project.sprites()->errorString());return false;
        }
        for (int offset=0;offset<count;++offset) {
            const quint32 target=quint32(targetFirst+offset);
            mapping[sourceIds[first+offset]]=target;imported.insert(target);
        }
    }
    ClientItem replacement=source.item;
    const ClientItem before=*current;
    replacement.id=before.id;
    if (!m_project.frameDurations()) replacement.animation_data.clear();
    if (m_category==0) m_project.dat()->adaptImportedItem(replacement);
    for (auto &id:replacement.sprite_ids) if(id) id=mapping.value(id,0);
    for (auto &group:replacement.frame_groups) {
        if (!m_project.frameDurations()) group.animation_data.clear();
        for (auto &id:group.sprite_ids) if(id) id=mapping.value(id,0);
    }
    replacement.modified=true;
    m_project.dat()->restoreObject(m_category,m_selected,replacement);
    stageTextureEdit(before);
    m_textureImportedSprites[textureEditKey()].unite(imported);
    message(QString("Imported OBD object into client ID %1 (%2 sprites)").arg(before.id).arg(imported.size()));
    return true;
}
