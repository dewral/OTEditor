#include "editorbackend.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <climits>

bool EditorBackend::exportPng(const QString &url,bool sheet){
    if(!loaded()||m_selected<0)return false;
    const QString target=path(url);
    const bool ok=exportPngAt(target,sheet,m_selected);
    message(ok?"PNG exported: "+target:"PNG export failed");
    return ok;
}
bool EditorBackend::exportPngAt(const QString &filePath,bool sheet,int row){
    const QImage output=renderObjectAt(sheet,row);
    return !output.isNull() && output.save(filePath,"PNG");
}
QImage EditorBackend::renderObjectAt(bool sheet,int row){
    const auto item=m_project.dat()->objectAt(m_category,row);
    if(!item)return {};
    const bool outfitSheet=sheet && m_category==1;
    const int groupCount=outfitSheet && !item->frame_groups.empty()?int(item->frame_groups.size()):1;
    const int spriteSize=m_project.spriteSize();
    qint64 outputWidth=0,outputHeight=0;
    for(int groupIndex=0;groupIndex<groupCount;++groupIndex){
        const ClientFrameGroup *group=outfitSheet && !item->frame_groups.empty()
            ? &item->frame_groups[size_t(groupIndex)] : nullptr;
        const qint64 cellWidth=qint64(group?group->width:item->width)*spriteSize;
        const qint64 cellHeight=qint64(group?group->height:item->height)*spriteSize;
        const int frames=sheet?(group?group->frames:item->frames):1;
        const int patterns=outfitSheet
            ? int(group?group->pattern_x:item->pattern_x)*int(group?group->pattern_y:item->pattern_y)*int(group?group->pattern_z:item->pattern_z)
            : 1;
        outputWidth=qMax(outputWidth,cellWidth*(outfitSheet?patterns:frames));
        outputHeight+=cellHeight*(outfitSheet?frames:1);
    }
    if(outputWidth<1||outputHeight<1||outputWidth>INT_MAX||outputHeight>INT_MAX
        ||outputWidth*outputHeight>128ll*1024*1024)return {};
    QImage output(int(outputWidth),int(outputHeight),QImage::Format_RGBA8888);
    if(output.isNull())return {};
    output.fill(Qt::transparent);
    QPainter painter(&output);
    int groupY=0;
    for(int groupIndex=0;groupIndex<groupCount;++groupIndex){
        const ClientFrameGroup *group=outfitSheet && !item->frame_groups.empty()
            ? &item->frame_groups[size_t(groupIndex)] : nullptr;
        const int cellWidth=int(group?group->width:item->width)*spriteSize;
        const int cellHeight=int(group?group->height:item->height)*spriteSize;
        const int frames=sheet?(group?group->frames:item->frames):1;
        const int patterns=outfitSheet
            ? int(group?group->pattern_x:item->pattern_x)*int(group?group->pattern_y:item->pattern_y)*int(group?group->pattern_z:item->pattern_z)
            : 1;
        for(int frame=0;frame<frames;++frame)for(int pattern=0;pattern<patterns;++pattern){
            const int previewPattern=outfitSheet?pattern:(m_category==1?2:0);
            const QString source=previewForCategory(m_category,row,frame,previewPattern,groupIndex,-1);
            painter.drawImage((outfitSheet?pattern:frame)*cellWidth,
                              groupY+(outfitSheet?frame:0)*cellHeight,
                              image(source.mid(QStringLiteral("image://itempreview/").size())));
        }
        groupY+=(outfitSheet?frames:1)*cellHeight;
    }
    painter.end();
    return output;
}
QString EditorBackend::defaultExportFolder() const {
    const QString desktop=QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    return desktop.isEmpty()?QDir::homePath():desktop;
}
int EditorBackend::exportObjects(const QString &folderUrl,const QString &name,
                                 const QString &format,bool transparentBackground){
    if(!loaded()||m_selectedRows.isEmpty()||m_compiling)return 0;
    const QString base=name.trimmed();
    if(base.isEmpty()||base=="."||base==".."||base.contains(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")))){
        message(QStringLiteral("Enter a valid export name without path separators."));return 0;
    }
    const QString extension=format.trimmed().toLower();
    if(extension!="png"&&extension!="bmp"&&extension!="jpg"){
        message(QStringLiteral("Unsupported export format: ")+format);return 0;
    }
    const QDir folder(path(folderUrl));
    if(!folder.exists()){message(QStringLiteral("Output folder does not exist."));return 0;}
    const QVariantList rows=selectedRows();
    m_compiling=true;m_compileProgress=0;m_compileStage=QStringLiteral("Exporting objects");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    int exported=0;
    for(int i=0;i<rows.size();++i){
        const int row=rows[i].toInt();
        const auto item=m_project.dat()->objectAt(m_category,row);
        if(item){
            const QString fileBase=rows.size()==1?base:QStringLiteral("%1_%2").arg(base).arg(item->id);
            QString target=folder.filePath(fileBase+"."+extension);
            for(int suffix=2;QFileInfo::exists(target);++suffix)
                target=folder.filePath(QStringLiteral("%1_%2.%3").arg(fileBase).arg(suffix).arg(extension));
            QImage output=renderObjectAt(true,row);
            if(!output.isNull()){
                if(extension!="png"||!transparentBackground){
                    QImage opaque(output.size(),QImage::Format_RGB32);
                    opaque.fill(Qt::white);
                    QPainter painter(&opaque);painter.drawImage(0,0,output);painter.end();
                    output=std::move(opaque);
                }
                if(output.save(target,extension.toUpper().toLatin1().constData()))++exported;
            }
        }
        m_compileProgress=100*(i+1)/rows.size();
        m_compileStage=QStringLiteral("Exporting %1 / %2").arg(i+1).arg(rows.size());
        if(i%8==0||i+1==rows.size()){emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);}
    }
    m_compiling=false;emit compileProgressChanged();
    message(QStringLiteral("Exported %1 of %2 objects to %3").arg(exported).arg(rows.size()).arg(folder.absolutePath()));
    return exported;
}
int EditorBackend::exportSelectedPngs(const QString &folderUrl,bool sheet){
    if(!loaded()||m_selectedRows.isEmpty()||m_compiling)return 0;
    const QDir folder(path(folderUrl));
    if(!folder.exists()){message("Export folder does not exist");return 0;}
    const QVariantList rows=selectedRows();
    const QStringList categories={"item","outfit","effect","missile"};
    m_compiling=true;m_compileProgress=0;m_compileStage=sheet?QStringLiteral("Exporting animation sheets"):QStringLiteral("Exporting objects");emit compileProgressChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    int exported=0;
    for(int i=0;i<rows.size();++i){
        const int row=rows[i].toInt();
        const auto item=m_project.dat()->objectAt(m_category,row);
        if(item){
            const QString base=QStringLiteral("%1_%2_%3").arg(categories[m_category]).arg(item->id).arg(sheet?QStringLiteral("sheet"):QStringLiteral("object"));
            QString target=folder.filePath(base+QStringLiteral(".png"));
            for(int suffix=2;QFileInfo::exists(target);++suffix)target=folder.filePath(QStringLiteral("%1_%2.png").arg(base).arg(suffix));
            if(exportPngAt(target,sheet,row))++exported;
        }
        m_compileProgress=100*(i+1)/rows.size();
        m_compileStage=QStringLiteral("Exporting %1 / %2").arg(i+1).arg(rows.size());
        if(i%8==0||i+1==rows.size()){emit compileProgressChanged();QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);}
    }
    m_compiling=false;emit compileProgressChanged();
    message(QStringLiteral("Exported %1 of %2 %3 to %4").arg(exported).arg(rows.size()).arg(sheet?QStringLiteral("animation sheets"):QStringLiteral("objects")).arg(folder.absolutePath()));
    return exported;
}
bool EditorBackend::exportSprite(const QString &url,int id){
    if(!loaded()||id<1||id>spriteCount())return false;bool ok=m_project.sprites()->spriteImage(id).save(path(url),"PNG");message(ok?"Sprite exported":"Sprite export failed");return ok;
}
