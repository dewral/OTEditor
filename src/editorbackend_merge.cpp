#include "editorbackend.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <climits>

int EditorBackend::mergeProject(const QString &folderUrl,int sourceVersion)
{
    if (!loaded() || m_compiling) return 0;
    const QString folder=path(folderUrl);
    if (sourceVersion<=0) sourceVersion=detectFolderVersion(folder);
    if (sourceVersion!=m_project.clientVersion()) {
        message("Merge requires matching client versions. Use Import Objects from Client for cross-version items.");
        return 0;
    }
    ProjectModel source;
    QString error;
    if (!source.open(folder,sourceVersion,m_project.transparency(),&error)) {
        message("Merge source: "+error);return 0;
    }
    if (source.spriteSize()!=m_project.spriteSize() || source.extended()!=m_project.extended() ||
        source.transparency()!=m_project.transparency() || source.frameGroups()!=m_project.frameGroups() ||
        source.frameDurations()!=m_project.frameDurations()) {
        message("Merge source and target must use the same sprite and DAT format options.");return 0;
    }
    int totalObjects=0;
    for (int category=0;category<4;++category) {
        const int target=m_project.dat()->categoryCount(category);
        const int incoming=source.dat()->categoryCount(category);
        if (incoming>65535-(category==0?99:0)-target) {
            message("Merge would exceed the client ID limit.");return 0;
        }
        totalObjects+=incoming;
    }
    QVector<quint32> spriteIds;
    QSet<quint32> seen;
    QHash<quint32,quint32> mapping;
    auto collect=[&](const std::vector<uint32_t> &ids)->bool {
        for (quint32 id:ids) {
            if (!id || seen.contains(id)) continue;
            seen.insert(id);
            if (id>quint32(source.sprites()->spriteCount())) return false;
            if (source.sprites()->hasSpriteData(id)) spriteIds.append(id);
            else mapping.insert(id,0);
        }
        return true;
    };
    for (int category=0;category<4;++category) for (int row=0;row<source.dat()->categoryCount(category);++row) {
        const auto *item=source.dat()->objectAt(category,row);
        if (!collect(item->sprite_ids)) { message("Source DAT references a missing sprite.");return 0; }
        for (const auto &group:item->frame_groups)
            if (!collect(group.sprite_ids)) { message("Source DAT references a missing sprite.");return 0; }
    }
    if (!m_project.extended() && spriteIds.size()>65535-spriteCount()) {
        message("Merge would exceed the 16-bit sprite ID limit.");return 0;
    }
    commitTextureEdits();
    m_compiling=true;m_compileProgress=0;m_compileStage="Merging source sprites";emit compileProgressChanged();
    QSet<quint32> added;
    auto fail=[&](const QString &reason) {
        m_project.sprites()->discardImportedSprites(added,m_project.dat()->usedSpriteIds());
        m_compiling=false;emit compileProgressChanged();message(reason);return 0;
    };
    source.sprites()->beginBulkAccess();
    for (int first=0;first<spriteIds.size();first+=256) {
        const int count=qMin(256,spriteIds.size()-first);
        QVector<QImage> pictures;pictures.reserve(count);
        for (int offset=0;offset<count;++offset) {
            const quint32 id=spriteIds[first+offset];
            const auto decoded=source.sprites()->loadSpriteUncached(id);
            if (!decoded || decoded->image.isNull()) {
                source.sprites()->endBulkAccess();return fail(QString("Could not decode source sprite %1.").arg(id));
            }
            pictures.append(decoded->image);
        }
        const int targetFirst=m_project.sprites()->addSprites(pictures);
        if (targetFirst<1) {source.sprites()->endBulkAccess();return fail(m_project.sprites()->errorString());}
        for (int offset=0;offset<count;++offset) {
            const quint32 targetId=quint32(targetFirst+offset);
            mapping.insert(spriteIds[first+offset],targetId);added.insert(targetId);
        }
        m_compileProgress=totalObjects ? 50*(first+count)/qMax(1,spriteIds.size()) : 50;
        m_compileStage=QString("Merging sprites: %1 / %2").arg(first+count).arg(spriteIds.size());
        emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    source.sprites()->endBulkAccess();
    const int firstNewItem=100+m_project.dat()->itemCount();
    const int incomingItems=source.dat()->categoryCount(0);
    if (incomingItems && !m_project.dat()->ensureItemId(firstNewItem+incomingItems-1))
        return fail("Could not allocate target item IDs.");
    int copied=0;
    for (int category=0;category<4;++category) {
        const int firstTarget=category==0?firstNewItem-100:m_project.dat()->categoryCount(category);
        const int incoming=source.dat()->categoryCount(category);
        for (int row=0;row<incoming;++row) {
            ClientItem item=*source.dat()->objectAt(category,row);
            item.id=uint16_t(firstTarget+row+(category==0?100:1));
            for (auto &id:item.sprite_ids) if(id) id=mapping.value(id,0);
            for (auto &group:item.frame_groups)
                for (auto &id:group.sprite_ids) if(id) id=mapping.value(id,0);
            item.modified=true;
            if (category>0 && m_project.dat()->createObject(category)<0)
                return fail("Could not create target object.");
            m_project.dat()->restoreObject(category,firstTarget+row,item);
            ++copied;
            if (copied%256==0 || copied==totalObjects) {
                m_compileProgress=50+50*copied/qMax(1,totalObjects);
                m_compileStage=QString("Merging objects: %1 / %2").arg(copied).arg(totalObjects);
                emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    }
    m_undo.clear();m_redo.clear();
    if (incomingItems) {
        m_category=0;m_selected=firstNewItem-100;m_selectedRows={m_selected};m_selectionAnchor=m_selected;
        notifySelection();
    }
    refresh();m_compileProgress=100;m_compileStage="Merge complete";
    m_compiling=false;emit compileProgressChanged();
    message(QString("Merged %1 objects and %2 sprites from %3. Server OTB entries were not merged.")
        .arg(copied).arg(added.size()).arg(folder));
    return copied;
}
