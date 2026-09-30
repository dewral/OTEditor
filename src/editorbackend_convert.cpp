#include "editorbackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>

#include <algorithm>

namespace {
ClientFrameGroup singleGroup(const ClientItem &item)
{
    ClientFrameGroup group;
    group.type=1;
    group.width=item.width;group.height=item.height;group.exact_size=item.exact_size;
    group.layers=item.layers;group.pattern_x=item.pattern_x;group.pattern_y=item.pattern_y;
    group.pattern_z=item.pattern_z;group.frames=item.frames;
    group.sprite_ids=item.sprite_ids;group.animation_data=item.animation_data;
    return group;
}

void selectGroup(ClientItem &item,const ClientFrameGroup &group)
{
    item.width=group.width;item.height=group.height;item.exact_size=group.exact_size;
    item.layers=group.layers;item.pattern_x=group.pattern_x;item.pattern_y=group.pattern_y;
    item.pattern_z=group.pattern_z;item.frames=group.frames;
    item.sprite_ids=group.sprite_ids;item.animation_data=group.animation_data;
}

bool copyFileAtomically(const QString &source,const QString &target)
{
    QFile input(source);
    QSaveFile output(target);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return false;
    while (!input.atEnd()) {
        const QByteArray block=input.read(1024*1024);
        if (block.isEmpty() || output.write(block)!=block.size()) return false;
    }
    return output.commit();
}
}

bool EditorBackend::convertProject(const QString &folderUrl,int targetVersion)
{
    if (!loaded() || m_compiling) return false;
    static const QList<int> supported={772,780,800,860,960,1010,1050,1057,1098,1200,1310};
    if (!supported.contains(targetVersion)) {
        message("Choose a supported target DAT version (7.72 through 13.10).");return false;
    }
    const QDir output(path(folderUrl));
    if (!output.exists() || output.canonicalPath()==QDir(m_project.folder()).canonicalPath()) {
        message("Choose an existing output folder different from the open project.");return false;
    }
    const QStringList names={"Tibia.dat","Tibia.spr","Tibia.otfi","items.otb","items.xml"};
    for (const QString &name:names) if (QFileInfo::exists(output.filePath(name))) {
        message("Output folder already contains "+name);return false;
    }
    const auto sourceIds=m_project.dat()->usedSpriteIds();
    if (!sourceIds.isEmpty() && *std::max_element(sourceIds.cbegin(),sourceIds.cend())>quint32(spriteCount())) {
        message("Source DAT references a missing sprite.");return false;
    }
    const bool extended=targetVersion>=960;
    int populatedSprites=0;
    for (quint32 id:sourceIds) if (m_project.sprites()->hasSpriteData(id)) ++populatedSprites;
    if (!extended && populatedSprites>65535) {
        message("Too many used sprites for a 16-bit target client.");return false;
    }
    QTemporaryDir staging(output.filePath(".oteditor-convert-XXXXXX"));
    if (!staging.isValid()) {message("Could not create temporary conversion folder.");return false;}
    EditorBackend converted;
    if (!converted.createAssetFiles(staging.path(),targetVersion,extended,m_project.transparency(),
                                    targetVersion>=1050,targetVersion>=1057,m_project.spriteSize())) {
        message("Could not initialize target project: "+converted.status());return false;
    }
    commitTextureEdits();
    m_compiling=true;m_compileProgress=0;m_compileStage="Converting sprites";emit compileProgressChanged();
    auto fail=[&](const QString &reason){
        m_compiling=false;emit compileProgressChanged();message(reason);return false;
    };
    QList<quint32> ids=sourceIds.values();
    std::sort(ids.begin(),ids.end());
    QHash<quint32,quint32> mapping;
    m_project.sprites()->beginBulkAccess();
    for (int first=0;first<ids.size();first+=256) {
        QVector<QImage> images;
        QVector<quint32> original;
        for (int index=first;index<qMin(first+256,ids.size());++index) {
            const quint32 id=ids[index];
            if (!m_project.sprites()->hasSpriteData(id)) {mapping.insert(id,0);continue;}
            const auto decoded=m_project.sprites()->loadSpriteUncached(id);
            if (!decoded || decoded->image.isNull()) {
                m_project.sprites()->endBulkAccess();
                return fail(QString("Could not decode source sprite %1.").arg(id));
            }
            images.append(decoded->image);original.append(id);
        }
        if (!images.isEmpty()) {
            const int targetFirst=converted.m_project.sprites()->addSprites(images);
            if (targetFirst<1) {
                m_project.sprites()->endBulkAccess();
                return fail(converted.m_project.sprites()->errorString());
            }
            for (int index=0;index<original.size();++index)
                mapping.insert(original[index],quint32(targetFirst+index));
        }
        m_compileProgress=ids.isEmpty()?50:50*qMin(first+256,ids.size())/ids.size();
        m_compileStage=QString("Converting sprites: %1 / %2").arg(qMin(first+256,ids.size())).arg(ids.size());
        emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    m_project.sprites()->endBulkAccess();
    const int itemCount=m_project.dat()->categoryCount(0);
    if (itemCount && !converted.m_project.dat()->ensureItemId(99+itemCount))
        return fail("Could not allocate target item IDs.");
    int total=0;
    for (int category=0;category<4;++category) total+=m_project.dat()->categoryCount(category);
    int processed=0;
    for (int category=0;category<4;++category) {
        const int count=m_project.dat()->categoryCount(category);
        for (int row=0;row<count;++row) {
            ClientItem item=*m_project.dat()->objectAt(category,row);
            if (category==1) {
                if (targetVersion<1057) {
                    if (!item.frame_groups.empty()) selectGroup(item,item.frame_groups.front());
                    item.frame_groups.clear();
                } else if (item.frame_groups.empty()) item.frame_groups.push_back(singleGroup(item));
            }
            if (targetVersion<1050) {
                item.animation_data.clear();
                for (auto &group:item.frame_groups) group.animation_data.clear();
            }
            if (targetVersion<780) item.has_bones=false;
            auto remap=[&](std::vector<uint32_t> &sprites){
                for (auto &id:sprites) if (id) id=mapping.value(id,0);
            };
            remap(item.sprite_ids);
            for (auto &group:item.frame_groups) remap(group.sprite_ids);
            converted.m_project.dat()->adaptImportedItem(item);
            if (category>0 && converted.m_project.dat()->createObject(category)<0)
                return fail("Could not allocate target object.");
            converted.m_project.dat()->restoreObject(category,row,item);
            ++processed;
            if (processed%256==0 || processed==total) {
                m_compileProgress=50+45*processed/qMax(1,total);
                m_compileStage=QString("Converting objects: %1 / %2").arg(processed).arg(total);
                emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    }
    if (!converted.m_project.compile()) return fail("Could not save converted DAT/SPR project.");
    if (m_project.otbLoaded() && !m_project.otb()->saveCopy(QDir(staging.path()).filePath("items.otb")))
        return fail(m_project.otb()->errorString());
    if (m_project.itemsXmlLoaded() && !m_project.itemsXml()->saveCopy(QDir(staging.path()).filePath("items.xml")))
        return fail("Could not copy items.xml.");
    QStringList copied;
    for (const QString &name:names) {
        const QString source=QDir(staging.path()).filePath(name);
        if (!QFileInfo::exists(source)) continue;
        if (!copyFileAtomically(source,output.filePath(name))) {
            for (const QString &created:copied) QFile::remove(output.filePath(created));
            return fail("Could not write converted "+name);
        }
        copied.append(name);
    }
    m_compileProgress=100;m_compileStage="Conversion complete";
    m_compiling=false;emit compileProgressChanged();
    message(QString("Converted %1 objects and %2 sprites to client %3 in %4. The open project is unchanged.")
            .arg(total).arg(mapping.size()).arg(targetVersion).arg(output.absolutePath()));
    return true;
}
