#include "editorbackend.h"
#include "otfireader.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <QDataStream>
#include <QDateTime>
#include <QUrl>
#include <QClipboard>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QtEndian>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <algorithm>
#include <climits>
#include <type_traits>

EditorBackend::EditorBackend(QObject *parent):QAbstractListModel(parent) {
    connect(&m_project, &ProjectModel::changed, this, &EditorBackend::changed);
    message("OTEditor ready. Open a folder containing DAT and SPR files.");
}
QString EditorBackend::path(const QString &url) { QUrl u(url); return u.isLocalFile()?u.toLocalFile():url; }
QString EditorBackend::localPath(const QString &url) const { return path(url); }
int EditorBackend::detectFolderVersion(const QString &url) const {
    const QDir folder(path(url));
    if (!folder.exists()) return 0;
    OtfiReader otfi;
    const bool hasOtfi = otfi.loadFromFolder(folder.absolutePath());
    QString datPath = folder.filePath(hasOtfi ? otfi.metadataFile() : QStringLiteral("Tibia.dat"));
    if (!QFileInfo::exists(datPath)) {
        const auto files = folder.entryInfoList({QStringLiteral("*.dat")}, QDir::Files, QDir::Name);
        if (files.isEmpty()) return 0;
        datPath = files.constFirst().absoluteFilePath();
    }
    const QList<int> versions = {1310,1200,1099,1098,1095,1094,1093,1057,1050,1010,960,860,780,772};
    QList<int> candidates;
    const QString name = folder.dirName();
    for (const int version : versions) {
        const QString digits = QString::number(version);
        const QString pattern = QStringLiteral("(?<!\\d)%1[._-]?%2(?!\\d)")
                                    .arg(digits.left(digits.size()-2), digits.right(2));
        if (QRegularExpression(pattern).match(name).hasMatch()) candidates.append(version);
    }
    for (const int version : versions) if (!candidates.contains(version)) candidates.append(version);
    for (const int version : candidates) {
        DatReader dat;
        dat.setClientVersion(version);
        dat.setOtfiOverrides(hasOtfi, hasOtfi && otfi.extended(),
                             hasOtfi && otfi.frameDurations(), hasOtfi && otfi.frameGroups());
        if (dat.loadFile(datPath)) return version;
    }
    return 0;
}
void EditorBackend::message(const QString &text) {
    m_status=text;
    m_log += QString("[%1]  %2\n").arg(QTime::currentTime().toString("HH:mm:ss"),text);
    if(m_log.size()>60000)m_log=m_log.right(50000);
    emit logChanged(); emit changed();
}
void EditorBackend::clearLog(){m_log.clear();emit logChanged();}
int EditorBackend::rowCount(const QModelIndex &parent) const {return parent.isValid()?0:m_rows.size();}
QHash<int,QByteArray> EditorBackend::roleNames() const {return {{ObjectId,"objectId"},{SourceRow,"sourceRow"},{ImageSource,"imageSource"},{Description,"description"}};}
QVariant EditorBackend::data(const QModelIndex &idx,int role) const {
    if(!idx.isValid()||idx.row()<0||idx.row()>=m_rows.size()||!loaded())return {};
    int row=m_rows[idx.row()];auto item=m_project.dat()->objectAt(m_category,row);if(!item)return {};
    switch(role){case ObjectId:return item->id;case SourceRow:return row;case ImageSource:return preview(row,0,m_category==1?2:0,0,m_category==1?0:-1);
    case Description:return QString("%1 × %2  ·  %3 frame(s)").arg(item->width*m_project.spriteSize()).arg(item->height*m_project.spriteSize()).arg(item->frames);default:return {};}
}
bool EditorBackend::openFolder(const QString &url,int version,bool alpha,const QString &serverFolder,int spriteSizeOverride) {
    QString error;
    if(!m_project.open(path(url),version,alpha,&error,path(serverFolder),spriteSizeOverride)){message(error);return false;}
    beginResetModel();m_rows.clear();m_category=0;m_selected=m_project.dat()->itemCount()?0:-1;endResetModel();
    m_selectedRows.clear();if(m_selected>=0)m_selectedRows.insert(m_selected);m_selectionAnchor=m_selected;notifySelection();
    m_version=version;m_folder=m_project.folder();m_alpha=m_project.transparency();m_extended=m_project.extended();
    m_durations=m_project.frameDurations();m_groups=m_project.frameGroups();
    m_undo.clear();m_redo.clear();m_textureEdits.clear();m_textureImportedSprites.clear();m_objectCopy.reset();m_patternsCopy.reset();m_propertiesCopy.reset();m_serverAttributesCopy.clear();++m_revision;rebuild();
    message(QString("Loaded %1 items, %2 outfits, %3 effects, %4 missiles and %5 sprites%6%7.")
                .arg(m_project.dat()->categoryCount(0)).arg(m_project.dat()->categoryCount(1))
                .arg(m_project.dat()->categoryCount(2)).arg(m_project.dat()->categoryCount(3))
                .arg(spriteCount())
                .arg(m_project.otbLoaded()?QStringLiteral(" • OTB"):QString())
                .arg(m_project.itemsXmlLoaded()?QStringLiteral(" • items.xml"):QString()));return true;
}
QVariantMap EditorBackend::inspectFolder(const QString &url,int version,bool alpha,const QString &serverFolder,int spriteSizeOverride) {
    if (url.isEmpty()) return {};
    ProjectModel preview;
    QString error;
    if (!preview.open(path(url),version,alpha,&error,path(serverFolder),spriteSizeOverride)) return {{"error",error}};
    return {{"ok",true},{"version",version},{"spriteSize",preview.spriteSize()},
            {"hasOtfi",preview.hasOtfi()},
            {"extended",preview.extended()},{"transparency",preview.transparency()},
            {"durations",preview.frameDurations()},{"groups",preview.frameGroups()},
            {"attributeServer",preview.attributeServer()},
            {"datSignature",QString::number(preview.dat()->signature(),16).toUpper()},
            {"items",preview.dat()->categoryCount(0)},{"outfits",preview.dat()->categoryCount(1)},
            {"effects",preview.dat()->categoryCount(2)},{"missiles",preview.dat()->categoryCount(3)},
            {"sprSignature",QString::number(preview.sprites()->signature(),16).toUpper()},
            {"sprites",preview.sprites()->spriteCount()}};
}
bool EditorBackend::createAssetFiles(const QString &folderUrl, int version, bool extended,
                                     bool transparency, bool durations, bool groups, int spriteSize) {
    const QDir folder(path(folderUrl));
    if (!folder.exists()) { message("Selected folder does not exist."); return false; }
    const QString datPath=folder.filePath("Tibia.dat");
    const QString sprPath=folder.filePath("Tibia.spr");
    const QString otfiPath=folder.filePath("Tibia.otfi");
    if (QFileInfo::exists(datPath) || QFileInfo::exists(sprPath) || QFileInfo::exists(otfiPath)) {
        message("The selected folder already contains Tibia asset files."); return false;
    }
    if (version < 710 || version > 1310) { message("Unsupported client version."); return false; }
    if (spriteSize != 32 && spriteSize != 64 && spriteSize != 128 && spriteSize != 256) {
        message("Unsupported sprite dimension."); return false;
    }
    const quint32 signature=static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    QSaveFile dat(datPath);
    if (!dat.open(QIODevice::WriteOnly)) { message("Cannot create Tibia.dat."); return false; }
    QDataStream datOut(&dat); datOut.setByteOrder(QDataStream::LittleEndian);
    datOut << signature << quint16(99) << quint16(0) << quint16(0) << quint16(0);
    if (datOut.status()!=QDataStream::Ok || !dat.commit()) { message("Cannot write Tibia.dat."); return false; }
    QSaveFile spr(sprPath);
    if (!spr.open(QIODevice::WriteOnly)) { QFile::remove(datPath); message("Cannot create Tibia.spr."); return false; }
    QDataStream sprOut(&spr); sprOut.setByteOrder(QDataStream::LittleEndian);
    sprOut << signature;
    if (extended) sprOut << quint32(0); else sprOut << quint16(0);
    if (sprOut.status()!=QDataStream::Ok || !spr.commit()) {
        QFile::remove(datPath); message("Cannot write Tibia.spr."); return false;
    }
    QSaveFile otfi(otfiPath);
    if (!otfi.open(QIODevice::WriteOnly)) {
        QFile::remove(datPath); QFile::remove(sprPath); message("Cannot create Tibia.otfi."); return false;
    }
    const QByteArray settings=QString("DatSpr\n  extended: %1\n  transparency: %2\n  frame-durations: %3\n  frame-groups: %4\n  metadata-controller: default\n  attribute-server: tfs1.4\n  metadata-file: Tibia.dat\n  sprites-file: Tibia.spr\n  sprite-size: %5\n  sprite-data-size: %6\n")
        .arg(extended?"true":"false", transparency?"true":"false",
             durations?"true":"false", groups?"true":"false",
             QString::number(spriteSize), QString::number(spriteSize*spriteSize*(transparency?4:3))).toUtf8();
    if (otfi.write(settings)!=settings.size() || !otfi.commit()) {
        QFile::remove(datPath); QFile::remove(sprPath); message("Cannot write Tibia.otfi."); return false;
    }
    if (!openFolder(folder.absolutePath(),version,transparency)) {
        QFile::remove(datPath); QFile::remove(sprPath); QFile::remove(otfiPath); return false;
    }
    message(QString("Created empty %1 asset files in %2.").arg(version).arg(folder.absolutePath()));
    return true;
}
void EditorBackend::rebuild(){
    beginResetModel();m_rows.clear();m_visiblePositions.clear();
    if(loaded())for(int i=0;i<m_project.dat()->categoryCount(m_category);++i){
        auto item=m_project.dat()->objectAt(m_category,i);
        if(m_hideEmpty&&std::all_of(item->sprite_ids.begin(),item->sprite_ids.end(),[](uint32_t s){return s==0;}))continue;
        if(!m_query.isEmpty()&&!QString::number(item->id).contains(m_query))continue;
        m_visiblePositions.insert(i,m_rows.size());m_rows.push_back(i);
    }
    endResetModel();emit changed();
}
void EditorBackend::notifySelection(){++m_selectionRevision;emit selectionChanged();}
void EditorBackend::filter(const QString &query,bool hideEmpty){m_query=query.trimmed();m_hideEmpty=hideEmpty;rebuild();if(!m_visiblePositions.contains(m_selected))m_selected=m_rows.isEmpty()?-1:m_rows.front();m_selectedRows.clear();if(m_selected>=0)m_selectedRows.insert(m_selected);m_selectionAnchor=m_selected;notifySelection();emit changed();}
QVariantList EditorBackend::selectedRows() const { QVariantList rows; for(int row:m_rows)if(m_selectedRows.contains(row))rows.append(row);return rows; }
void EditorBackend::select(int row){if(!loaded()||!m_project.dat()->objectAt(m_category,row))return;m_selected=row;m_selectedRows={row};m_selectionAnchor=row;notifySelection();emit changed();}
void EditorBackend::selectWithModifiers(int row,int modifiers){
    if(!loaded()||!m_project.dat()->objectAt(m_category,row)||!m_visiblePositions.contains(row))return;
    const bool control=modifiers&Qt::ControlModifier;
    const bool shift=modifiers&Qt::ShiftModifier;
    if(shift && m_visiblePositions.contains(m_selectionAnchor)){
        if(!control)m_selectedRows.clear();
        const int first=m_visiblePositions.value(m_selectionAnchor),last=m_visiblePositions.value(row);
        for(int i=qMin(first,last);i<=qMax(first,last);++i)m_selectedRows.insert(m_rows[i]);
        m_selected=row;
    }else if(control){
        if(m_selectedRows.contains(row))m_selectedRows.remove(row);else m_selectedRows.insert(row);
        m_selected=m_selectedRows.contains(row)?row:-1;
        if(m_selected<0)for(int visible:m_rows)if(m_selectedRows.contains(visible)){m_selected=visible;break;}
        m_selectionAnchor=row;
    }else{select(row);return;}
    notifySelection();emit changed();
}
void EditorBackend::activateSelected(int row){if(m_selectedRows.contains(row)){m_selected=row;emit changed();}}
void EditorBackend::setCategory(int category){if(category<0||category>3||m_category==category)return;m_category=category;m_selected=loaded()&&m_project.dat()->categoryCount(category)?0:-1;m_selectedRows.clear();m_selectionAnchor=m_selected;rebuild();if(!m_visiblePositions.contains(m_selected))m_selected=m_rows.isEmpty()?-1:m_rows.front();if(m_selected>=0)m_selectedRows.insert(m_selected);m_selectionAnchor=m_selected;notifySelection();emit changed();}
void EditorBackend::jump(int id){select(id-(m_category==0?100:1));}
void EditorBackend::step(int offset){if(m_rows.isEmpty())return;int pos=m_rows.indexOf(m_selected);select(m_rows[qBound(0,pos+offset,int(m_rows.size())-1)]);}
void EditorBackend::refresh(){++m_revision;rebuild();}
QVariantMap EditorBackend::details() const {
    if(!loaded())return {};auto item=m_project.dat()->objectAt(m_category,m_selected);if(!item)return {};
    if(m_category==0)return m_project.dat()->detailsAt(m_selected);
    QVariantList ids;for(auto id:item->sprite_ids)ids.append(id);
    QVariantList groups;
    for(const auto &group:item->frame_groups){
        QVariantList groupIds;for(auto id:group.sprite_ids)groupIds.append(id);
        groups.append(QVariantMap{{"type",group.type},{"itemWidth",group.width},{"itemHeight",group.height},
            {"cropSize",group.exact_size},{"layers",group.layers},{"frames",group.frames},
            {"patternX",group.pattern_x},{"patternY",group.pattern_y},{"patternZ",group.pattern_z},{"spriteIds",groupIds}});
    }
    if(groups.isEmpty())groups.append(QVariantMap{{"type",0},{"itemWidth",item->width},{"itemHeight",item->height},
        {"cropSize",item->exact_size},{"layers",item->layers},{"frames",item->frames},
        {"patternX",item->pattern_x},{"patternY",item->pattern_y},{"patternZ",item->pattern_z},{"spriteIds",ids}});
    return {{"itemId",item->id},{"itemWidth",item->width},{"itemHeight",item->height},{"cropSize",item->exact_size},
        {"layers",item->layers},{"frames",item->frames},{"patternX",item->pattern_x},{"patternY",item->pattern_y},
        {"patternZ",item->pattern_z},{"spriteIds",ids},{"frameGroupCount",groups.size()},{"frameGroups",groups},
        {"hasLight",item->has_light},{"lightLevel",item->light_level},{"lightColor",item->light_color},
        {"hasOffset",item->has_offset},{"offsetX",item->offset_x},{"offsetY",item->offset_y},
        {"animateAlways",item->animate_always}};
}
QVariantMap EditorBackend::info() const {
    if(!loaded())return {};
    const auto otb=m_project.otb();
    const QString otbVersion=m_project.otbLoaded()
        ? QString("%1.%2.%3").arg(otb->majorVersion()).arg(otb->minorVersion()).arg(otb->buildNumber())
        : QStringLiteral("—");
    QString attributes=m_project.attributeServer();
    if(attributes.startsWith("tfs",Qt::CaseInsensitive))attributes="TFS "+attributes.mid(3);
    if(attributes.isEmpty())attributes=QStringLiteral("—");
    QString metadataController=m_project.metadataController();
    if(metadataController.isEmpty()||metadataController.compare("default",Qt::CaseInsensitive)==0)
        metadataController=QStringLiteral("Default");
    return {{"version",QString::number(m_version/100)+"."+QString::number(m_version%100).rightJustified(2,'0')},{"folder",m_project.folder()},{"dat",QFileInfo(m_project.dat()->filePath()).fileName()},
    {"otbVersion",otbVersion},{"otbMajor",m_project.otbLoaded()?int(m_project.otb()->majorVersion()):0},
    {"otbMinor",m_project.otbLoaded()?int(m_project.otb()->minorVersion()):0},
    {"otbBuild",m_project.otbLoaded()?int(m_project.otb()->buildNumber()):0},{"attributes",attributes},
    {"spriteDimension",QString("%1x%1").arg(m_project.spriteSize())},
    {"metadataController",metadataController},
    {"signature",QString::number(m_project.dat()->signature(),16).toUpper()},{"items",m_project.dat()->categoryCount(0)},{"outfits",m_project.dat()->categoryCount(1)},{"effects",m_project.dat()->categoryCount(2)},{"missiles",m_project.dat()->categoryCount(3)},
    {"sprSignature",QString::number(m_project.sprites()->signature(),16).toUpper()},{"sprites",spriteCount()},{"extended",m_extended},{"alpha",m_alpha},{"durations",m_durations},{"groups",m_groups},
    {"otb",m_project.otbLoaded()},{"itemsXml",m_project.itemsXmlLoaded()},{"datDirty",m_project.dat()->isDirty()},{"sprDirty",m_project.sprites()->isDirty()}};
}
QString EditorBackend::preview(int row,int frame,int pattern,int groupIndex,int selectedLayer) const {
    return previewForCategory(m_category,row,frame,pattern,groupIndex,selectedLayer);
}
QString EditorBackend::previewObject(int category,int id,int frame,int pattern,int groupIndex,int selectedLayer) const {
    if(category<0||category>3)return {};
    return previewForCategory(category,id-(category==0?100:1),frame,pattern,groupIndex,selectedLayer);
}
QString EditorBackend::previewForCategory(int category,int row,int frame,int pattern,int groupIndex,int selectedLayer) const {
    if(!loaded())return {};auto item=m_project.dat()->objectAt(category,row);if(!item)return {};
    int width=item->width,height=item->height,layers=item->layers,frames=item->frames;
    int patternX=item->pattern_x,patternY=item->pattern_y,patternZ=item->pattern_z;
    const std::vector<uint32_t> *spriteIds=&item->sprite_ids;
    if(category==1 && !item->frame_groups.empty()){
        const auto &group=item->frame_groups[size_t(qBound(0,groupIndex,int(item->frame_groups.size())-1))];
        width=group.width;height=group.height;layers=group.layers;frames=group.frames;
        patternX=group.pattern_x;patternY=group.pattern_y;patternZ=group.pattern_z;spriteIds=&group.sprite_ids;
    }
    const int cells=width*height;
    const int patterns=patternX*patternY*patternZ;
    const int safeFrame=qBound(0,frame,frames-1),safePattern=qBound(0,pattern,patterns-1);
    const bool oneLayer=selectedLayer>=0;
    const int layer=oneLayer?qBound(0,selectedLayer,layers-1):0;
    const int start=((safeFrame*patterns+safePattern)*layers+layer)*cells;
    const int count=oneLayer?cells:cells*layers;
    QVariantList ids;for(int i=0;i<count&&start+i<int(spriteIds->size());++i)ids.append((*spriteIds)[size_t(start+i)]);
    return m_project.sprites()->itemImageSource(ids,width,height,oneLayer?1:layers)+"?v="+QString::number(m_revision);
}
void EditorBackend::copyText(const QString &value) {
    QGuiApplication::clipboard()->setText(value);
}
QVariantMap EditorBackend::frameDuration(int category,int id,int group,int frame) const {
    if(!loaded() || category<0 || category>3)return {};
    const auto item=m_project.dat()->objectAt(category,id-(category==0?100:1));
    if(!item)return {};
    const QByteArray *data=&item->animation_data;
    int frames=item->frames;
    if(category==1 && !item->frame_groups.empty()) {
        if(group<0 || group>=int(item->frame_groups.size()))return {};
        const auto &selected=item->frame_groups[size_t(group)];
        data=&selected.animation_data;frames=selected.frames;
    }
    if(frame<0 || frame>=frames)return {};
    if(data->size()!=6+frames*8)return {{"minimum",100},{"maximum",100},{"frames",frames},
        {"mode",0},{"loopCount",0},{"startFrame",0}};
    const uchar *bytes=reinterpret_cast<const uchar *>(data->constData()+6+frame*8);
    return {{"minimum",qFromLittleEndian<quint32>(bytes)},
            {"maximum",qFromLittleEndian<quint32>(bytes+4)},{"frames",frames},
            {"mode",quint8(data->at(0))},{"loopCount",qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(data->constData()+1))},
            {"startFrame",quint8(data->at(5))}};
}
bool EditorBackend::setFrameDuration(int category,int id,int group,int frame,int minimum,int maximum) {
    const int row=id-(category==0?100:1);
    const auto *current=loaded()?m_project.dat()->objectAt(category,row):nullptr;
    if(!current || minimum<1 || maximum<minimum)return false;
    const ClientItem before=*current;
    if(!m_project.dat()->setFrameDuration(category,row,group,frame,minimum,maximum))return false;
    m_undo.push_back({category,row,before,*m_project.dat()->objectAt(category,row)});
    if(m_undo.size()>100)m_undo.erase(m_undo.begin());
    m_redo.clear();refresh();
    message(QStringLiteral("Updated animation frame duration"));return true;
}
bool EditorBackend::setAnimationSettings(int category,int id,int group,int mode,int loopCount,int startFrame) {
    const int row=id-(category==0?100:1);
    const auto *current=loaded()?m_project.dat()->objectAt(category,row):nullptr;
    if (!current) return false;
    const ClientItem before=*current;
    if (!m_project.dat()->setAnimationSettings(category,row,group,mode,loopCount,startFrame)) return false;
    m_undo.push_back({category,row,before,*m_project.dat()->objectAt(category,row)});
    if (m_undo.size()>100) m_undo.erase(m_undo.begin());
    m_redo.clear();refresh();message("Updated animation playback settings");return true;
}
bool EditorBackend::duplicateFrame(int category,int id,int group,int frame) {
    if(!loaded() || !m_project.dat()->duplicateFrame(category,id-(category==0?100:1),group,frame))return false;
    ++m_revision;emit changed();message(QStringLiteral("Duplicated animation frame"));return true;
}
bool EditorBackend::deleteFrame(int category,int id,int group,int frame) {
    if(!loaded() || !m_project.dat()->deleteFrame(category,id-(category==0?100:1),group,frame))return false;
    ++m_revision;emit changed();message(QStringLiteral("Deleted animation frame"));return true;
}
int EditorBackend::optimizeFrameDurations(bool items,bool outfits,bool effects,int minimum,int maximum) {
    if(!loaded() || minimum<1 || maximum<minimum)return 0;
    const int count=m_project.dat()->optimizeFrameDurations(items,outfits,effects,minimum,maximum);
    if(count){++m_revision;emit changed();}
    message(QStringLiteral("Updated %1 animation frame duration(s)").arg(count));return count;
}
bool EditorBackend::convertFrameGroups(bool enabled,bool removeMounts) {
    if(!m_project.convertFrameGroups(enabled,removeMounts))return false;
    m_groups=enabled;++m_revision;emit changed();
    message(enabled?QStringLiteral("Added walking frame groups to outfits"):
                    QStringLiteral("Removed walking frame groups from outfits"));
    return true;
}
QVariantMap EditorBackend::optimizeSprites(bool compactIds) {
    if(!loaded() || m_compiling)return {};
    m_compiling=true;m_compileProgress=0;m_compileStage=QStringLiteral("Optimizing sprites");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    auto *sprites=m_project.sprites();
    const int total=sprites->spriteCount();
    auto used=m_project.dat()->usedSpriteIds();
    QList<quint32> ids=used.values();
    std::sort(ids.begin(),ids.end());
    QHash<QByteArray,quint32> canonical;
    QHash<quint32,quint32> mapping;
    sprites->beginBulkAccess();
    for(int i=0;i<ids.size();++i){
        const quint32 id=ids[i];
        if(id>quint32(total)){mapping.insert(id,0);continue;}
        const auto sprite=sprites->loadSpriteUncached(id);
        if(!sprite || sprite->is_empty){mapping.insert(id,0);continue;}
        const QImage image=sprite->image.convertToFormat(QImage::Format_RGBA8888);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(QByteArrayView(reinterpret_cast<const char *>(image.constBits()),image.sizeInBytes()));
        const QByteArray digest=hash.result();
        if(canonical.contains(digest))mapping.insert(id,canonical.value(digest));
        else canonical.insert(digest,id);
        if(i%256==0){m_compileProgress=ids.isEmpty()?60:60*i/ids.size();emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);}
    }
    sprites->endBulkAccess();
    QSet<quint32> keep;
    for(auto id:used)keep.insert(mapping.value(id,id));
    int objects=0,cleared=0;
    if(compactIds){
        QList<quint32> canonicalIds=keep.values();
        canonicalIds.removeAll(0);
        std::sort(canonicalIds.begin(),canonicalIds.end());
        QHash<quint32,quint32> compacted;
        for(int i=0;i<canonicalIds.size();++i)compacted.insert(canonicalIds[i],quint32(i+1));
        QHash<quint32,quint32> finalMapping;
        for(auto id:used)finalMapping.insert(id,compacted.value(mapping.value(id,id),0));
        if(!sprites->compactSprites(canonicalIds)){
            m_compiling=false;emit compileProgressChanged();message(QStringLiteral("Could not compact sprite IDs"));return {};
        }
        objects=m_project.dat()->remapSpriteIds(finalMapping);
        cleared=total-canonicalIds.size();
    } else {
        objects=m_project.dat()->remapSpriteIds(mapping);
        QSet<quint32> clear;
        for(int id=1;id<=total;++id)if(!keep.contains(quint32(id)))clear.insert(quint32(id));
        cleared=sprites->clearSprites(clear);
    }
    m_compileProgress=100;emit compileProgressChanged();
    m_compiling=false;emit compileProgressChanged();
    if(objects||cleared){++m_revision;emit changed();}
    message(QStringLiteral("Sprite optimization: %1 duplicate or empty ID(s) redirected; %2 sprite slot(s) removed%3.")
            .arg(mapping.size()).arg(cleared).arg(compactIds?QStringLiteral(" and IDs compacted"):QStringLiteral(" without changing IDs")));
    return {{"redirected",mapping.size()},{"cleared",cleared},{"objects",objects},{"newCount",sprites->spriteCount()}};
}
QString EditorBackend::spriteSource(int id) const {return loaded()?m_project.sprites()->spriteImageSource(id)+"?v="+QString::number(m_revision):QString();}
QImage EditorBackend::image(const QString &id){
    const QString clean=id.section('?',0,0);
    if(clean==QLatin1String("slicer/source"))return m_slicerImage;
    if(clean.startsWith(QLatin1String("slicer/tile/"))){
        bool ok=false;const int index=clean.mid(12).toInt(&ok);
        return ok&&index>=0&&index<m_slicerTiles.size()?m_slicerTiles.at(index):QImage();
    }
    return m_project.sprites()?m_project.sprites()->imageForProviderId(clean):QImage();
}
void EditorBackend::remember(const ClientItem &before){
    if (textureEditPending()) { refresh(); return; }
    m_undo.push_back({m_category,m_selected,before,*m_project.dat()->objectAt(m_category,m_selected)});if(m_undo.size()>100)m_undo.erase(m_undo.begin());m_redo.clear();refresh();
}
bool EditorBackend::convertFrameDurations(bool enabled,int minimum,int maximum) {
    if(!m_project.convertFrameDurations(enabled,minimum,maximum))return false;
    m_durations=enabled;++m_revision;emit changed();
    message(enabled?QStringLiteral("Enabled frame durations for animated objects"):
                    QStringLiteral("Removed frame durations from animated objects"));
    return true;
}
bool EditorBackend::setValue(const QString &key,const QVariant &value){
    if(!loaded()||m_category!=0||m_selected<0)return false;
    auto before=*m_project.dat()->objectAt(0,m_selected);
    if(!m_project.dat()->setValue(m_selected,key,value)){message("Cannot change attribute: "+key);return false;}remember(before);return true;
}
bool EditorBackend::setValues(const QVariantMap &values){
    if(!loaded()||m_category!=0||m_selected<0)return false;
    const auto before=*m_project.dat()->objectAt(0,m_selected);
    if(!m_project.dat()->setValues(m_selected,values)){message("Cannot save object properties for this DAT version.");return false;}
    if(!values.isEmpty())remember(before);
    return true;
}
bool EditorBackend::setTextureValue(int groupIndex,const QString &key,int value){
    if(!loaded()||m_category<1||m_category>3||m_selected<0)return false;
    const auto *current=m_project.dat()->objectAt(m_category,m_selected);
    if(!current)return false;
    const ClientItem before=*current;
    ClientItem edited=before;
    if(key==QLatin1String("frameGroupCount")){
        if(m_category!=1||!m_project.frameGroups()||value<1||value>2||edited.frame_groups.empty())return false;
        if(value==int(edited.frame_groups.size()))return true;
        if(value==2){auto walking=edited.frame_groups.front();walking.type=1;edited.frame_groups.push_back(std::move(walking));}
        else edited.frame_groups.resize(1);
    }else{
        if(groupIndex<0||(m_category!=1&&groupIndex!=0))return false;
        ClientFrameGroup *group=nullptr;
        if(m_category==1){if(groupIndex>=int(edited.frame_groups.size()))return false;group=&edited.frame_groups[size_t(groupIndex)];}
        auto assign=[&](auto &field,int minimum,int maximum){if(value<minimum||value>maximum)return false;field=static_cast<std::decay_t<decltype(field)>>(value);return true;};
        auto update=[&](auto &width,auto &height,auto &crop,auto &layers,auto &patternX,auto &patternY,auto &patternZ,auto &frames)->bool{
            if(key==QLatin1String("itemWidth"))return assign(width,1,32);
            if(key==QLatin1String("itemHeight"))return assign(height,1,32);
            if(key==QLatin1String("cropSize"))return assign(crop,1,255);
            if(key==QLatin1String("layers"))return assign(layers,1,16);
            if(key==QLatin1String("patternX"))return assign(patternX,1,32);
            if(key==QLatin1String("patternY"))return assign(patternY,1,32);
            if(key==QLatin1String("patternZ"))return assign(patternZ,1,32);
            if(key==QLatin1String("frames"))return assign(frames,1,255);
            return false;
        };
        if(group){
            if(!update(group->width,group->height,group->exact_size,group->layers,group->pattern_x,group->pattern_y,group->pattern_z,group->frames))return false;
            if(group->getTotalSprites()>1048576)return false;
            group->sprite_ids.resize(size_t(group->getTotalSprites()),0);
            if(group->frames!=before.frame_groups[size_t(groupIndex)].frames)group->animation_data.clear();
            if(groupIndex==0){
                edited.width=group->width;edited.height=group->height;edited.exact_size=group->exact_size;
                edited.layers=group->layers;edited.pattern_x=group->pattern_x;edited.pattern_y=group->pattern_y;
                edited.pattern_z=group->pattern_z;edited.frames=group->frames;
                edited.sprite_ids=group->sprite_ids;edited.animation_data=group->animation_data;
            }
        }else{
            if(!update(edited.width,edited.height,edited.exact_size,edited.layers,edited.pattern_x,edited.pattern_y,edited.pattern_z,edited.frames))return false;
            if(edited.getTotalSprites()>1048576)return false;
            edited.sprite_ids.resize(size_t(edited.getTotalSprites()),0);
            if(edited.frames!=before.frames)edited.animation_data.clear();
        }
    }
    edited.modified=true;
    m_project.dat()->restoreObject(m_category,m_selected,edited);
    remember(before);
    return true;
}
bool EditorBackend::assignSprite(int slot,int id){
    if(!loaded()||m_category!=0||m_selected<0||id<0||id>spriteCount())return false;
    auto before=*m_project.dat()->objectAt(0,m_selected);
    if(!m_project.dat()->setSpriteId(m_selected,slot,id))return false;remember(before);message(QString("Assigned sprite %1 to slot %2").arg(id).arg(slot));return true;
}
void EditorBackend::create(bool duplicate){
    if(!loaded()||(duplicate&&m_selected<0))return;
    commitTextureEdits();
    const int row=m_project.dat()->createObject(m_category,duplicate?m_selected:-1);
    if(row<0){message("Cannot create object");return;}
    m_selected=row;m_selectedRows={row};m_selectionAnchor=row;notifySelection();m_undo.clear();m_redo.clear();refresh();
    message(QString("Created object %1").arg(row+(m_category==0?100:1)));
}
void EditorBackend::clearObject(){
    if(!loaded()||m_category!=0||m_selected<0)return;auto before=*m_project.dat()->objectAt(0,m_selected);ClientItem empty;empty.id=before.id;empty.sprite_ids={0};empty.modified=true;m_project.dat()->restoreItem(m_selected,empty);remember(before);message("Object cleared; its ID is preserved.");
}
bool EditorBackend::removeObject(){
    if(!loaded()||m_selected<0)return false;
    commitTextureEdits();
    const int removedId=m_selected+(m_category==0?100:1);
    if(!m_project.dat()->removeObject(m_category,m_selected))return false;
    m_undo.clear();m_redo.clear();
    const int count=m_project.dat()->categoryCount(m_category);
    m_selected=count>0 ? qMin(m_selected,count-1) : -1;
    m_selectedRows.clear();if(m_selected>=0)m_selectedRows.insert(m_selected);m_selectionAnchor=m_selected;
    refresh();
    if(!m_visiblePositions.contains(m_selected)){m_selected=m_rows.isEmpty() ? -1 : m_rows.front();m_selectedRows.clear();if(m_selected>=0)m_selectedRows.insert(m_selected);m_selectionAnchor=m_selected;}
    notifySelection();emit changed();
    message(QString("Removed object %1; later IDs shifted down by one.").arg(removedId));
    return true;
}
namespace {
void copyGraphics(ClientItem &target, const ClientItem &source) {
    target.width=source.width;target.height=source.height;target.layers=source.layers;
    target.pattern_x=source.pattern_x;target.pattern_y=source.pattern_y;target.pattern_z=source.pattern_z;
    target.frames=source.frames;target.exact_size=source.exact_size;
    target.sprite_ids=source.sprite_ids;target.animation_data=source.animation_data;
}
}
QVariantMap EditorBackend::objectClipboard() const {
    return {{"object",m_objectCopy.has_value()},{"patterns",m_patternsCopy.has_value()},{"properties",m_propertiesCopy.has_value()}};
}
int EditorBackend::serverId() const {
    if(!loaded()||m_category!=0||m_selected<0||!m_project.otbLoaded())return -1;
    const auto item=m_project.dat()->objectAt(0,m_selected);
    const int row=m_project.otb()->rowForClientId(item->id);
    return row<0?-1:m_project.otb()->detailsAt(row).value("serverId").toInt();
}
QVariantMap EditorBackend::serverAttributes() const {
    if (!loaded() || m_category != 0 || m_selected < 0 || !m_project.otbLoaded()) return {};
    const auto *item = m_project.dat()->objectAt(0, m_selected);
    if (!item) return {};
    const int row = m_project.otb()->rowForClientId(item->id);
    return row < 0 ? QVariantMap{} : m_project.otb()->detailsAt(row);
}
bool EditorBackend::setServerAttributes(const QVariantMap &values) {
    if (serverId() < 0 || values.isEmpty()) return false;
    auto *otb = m_project.otb();
    const int row = otb->rowForServerId(serverId());
    const QVariantMap current = otb->detailsAt(row);
    const QHash<QString, QPair<int,int>> numeric = {
        {"serverId",{1,65535}}, {"clientId",{100,65535}},
        {"groupId",{0,15}}, {"speed",{0,65535}}, {"maxReadWriteLength",{0,65535}},
        {"maxReadLength",{0,65535}}, {"minimapColor",{0,65535}}, {"wareId",{0,65535}},
        {"lightLevel",{0,255}}, {"lightColor",{0,255}}, {"stackOrder",{-128,127}}};
    const QStringList booleans = {"unpassable","blockMissiles","blockPathfinder","hasElevation",
        "useable","pickupable","moveable","stackable","alwaysOnTop","readable","rotatable",
        "hangable","hookEast","hookSouth","allowDistRead","clientDuration","clientCharges",
        "ignoreLook","animation","fullGround","forceUse"};
    for (auto it=values.cbegin(); it!=values.cend(); ++it) {
        if (it.key()=="name" || it.key()=="description") continue;
        if (booleans.contains(it.key())) continue;
        if (!numeric.contains(it.key())) return false;
        bool ok=false; const qlonglong n=it.value().toLongLong(&ok);
        const auto bounds=numeric.value(it.key());
        if (!ok || n<bounds.first || n>bounds.second) return false;
        if (it.key()=="serverId" && otb->rowForServerId(int(n))>=0 && otb->rowForServerId(int(n))!=row) return false;
        if (it.key()=="clientId" && otb->rowForClientId(int(n))>=0 && otb->rowForClientId(int(n))!=row) return false;
    }
    bool modified=false;
    for (auto it=values.cbegin(); it!=values.cend(); ++it) {
        if (current.value(it.key())==it.value()) continue;
        if (!otb->setValue(row,it.key(),it.value())) return false;
        modified=true;
    }
    if (modified) { ++m_revision; emit changed(); message("Saved server attributes"); }
    return true;
}
bool EditorBackend::copyServerAttributes() {
    const QVariantMap attributes=serverAttributes();
    if (attributes.isEmpty()) return false;
    m_serverAttributesCopy.clear();
    const QStringList keys={"name","description","groupId","speed","maxReadWriteLength",
        "serverId","clientId",
        "maxReadLength","minimapColor","wareId","lightLevel","lightColor","stackOrder",
        "unpassable","blockMissiles","blockPathfinder","hasElevation","useable","pickupable",
        "moveable","stackable","alwaysOnTop","readable","rotatable","hangable","hookEast",
        "hookSouth","allowDistRead","clientDuration","clientCharges","ignoreLook","animation",
        "fullGround","forceUse"};
    for (const QString &key:keys) m_serverAttributesCopy.insert(key,attributes.value(key));
    emit changed(); message("Copied server attributes"); return true;
}
bool EditorBackend::pasteServerAttributes() {
    if (m_serverAttributesCopy.isEmpty()) return false;
    const bool ok=setServerAttributes(m_serverAttributesCopy);
    if (ok) message("Pasted server attributes");
    return ok;
}
bool EditorBackend::createServerItem() {
    if (!loaded() || m_category != 0 || m_selected < 0 || !m_project.otbLoaded()) return false;
    const auto *item=m_project.dat()->objectAt(0,m_selected);
    if (!item || item->id>65535 || m_project.otb()->rowForClientId(item->id)>=0) return false;
    const int row=m_project.otb()->createItem(item->id);
    if (row<0) return false;
    m_project.otb()->reloadItem(row);
    ++m_revision; emit changed(); message(QString("Created server item for client ID %1").arg(item->id));
    return true;
}
bool EditorBackend::createOtbFile() {
    QString error;
    if (!m_project.createOtb(&error)) { message(error); return false; }
    ++m_revision; emit changed();
    message(QStringLiteral("Created items.otb. Use Create Missing OTB Items to add the remaining client items."));
    return true;
}
bool EditorBackend::reloadSelectedOtbItem() {
    if (serverId()<0) return false;
    auto *otb=m_project.otb();
    const int row=otb->rowForServerId(serverId());
    if (row<0 || !otb->reloadItem(row)) return false;
    ++m_revision; emit changed(); message("Reloaded selected OTB item from its client object");
    return true;
}
bool EditorBackend::updateOtbVersion(int major,int minor,int build) {
    if (!m_project.otbLoaded() || major<0 || minor<0 || build<0) return false;
    m_project.otb()->setOtbVersion(quint32(major),quint32(minor),quint32(build));
    ++m_revision; emit changed(); message("Updated OTB version");
    return true;
}
QVariantMap EditorBackend::compareOtbFile(const QString &fileUrl) const {
    QVariantMap result;
    if (!m_project.otbLoaded()) { result.insert("error","No items.otb is loaded"); return result; }
    OtbReader other;
    if (!other.loadFile(path(fileUrl))) { result.insert("error",other.errorString()); return result; }
    const OtbReader *current=m_project.otb();
    int added=0,removed=0,changed=0;
    QVariantList differences;
    const QStringList keys={"clientId","name","description","groupId","speed","maxReadWriteLength",
        "maxReadLength","minimapColor","wareId","lightLevel","lightColor","stackOrder","unpassable",
        "blockMissiles","blockPathfinder","hasElevation","useable","pickupable","moveable","stackable",
        "alwaysOnTop","readable","rotatable","hangable","hookEast","hookSouth","allowDistRead",
        "clientDuration","clientCharges","ignoreLook","animation","fullGround","forceUse"};
    for (int row=0;row<current->itemCount();++row) {
        const auto left=current->detailsAt(row);
        const int id=left.value("serverId").toInt();
        const int otherRow=other.rowForServerId(id);
        if (otherRow<0) { ++removed; differences.append(QString("Only in current: server ID %1").arg(id)); continue; }
        const auto right=other.detailsAt(otherRow);
        QStringList fields;
        for (const QString &key:keys) if (left.value(key)!=right.value(key)) fields.append(key);
        if (!fields.isEmpty()) { ++changed; differences.append(QString("Server ID %1: %2").arg(id).arg(fields.join(", "))); }
    }
    for (int row=0;row<other.itemCount();++row) {
        const int id=other.detailsAt(row).value("serverId").toInt();
        if (current->rowForServerId(id)<0) { ++added; differences.append(QString("Only in comparison: server ID %1").arg(id)); }
    }
    result.insert("added",added); result.insert("removed",removed); result.insert("changed",changed);
    result.insert("differences",differences); return result;
}
void EditorBackend::copyId(bool server) {
    if(!loaded()||m_selected<0)return;
    const int id=server?serverId():m_project.dat()->objectAt(m_category,m_selected)->id;
    if(id<0)return;
    QGuiApplication::clipboard()->setText(QString::number(id));
    message(QString("Copied %1 ID: %2").arg(server?"server":"client").arg(id));
}
bool EditorBackend::copyObjectPart(const QString &part) {
    if(!loaded()||m_category!=0||m_selected<0)return false;
    auto *copy=part=="object"?&m_objectCopy:part=="patterns"?&m_patternsCopy:part=="properties"?&m_propertiesCopy:nullptr;
    if(!copy)return false;
    *copy=*m_project.dat()->objectAt(0,m_selected);
    message("Copied "+part);return true;
}
bool EditorBackend::applyObject(ClientItem item) {
    if(!loaded()||m_category!=0||m_selected<0)return false;
    const auto before=*m_project.dat()->objectAt(0,m_selected);
    item.id=before.id;item.modified=true;item.raw_record.clear();
    m_project.dat()->restoreItem(m_selected,item);remember(before);return true;
}
bool EditorBackend::pasteObjectPart(const QString &part) {
    if(!loaded()||m_category!=0||m_selected<0)return false;
    const auto *copy=part=="object"?&m_objectCopy:part=="patterns"?&m_patternsCopy:part=="properties"?&m_propertiesCopy:nullptr;
    if(!copy||!copy->has_value())return false;
    auto item=**copy;
    const auto target=*m_project.dat()->objectAt(0,m_selected);
    if(part=="patterns"){item=target;copyGraphics(item,**copy);}
    else if(part=="properties")copyGraphics(item,target);
    if(!applyObject(item))return false;
    message("Pasted "+part+"; target ID preserved");return true;
}
bool EditorBackend::replaceObject(int clientId) {
    if(!loaded()||m_category!=0||m_selected<0||clientId<100||clientId>65535)return false;
    const auto source=m_project.dat()->itemByClientId(static_cast<uint16_t>(clientId));
    if(!source){message("Source item does not exist");return false;}
    if(!applyObject(*source))return false;
    message(QString("Replaced object using client ID %1; target ID preserved").arg(clientId));return true;
}
int EditorBackend::bulkReplaceObjects(int sourceId) {
    if (!loaded() || m_category!=0 || m_selectedRows.isEmpty() || sourceId<100 || sourceId>65535) return 0;
    const auto *source=m_project.dat()->itemByClientId(uint16_t(sourceId));
    if (!source) return 0;
    commitTextureEdits();
    const ClientItem original=*source;
    int count=0;
    for (int row:m_selectedRows) {
        const auto *target=m_project.dat()->objectAt(0,row);
        if (!target) continue;
        ClientItem replacement=original;
        replacement.id=target->id;
        replacement.modified=true;
        m_project.dat()->restoreItem(row,replacement);
        ++count;
    }
    if (count) { m_undo.clear();m_redo.clear();refresh();message(QString("Replaced %1 selected item(s) from client ID %2").arg(count).arg(sourceId)); }
    return count;
}
int EditorBackend::bulkSetItemAttribute(const QString &key,const QVariant &value) {
    if (!loaded() || m_category!=0 || m_selectedRows.isEmpty()) return 0;
    // Validate the requested attribute before touching any selected object.
    const auto *first=m_project.dat()->objectAt(0,*m_selectedRows.cbegin());
    if (!first) return 0;
    ClientItem probe=*first;
    const int probeRow=*m_selectedRows.cbegin();
    if (!m_project.dat()->setValue(probeRow,key,value)) return 0;
    m_project.dat()->restoreItem(probeRow,probe);
    int count=0;
    for (int row:m_selectedRows) if (m_project.dat()->setValue(row,key,value)) ++count;
    if (count) { m_undo.clear();m_redo.clear();refresh();message(QString("Updated %1 selected item(s): %2").arg(count).arg(key)); }
    return count;
}
QVariantMap EditorBackend::compareSelectedObjects() const {
    QVariantMap result;
    if (!loaded() || m_selectedRows.size()!=2) { result.insert("error","Select exactly two objects"); return result; }
    auto it=m_selectedRows.cbegin(); const int rowA=*it++; const int rowB=*it;
    const int firstRow=qMin(rowA,rowB),secondRow=qMax(rowA,rowB);
    const auto *first=m_project.dat()->objectAt(m_category,firstRow);
    const auto *second=m_project.dat()->objectAt(m_category,secondRow);
    if (!first || !second) { result.insert("error","Selection is unavailable"); return result; }
    QVariantList differences;
    if (m_category==0) {
        const auto left=m_project.dat()->detailsAt(firstRow);
        const auto right=m_project.dat()->detailsAt(secondRow);
        for (auto field=left.cbegin();field!=left.cend();++field) {
            if (field.key()=="itemId" || field.key()=="spriteIds" || field.key()=="previewSpriteId") continue;
            if (field.value()!=right.value(field.key()))
                differences.append(QString("%1: %2 → %3").arg(field.key(),field.value().toString(),right.value(field.key()).toString()));
        }
    } else {
        const auto field=[&](const QString &name,int a,int b){if(a!=b)differences.append(QString("%1: %2 → %3").arg(name).arg(a).arg(b));};
        field("width",first->width,second->width); field("height",first->height,second->height);
        field("layers",first->layers,second->layers); field("pattern X",first->pattern_x,second->pattern_x);
        field("pattern Y",first->pattern_y,second->pattern_y); field("pattern Z",first->pattern_z,second->pattern_z);
        field("frames",first->frames,second->frames);
        field("frame groups",int(first->frame_groups.size()),int(second->frame_groups.size()));
    }
    const auto &leftSprites=first->sprite_ids,&rightSprites=second->sprite_ids;
    if (leftSprites.size()!=rightSprites.size())
        differences.append(QString("Sprite slots: %1 → %2").arg(leftSprites.size()).arg(rightSprites.size()));
    else {
        int mismatch=0;
        for (size_t slot=0;slot<leftSprites.size();++slot) if(leftSprites[slot]!=rightSprites[slot]) ++mismatch;
        if (mismatch) differences.append(QString("Different sprite slots: %1").arg(mismatch));
    }
    result.insert("firstId",first->id); result.insert("secondId",second->id);
    result.insert("differences",differences); return result;
}
bool EditorBackend::importItemGraphics(const QString &folderUrl,int sourceVersion,int sourceId) {
    if(!loaded() || m_category!=0 || m_selected<0){message("Select a target item first.");return false;}
    return importItemGraphicsRange(folderUrl,sourceVersion,sourceId,sourceId,m_selected+100)==1;
}
int EditorBackend::importItemGraphicsRange(const QString &folderUrl,int sourceVersion,int sourceFirstId,int sourceLastId,int targetFirstId) {
    if(!loaded() || m_category!=0 || m_compiling){message("Open a target item project first.");return 0;}
    if(sourceFirstId<100 || sourceLastId<sourceFirstId || sourceLastId>65535 || targetFirstId<100
       || quint64(targetFirstId)+quint64(sourceLastId-sourceFirstId)>65535){
       message("The source range or target range is outside the valid item IDs.");return 0;
    }
    commitTextureEdits();
    m_compiling=true;
    const auto progress=[this](int value,const QString &stage){
        m_compileProgress=value;m_compileStage=stage;emit compileProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    const auto fail=[this](const QString &error){
        m_compiling=false;emit compileProgressChanged();message(error);return 0;
    };
    progress(0,QStringLiteral("Import: loading source DAT and SPR"));
    const QString folder=path(folderUrl);
    if(sourceVersion<=0)sourceVersion=detectFolderVersion(folder);
    if(sourceVersion<=0)return fail(QStringLiteral("Could not detect the source client version."));
    ProjectModel sourceProject;
    QString error;
    if(!sourceProject.open(folder,sourceVersion,false,&error))return fail("Source: "+error);
    if(sourceProject.spriteSize()!=m_project.spriteSize()){
        return fail(QString("Sprite dimensions differ: source %1, target %2.").arg(sourceProject.spriteSize()).arg(m_project.spriteSize()));
    }
    const int sourceMaximumId=99+sourceProject.dat()->itemCount();
    if(sourceLastId>sourceMaximumId)
        return fail(QString("The source client ends at item ID %1; requested ID %2 does not exist.")
                    .arg(sourceMaximumId).arg(sourceLastId));
    QVector<const ClientItem *> sources;
    sources.reserve(sourceLastId-sourceFirstId+1);
    QVector<quint32> uniqueIds;
    QSet<quint32> seen;
    QHash<quint32,quint32> mapped;
    const int spriteCapacity=m_project.extended() ? INT_MAX : 65535-spriteCount();
    progress(5,QStringLiteral("Import: checking source IDs"));
    for(int sourceId=sourceFirstId;sourceId<=sourceLastId;++sourceId){
        const ClientItem *source=sourceProject.dat()->itemByClientId(static_cast<uint16_t>(sourceId));
        if(!source)return fail(QString("Source item %1 does not exist.").arg(sourceId));
        sources.append(source);
        for(const quint32 id:source->sprite_ids){
            if(id==0 || seen.contains(id))continue;
            seen.insert(id);
            if(id>quint32(sourceProject.sprites()->spriteCount()))return fail(QStringLiteral("Source DAT references a missing sprite."));
            if(!sourceProject.sprites()->hasSpriteData(id)){mapped.insert(id,0);continue;}
            uniqueIds.append(id);
            if(uniqueIds.size()>spriteCapacity)
                return fail(QString("This range needs at least %1 new sprites, but the target client has only %2 free sprite IDs.")
                            .arg(uniqueIds.size()).arg(spriteCapacity));
        }
        if((sourceId-sourceFirstId)%256==0)
            progress(5+15*(sourceId-sourceFirstId+1)/(sourceLastId-sourceFirstId+1),QStringLiteral("Import: checking source IDs"));
    }
    const int targetLastId=targetFirstId+sources.size()-1;
    const int previousLastId=99+m_project.dat()->itemCount();
    sourceProject.sprites()->beginBulkAccess();
    for(int first=0;first<uniqueIds.size();first+=256){
        const int count=qMin(256,uniqueIds.size()-first);
        QVector<QImage> pictures;pictures.reserve(count);
        for(int offset=0;offset<count;++offset){
            const quint32 id=uniqueIds[first+offset];
            const auto sprite=sourceProject.sprites()->loadSpriteUncached(id);
            if(!sprite || sprite->image.isNull()){
                sourceProject.sprites()->endBulkAccess();
                return fail(QString("Could not decode source sprite %1.").arg(id));
            }
            pictures.append(sprite->image);
        }
        const int targetFirstSprite=m_project.sprites()->addSprites(pictures);
        if(targetFirstSprite<=0){sourceProject.sprites()->endBulkAccess();return fail(m_project.sprites()->errorString());}
        for(int offset=0;offset<count;++offset)mapped.insert(uniqueIds[first+offset],quint32(targetFirstSprite+offset));
        progress(20+50*(first+count)/qMax(1,uniqueIds.size()),QStringLiteral("Import: copying sprites"));
    }
    sourceProject.sprites()->endBulkAccess();
    progress(70,QStringLiteral("Import: creating target IDs"));
    if(!m_project.dat()->ensureItemId(targetLastId))return fail(QStringLiteral("Could not create the missing target item IDs."));
    for(int first=0;first<sources.size();first+=256){
        const int count=qMin(256,sources.size()-first);
        const int firstRow=targetFirstId-100+first;
        std::vector<ClientItem> importedItems;importedItems.reserve(size_t(count));
        for(int offset=0;offset<count;++offset){
            const int row=firstRow+offset;
            const ClientItem before=*m_project.dat()->objectAt(0,row);
            ClientItem imported=*sources[first+offset];
            if(!m_project.frameDurations())imported.animation_data.clear();
            m_project.dat()->adaptImportedItem(imported);
            for(quint32 &id:imported.sprite_ids)if(id)id=mapped.value(id,0);
            imported.id=before.id;
            if(before.id<=previousLastId)m_undo.push_back({0,row,before,imported});
            importedItems.push_back(std::move(imported));
        }
        if(!m_project.dat()->replaceItems(firstRow,importedItems))return fail(QStringLiteral("Could not update target items."));
        if(m_undo.size()>100)m_undo.erase(m_undo.begin(),m_undo.end()-100);
        progress(70+29*(first+count)/sources.size(),QStringLiteral("Import: copying objects"));
    }
    m_redo.clear();m_selected=targetFirstId-100;m_selectedRows={m_selected};m_selectionAnchor=m_selected;notifySelection();refresh();
    progress(100,QStringLiteral("Import: complete"));
    m_compiling=false;emit compileProgressChanged();
    message(QString("Imported %1 complete client items from version %2, IDs %3-%4 into target IDs %5-%6 (%7 sprites, %8 new item IDs).")
            .arg(sources.size()).arg(sourceVersion).arg(sourceFirstId).arg(sourceLastId)
            .arg(targetFirstId).arg(targetLastId).arg(mapped.size()).arg(qMax(0,targetLastId-previousLastId)));
    return sources.size();
}
void EditorBackend::undo(){if(textureEditPending()){resetTextureEdit();return;}if(!canUndo())return;auto e=m_undo.back();m_undo.pop_back();m_project.dat()->restoreObject(e.category,e.row,e.before);m_redo.push_back(e);m_category=e.category;m_selected=e.row;m_selectedRows={m_selected};m_selectionAnchor=m_selected;notifySelection();refresh();}
void EditorBackend::redo(){if(!canRedo())return;auto e=m_redo.back();m_redo.pop_back();m_project.dat()->restoreObject(e.category,e.row,e.after);m_undo.push_back(e);m_category=e.category;m_selected=e.row;m_selectedRows={m_selected};m_selectionAnchor=m_selected;notifySelection();refresh();}
bool EditorBackend::compile(){
    if(m_compiling)return false;
    m_compiling=true;m_compileProgress=0;m_compileStage=QStringLiteral("Preparing project");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const auto progress=[this](int value,const QString &stage) {
        if(value==m_compileProgress && stage==m_compileStage)return;
        m_compileProgress=value;m_compileStage=stage;emit compileProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    QString error;const bool ok=m_project.compile(&error,progress);
    m_compiling=false;emit compileProgressChanged();
    if (ok) commitTextureEdits();
    message(ok?QStringLiteral("Project compiled: DAT, SPR%1%2").arg(m_project.otbLoaded()?QStringLiteral(", OTB"):QString()).arg(m_project.itemsXmlLoaded()?QStringLiteral(", items.xml"):QString()):error);
    return ok;
}
bool EditorBackend::compileAs(const QString &folderUrl){
    if(m_compiling)return false;
    m_compiling=true;m_compileProgress=0;m_compileStage=QStringLiteral("Preparing project");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const auto progress=[this](int value,const QString &stage) {
        if(value==m_compileProgress && stage==m_compileStage)return;
        m_compileProgress=value;m_compileStage=stage;emit compileProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    QString error;const bool ok=m_project.compileAs(path(folderUrl),&error,progress);
    m_compiling=false;emit compileProgressChanged();
    if (ok) commitTextureEdits();
    if(ok)m_folder=m_project.folder();
    message(ok?QStringLiteral("Project compiled to %1").arg(m_project.folder()):error);
    return ok;
}
bool EditorBackend::save(const QString &url){
    return url.isEmpty()?compile():compileAs(url);
}
int EditorBackend::createMissingOtbItems(){
    if(!m_project.otbLoaded() || m_compiling)return 0;
    m_compiling=true;m_compileProgress=0;m_compileStage=QStringLiteral("Creating OTB items");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const int count=m_project.otb()->createMissingItems([this](int done,int total){
        m_compileProgress=total ? 100*done/total : 100;
        m_compileStage=QStringLiteral("Creating OTB items: %1 / %2").arg(done).arg(total);
        emit compileProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });
    m_compiling=false;emit compileProgressChanged();
    ++m_revision;emit changed();
    message(QString("Created %1 missing OTB item(s).").arg(count));
    return count;
}
int EditorBackend::reloadItemAttributes(){
    if(!m_project.otbLoaded())return 0;
    const int count=m_project.otb()->reloadAllItems();
    ++m_revision;emit changed();
    message(QString("Reloaded attributes for %1 OTB item(s).").arg(count));
    return count;
}
bool EditorBackend::replaceSprite(int spriteId,const QString &fileUrl){
    if(!loaded())return false;QImage image(path(fileUrl));
    if(image.isNull()){message("Cannot read sprite image");return false;}
    const bool ok=m_project.sprites()->replaceSprite(spriteId,image);
    if(ok){++m_revision;emit changed();message(QString("Sprite %1 replaced").arg(spriteId));}
    else message(m_project.sprites()->errorString());
    return ok;
}
int EditorBackend::addSprite(const QString &fileUrl){
    if(!loaded())return 0;QImage image(path(fileUrl));
    if(image.isNull()){message("Cannot read sprite image");return 0;}
    const int id=m_project.sprites()->addSprite(image);
    if(id){++m_revision;emit changed();message(QString("Sprite %1 added").arg(id));}
    else message(m_project.sprites()->errorString());
    return id;
}
bool EditorBackend::removeSprite(int spriteId){
    if(!loaded())return false;const bool ok=m_project.sprites()->removeSprite(spriteId);
    if(ok){++m_revision;emit changed();message(QString("Sprite %1 cleared; IDs were not shifted").arg(spriteId));}
    return ok;
}
