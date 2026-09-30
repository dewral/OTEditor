#include <QtTest>
#include <QJSValue>
#include <QTemporaryDir>
#include <QDataStream>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QClipboard>
#include <QPainter>
#include <QAbstractItemModelTester>
#include <QtEndian>
#include <algorithm>
#include "editorbackend.h"
#include "obdcodec.h"
#include "otfireader.h"
class EditorTests : public QObject {
 Q_OBJECT
private:
 QQuickItem *visualItem(QQuickItem *parent, const QString &name) {
    if(parent->objectName()==name)return parent;
    for(auto child:parent->childItems())if(auto found=visualItem(child,name))return found;
    return nullptr;
 }
 void fixture(const QString &folder) {
    QFile dat(folder+"/Tibia.dat");QVERIFY(dat.open(QIODevice::WriteOnly));QDataStream d(&dat);d.setByteOrder(QDataStream::LittleEndian);
    d<<quint32(0x12345678)<<quint16(101)<<quint16(0)<<quint16(0)<<quint16(1);
    for(int i=0;i<3;++i){d<<quint8(255);for(int j=0;j<7;++j)d<<quint8(1);d<<quint16(1);}dat.close();
    QFile spr(folder+"/Tibia.spr");QVERIFY(spr.open(QIODevice::WriteOnly));QDataStream s(&spr);s.setByteOrder(QDataStream::LittleEndian);
    s<<quint32(0x1234)<<quint16(1)<<quint32(10)<<quint8(255)<<quint8(0)<<quint8(255)<<quint16(3076)<<quint16(0)<<quint16(1024);
    for(int i=0;i<1024;++i)s<<quint8(230)<<quint8(90)<<quint8(20);
 }
private slots:
 void initTestCase() { QQuickStyle::setStyle("Basic"); }
 void droppedImageDetectsItemSizeAndTargetsSpriteCell() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    const int originalSprites=backend.spriteCount();
    QImage sheet(64,64,QImage::Format_ARGB32);
    sheet.fill(Qt::transparent);
    sheet.fill(Qt::red);
    QPainter painter(&sheet);
    painter.fillRect(32,0,32,32,Qt::green);
    painter.fillRect(0,32,32,32,Qt::blue);
    painter.fillRect(32,32,32,32,Qt::yellow);
    painter.end();
    const QString imagePath=dir.path()+"/item-sheet.png";
    QVERIFY(sheet.save(imagePath));
    QVERIFY(backend.importObjectImage(imagePath,0,0,0,0,0,0));
    QCOMPARE(backend.details().value("itemWidth").toInt(),2);
    QCOMPARE(backend.details().value("itemHeight").toInt(),2);
    QCOMPARE(backend.spriteCount(),originalSprites+4);
    auto source=backend.preview(0);
    QImage preview=backend.image(source.mid(QStringLiteral("image://itempreview/").size()));
    QCOMPARE(preview.pixelColor(8,8),QColor(Qt::red));
    QCOMPARE(preview.pixelColor(40,8),QColor(Qt::green));
    QCOMPARE(preview.pixelColor(8,40),QColor(Qt::blue));
    QCOMPARE(preview.pixelColor(40,40),QColor(Qt::yellow));
    QVERIFY(backend.assignSpriteToCell(0,0,0,0,1,1,1));
    QCOMPARE(backend.details().value("spriteIds").toList().at(3).toInt(),1);
    QVERIFY(backend.textureEditPending());
    QVERIFY(backend.resetTextureEdit());
    QVERIFY(!backend.textureEditPending());
    QCOMPARE(backend.details().value("itemWidth").toInt(),1);
    QCOMPARE(backend.spriteCount(),originalSprites);
    QVERIFY(backend.importObjectImage(imagePath,0,0,0,0,0,0));
    QVERIFY(backend.assignSpriteToCell(0,0,0,0,1,1,1));
    QVERIFY(backend.saveTextureEdit());
    QVERIFY(!backend.textureEditPending());
    backend.undo();
    QCOMPARE(backend.details().value("itemWidth").toInt(),1);
    backend.redo();
    QCOMPARE(backend.details().value("itemWidth").toInt(),2);
    QCOMPARE(backend.details().value("spriteIds").toList().at(3).toInt(),1);
    QImage invalid(48,32,QImage::Format_ARGB32); invalid.fill(Qt::cyan);
    const QString invalidPath=dir.path()+"/invalid.png"; QVERIFY(invalid.save(invalidPath));
    QVERIFY(!backend.importObjectImage(invalidPath,0,0,0,0,0,0));
    QCOMPARE(backend.spriteCount(),originalSprites+4);
    QVERIFY(backend.assignSpriteToCell(0,0,0,0,0,0,1));
    QVERIFY(backend.textureEditPending());
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    QVERIFY(!backend.textureEditPending());
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    QCOMPARE(reopened.details().value("itemWidth").toInt(),2);
    QCOMPARE(reopened.details().value("itemHeight").toInt(),2);
    QCOMPARE(reopened.details().value("spriteIds").toList().first().toInt(),1);
 }
 void pixelEditorPaintsAndSavesSprite() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    EditorBackend backend; QVERIFY(backend.openFolder(dir.path(),860));
    QVERIFY(backend.beginPixelEdit(0,0,0,0,0,0));
    QCOMPARE(backend.pixelEditSpriteId(),1);
    QVERIFY(backend.paintPixel(1,2,Qt::blue));
    QCOMPARE(backend.image("pixel-editor").pixelColor(1,2),QColor(Qt::blue));
    backend.resetPixelEdit();
    QVERIFY(backend.image("pixel-editor").pixelColor(1,2)!=QColor(Qt::blue));
    QVERIFY(backend.paintPixel(1,2,Qt::blue));
    QVERIFY(backend.savePixelEdit());
    QVERIFY(!backend.pixelEditActive());
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY(reopened.openFolder(dir.path(),860));
    QCOMPARE(reopened.image(reopened.spriteSource(1).mid(QStringLiteral("image://itempreview/").size())).pixelColor(1,2),QColor(Qt::blue));
    QVERIFY(reopened.assignSprite(0,0));
    const int originalCount=reopened.spriteCount();
    QVERIFY(reopened.beginPixelEdit(0,0,0,0,0,0));
    QCOMPARE(reopened.pixelEditSpriteId(),0);
    QVERIFY(reopened.paintPixel(0,0,Qt::green));
    QVERIFY(reopened.savePixelEdit());
    QCOMPARE(reopened.spriteCount(),originalCount+1);
    QVERIFY(reopened.textureEditPending());
    QVERIFY(reopened.resetTextureEdit());
    QCOMPARE(reopened.spriteCount(),originalCount);
    QCOMPARE(reopened.details().value("spriteIds").toList().first().toInt(),0);
 }
 void droppedOutfitSheetPreservesOtherGroupsAndAcceptsCombinedSheet() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    quint32 signature=0;
    {
        EditorBackend created; QVERIFY(created.createAssetFiles(dir.path(),1098,true,false,false,true));
        signature=created.info().value("signature").toString().toUInt(nullptr,16);
    }
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<signature<<quint16(99)<<quint16(1)<<quint16(0)<<quint16(0);
    out<<quint8(255)<<quint8(2);
    for(int group=0;group<2;++group) {
        out<<quint8(group)<<quint8(1)<<quint8(1)<<quint8(1)
           <<quint8(2)<<quint8(1)<<quint8(1)<<quint8(1);
        out<<quint32(0)<<quint32(0);
    }
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),1098),qPrintable(backend.status()));
    backend.setCategory(1); backend.jump(1);
    QImage groupSheet(64,32,QImage::Format_ARGB32); groupSheet.fill(Qt::red);
    const QString groupPath=dir.path()+"/walking.png"; QVERIFY(groupSheet.save(groupPath));
    QVERIFY(backend.importObjectImage(groupPath,1,0,0,0,0,0));
    auto groups=backend.details().value("frameGroups").toList();
    QCOMPARE(groups.size(),2);
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({0,0}));
    const auto walking=groups[1].toMap().value("spriteIds").toList();
    QCOMPARE(walking.size(),2);
    QVERIFY(walking[0].toInt()>0 && walking[1].toInt()>0);
    QVERIFY(backend.saveTextureEdit());

    QImage combined(64,64,QImage::Format_ARGB32);
    combined.fill(Qt::blue);
    const QString combinedPath=dir.path()+"/combined.png"; QVERIFY(combined.save(combinedPath));
    QVERIFY(backend.importObjectImage(combinedPath,1,0,0,0,0,0));
    const int spritesBeforeReset=backend.spriteCount()-4;
    groups=backend.details().value("frameGroups").toList();
    for(const auto &group:groups) {
        const auto ids=group.toMap().value("spriteIds").toList();
        QCOMPARE(ids.size(),2);
        QVERIFY(ids[0].toInt()>0 && ids[1].toInt()>0);
    }
    QVERIFY(backend.resetTextureEdit());
    QCOMPARE(backend.spriteCount(),spritesBeforeReset);
    groups=backend.details().value("frameGroups").toList();
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({0,0}));
    QCOMPARE(groups[1].toMap().value("spriteIds").toList(),walking);
    QVERIFY(backend.importObjectImage(combinedPath,1,0,0,0,0,0));
    QVERIFY(backend.saveTextureEdit());
    backend.undo();
    groups=backend.details().value("frameGroups").toList();
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({0,0}));
    QCOMPARE(groups[1].toMap().value("spriteIds").toList(),walking);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),1098),qPrintable(reopened.status()));
    reopened.setCategory(1); reopened.jump(1);
    groups=reopened.details().value("frameGroups").toList();
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({0,0}));
    QCOMPARE(groups[1].toMap().value("spriteIds").toList(),walking);
 }
 void removeSelectedItemShiftsFollowingIds() {
    QTemporaryDir dir;QVERIFY(dir.isValid());fixture(dir.path());
    EditorBackend backend;QVERIFY(backend.openFolder(dir.path(),860));
    backend.select(1);QVERIFY(backend.setValue("isStackable",true));
    backend.select(0);QVERIFY(backend.removeObject());
    QCOMPARE(backend.count(),1);
    QCOMPARE(backend.details().value("itemId").toInt(),100);
    QVERIFY(backend.details().value("isStackable").toBool());
    QVERIFY(!backend.canUndo());
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened;QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    QCOMPARE(reopened.count(),1);
    QCOMPARE(reopened.details().value("itemId").toInt(),100);
    QVERIFY(reopened.details().value("isStackable").toBool());
 }
 void categoryObjectsCreateDuplicateAndRemoveRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    for (int category=1; category<=3; ++category) {
        backend.setCategory(category);
        QCOMPARE(backend.count(),category==3?1:0);
        backend.create();
        QCOMPARE(backend.details().value("itemId").toInt(),category==3?2:1);
        backend.create(true);
        QCOMPARE(backend.details().value("itemId").toInt(),category==3?3:2);
        backend.select(category==3?1:0);
        QVERIFY(backend.removeObject());
        QCOMPARE(backend.count(),category==3?2:1);
        QCOMPARE(backend.details().value("itemId").toInt(),category==3?2:1);
    }
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    for (int category=1; category<=3; ++category) {
        reopened.setCategory(category);
        QCOMPARE(reopened.count(),category==3?2:1);
        QCOMPARE(reopened.details().value("itemId").toInt(),1);
    }
 }
 void outfitTexturePatternRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream d(&dat); d.setByteOrder(QDataStream::LittleEndian);
    d<<quint32(0x12345678)<<quint16(99)<<quint16(1)<<quint16(0)<<quint16(0);
    d<<quint8(255)<<quint8(1)<<quint8(1)<<quint8(2)<<quint8(4)<<quint8(1)<<quint8(1)<<quint8(1);
    for(int i=0;i<8;++i)d<<quint16(1);
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    backend.setCategory(1); backend.jump(1);
    QCOMPARE(backend.details().value("layers").toInt(),2);
    QVERIFY(backend.setTextureValue(0,"itemWidth",2));
    QVERIFY(backend.setTextureValue(0,"layers",1));
    QVERIFY(backend.setTextureValue(0,"frames",2));
    QCOMPARE(backend.details().value("spriteIds").toList().size(),16);
    backend.undo(); QCOMPARE(backend.category(),1); QCOMPARE(backend.details().value("frames").toInt(),1);
    backend.redo(); QCOMPARE(backend.details().value("frames").toInt(),2);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    reopened.setCategory(1); reopened.jump(1);
    QCOMPARE(reopened.details().value("itemWidth").toInt(),2);
    QCOMPARE(reopened.details().value("layers").toInt(),1);
    QCOMPARE(reopened.details().value("frames").toInt(),2);
    QCOMPARE(reopened.details().value("spriteIds").toList().size(),16);
 }
 void outfitBonesRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0x12345678)<<quint16(99)<<quint16(1)<<quint16(0)<<quint16(0);
    out<<quint8(5)<<quint8(0x27);
    for(int direction=0;direction<4;++direction)out<<quint16(direction+1)<<quint16(direction+2);
    out<<quint8(255);
    for(int index=0;index<7;++index)out<<quint8(1);
    out<<quint16(1);dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    backend.setCategory(1);
    QVERIFY(backend.details().value("hasBones").toBool());
    QCOMPARE(backend.details().value("boneOffsetX").toList().at(2).toInt(),3);
    QVERIFY(backend.setOutfitBones(true,2,-12,34));
    QVERIFY(backend.textureEditPending());
    QVERIFY(backend.resetTextureEdit());
    QCOMPARE(backend.details().value("boneOffsetX").toList().at(2).toInt(),3);
    QVERIFY(backend.setOutfitBones(true,2,-12,34));
    QVERIFY(backend.saveTextureEdit());
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    reopened.setCategory(1);
    QCOMPARE(reopened.details().value("boneOffsetX").toList().at(2).toInt(),-12);
    QCOMPARE(reopened.details().value("boneOffsetY").toList().at(2).toInt(),34);
    QVERIFY(reopened.setOutfitBones(false,0,0,0));
    QVERIFY(reopened.compile());
    EditorBackend withoutBones; QVERIFY(withoutBones.openFolder(dir.path(),860));
    withoutBones.setCategory(1);
    QVERIFY(!withoutBones.details().value("hasBones").toBool());
 }
 void outfitWalkingGroupTextureRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    EditorBackend created; QVERIFY(created.createAssetFiles(dir.path(),1098,true,false,true,true));
    const quint32 signature=created.info().value("signature").toString().toUInt(nullptr,16);
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<signature<<quint16(99)<<quint16(1)<<quint16(0)<<quint16(0);
    out<<quint8(255)<<quint8(2);
    for(int group=0;group<2;++group){
        out<<quint8(group)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(4)<<quint8(1)<<quint8(1)<<quint8(2);
        out<<quint8(0)<<quint32(0)<<quint8(0);
        for(int frame=0;frame<2;++frame)out<<quint32(100)<<quint32(100);
        for(int sprite=0;sprite<8;++sprite)out<<quint32(0);
    }
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),1098),qPrintable(backend.status()));
    backend.setCategory(1); backend.jump(1);
    QCOMPARE(backend.details().value("frameGroupCount").toInt(),2);
    QVERIFY(backend.setTextureValue(1,"patternX",5));
    auto groups=backend.details().value("frameGroups").toList();
    QCOMPARE(groups[0].toMap().value("patternX").toInt(),4);
    QCOMPARE(groups[1].toMap().value("patternX").toInt(),5);
    QVERIFY(backend.setTextureValue(0,"frameGroupCount",1));
    QCOMPARE(backend.details().value("frameGroupCount").toInt(),1);
    backend.undo(); QCOMPARE(backend.details().value("frameGroupCount").toInt(),2);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),1098),qPrintable(reopened.status()));
    reopened.setCategory(1); reopened.jump(1);
    groups=reopened.details().value("frameGroups").toList();
    QCOMPARE(groups.size(),2);
    QCOMPARE(groups[1].toMap().value("patternX").toInt(),5);
    QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("Backend",&reopened);
    engine.addImageProvider("itempreview",new EditorImageProvider(&reopened));
    engine.load(QUrl::fromLocalFile(QStringLiteral(QT_TESTCASE_SOURCEDIR "/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto inspector=engine.rootObjects().first()->findChild<QObject *>("objectInspector"); QVERIFY(inspector);
    auto surface=inspector->findChild<QQuickItem *>("patternPreviewSurface"); QVERIFY(surface);
    auto repeater=inspector->findChild<QObject *>("patternPreviewRepeater"); QVERIFY(repeater);
    QTRY_COMPARE(repeater->property("count").toInt(),1);
    QCOMPARE(surface->width(),32.0);
    auto tile=visualItem(surface,"patternPreviewTile"); QVERIFY(tile);
    QCOMPARE(tile->property("sourcePattern").toInt(),2);
    QVERIFY(inspector->setProperty("outfitDirection",0));
    QTRY_COMPARE(tile->property("sourcePattern").toInt(),0);
 }
 void frameGroupsConverterSplitsAndRejoinsFrames() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0x12345678)<<quint16(99)<<quint16(2)<<quint16(0)<<quint16(0);
    const auto outfit=[&](int frames,int firstSprite){
        out<<quint8(255)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(4)
           <<quint8(1)<<quint8(1)<<quint8(frames);
        for(int index=0;index<frames*4;++index)out<<quint16(firstSprite+index);
    };
    outfit(3,1);outfit(1,20);dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    QVERIFY(backend.convertFrameGroups(true));
    backend.setCategory(1);backend.jump(1);
    auto groups=backend.details().value("frameGroups").toList();
    QCOMPARE(groups.size(),2);
    QCOMPARE(groups[0].toMap().value("frames").toInt(),1);
    QCOMPARE(groups[1].toMap().value("frames").toInt(),2);
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({1,2,3,4}));
    QCOMPARE(groups[1].toMap().value("spriteIds").toList(),QVariantList({5,6,7,8,9,10,11,12}));
    backend.jump(2);QCOMPARE(backend.details().value("frameGroupCount").toInt(),1);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    reopened.setCategory(1);reopened.jump(1);
    groups=reopened.details().value("frameGroups").toList();
    QCOMPARE(groups.size(),2);
    QCOMPARE(groups[1].toMap().value("spriteIds").toList(),QVariantList({5,6,7,8,9,10,11,12}));
    QVERIFY(reopened.convertFrameGroups(false));
    groups=reopened.details().value("frameGroups").toList();
    QCOMPARE(groups.size(),1);
    QCOMPARE(groups[0].toMap().value("frames").toInt(),3);
    QCOMPARE(groups[0].toMap().value("spriteIds").toList(),QVariantList({1,2,3,4,5,6,7,8,9,10,11,12}));
    QVERIFY2(reopened.compile(),qPrintable(reopened.status()));
    EditorBackend finalProject; QVERIFY2(finalProject.openFolder(dir.path(),860),qPrintable(finalProject.status()));
    finalProject.setCategory(1);finalProject.jump(1);
    QCOMPARE(finalProject.details().value("frames").toInt(),3);
    QCOMPARE(finalProject.details().value("spriteIds").toList(),QVariantList({1,2,3,4,5,6,7,8,9,10,11,12}));
 }
 void frameGroupsConverterRemovesMounts() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    EditorBackend created; QVERIFY(created.createAssetFiles(dir.path(),1098,true,false,false,true));
    const quint32 signature=created.info().value("signature").toString().toUInt(nullptr,16);
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<signature<<quint16(99)<<quint16(1)<<quint16(0)<<quint16(0);
    out<<quint8(255)<<quint8(2);
    for(int group=0;group<2;++group){
        const int frames=group==0?1:2;
        out<<quint8(group)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(4)
           <<quint8(1)<<quint8(2)<<quint8(frames);
        for(int index=0;index<frames*8;++index)out<<quint32(group==0?index+1:index+9);
    }
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),1098),qPrintable(backend.status()));
    QVERIFY(backend.convertFrameGroups(false,true));
    backend.setCategory(1);backend.jump(1);
    QCOMPARE(backend.details().value("patternZ").toInt(),1);
    QCOMPARE(backend.details().value("spriteIds").toList(),QVariantList({1,2,3,4,9,10,11,12,17,18,19,20}));
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),1098),qPrintable(reopened.status()));
    reopened.setCategory(1);reopened.jump(1);
    QCOMPARE(reopened.details().value("patternZ").toInt(),1);
    QCOMPARE(reopened.details().value("spriteIds").toList(),QVariantList({1,2,3,4,9,10,11,12,17,18,19,20}));
 }
 void itemAndEffectFrameDurationsInTexture() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    EditorBackend created; QVERIFY(created.createAssetFiles(dir.path(),1098,true,false,true,true));
    const quint32 signature=created.info().value("signature").toString().toUInt(nullptr,16);
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<signature<<quint16(100)<<quint16(0)<<quint16(1)<<quint16(0);
    auto writeAnimated=[&](int frames){
        out<<quint8(255)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(frames);
        out<<quint8(0)<<quint32(0)<<quint8(0);
        for(int frame=0;frame<frames;++frame)out<<quint32(100)<<quint32(100);
        for(int frame=0;frame<frames;++frame)out<<quint32(0);
    };
    writeAnimated(8); writeAnimated(3); dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),1098),qPrintable(backend.status()));
    QCOMPARE(backend.details().value("frames").toInt(),8);
    QVERIFY(backend.setFrameDuration(0,100,0,3,80,160));
    QCOMPARE(backend.frameDuration(0,100,0,3).value("minimum").toInt(),80);
    backend.undo(); QCOMPARE(backend.frameDuration(0,100,0,3).value("minimum").toInt(),100);
    backend.redo(); QCOMPARE(backend.frameDuration(0,100,0,3).value("maximum").toInt(),160);
    QVERIFY(backend.setAnimationSettings(0,100,0,1,4,2));
    QCOMPARE(backend.frameDuration(0,100,0,0).value("mode").toInt(),1);
    QCOMPARE(backend.frameDuration(0,100,0,0).value("loopCount").toInt(),4);
    QCOMPARE(backend.frameDuration(0,100,0,0).value("startFrame").toInt(),2);
    QVERIFY(!backend.setAnimationSettings(0,100,0,1,4,8));
    backend.setCategory(2); backend.jump(1);
    QCOMPARE(backend.details().value("frames").toInt(),3);
    QVERIFY(backend.setFrameDuration(2,1,0,1,120,240));
    QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("Backend",&backend);
    engine.addImageProvider("itempreview",new EditorImageProvider(&backend));
    engine.load(QUrl::fromLocalFile(QStringLiteral(QT_TESTCASE_SOURCEDIR "/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto inspector=engine.rootObjects().first()->findChild<QObject *>("objectInspector"); QVERIFY(inspector);
    auto minimum=inspector->findChild<QObject *>("minimumFrameDuration"); QVERIFY(minimum);
    auto maximum=inspector->findChild<QObject *>("maximumFrameDuration"); QVERIFY(maximum);
    QVERIFY(inspector->setProperty("previewFrame",1));
    QTRY_COMPARE(minimum->property("value").toInt(),120);
    QCOMPARE(maximum->property("value").toInt(),240);
    QVERIFY(minimum->property("visible").toBool());
    backend.setCategory(0); backend.jump(100);
    QVERIFY(inspector->setProperty("previewFrame",3));
    QTRY_COMPARE(minimum->property("value").toInt(),80);
    QCOMPARE(maximum->property("value").toInt(),160);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),1098),qPrintable(reopened.status()));
    QCOMPARE(reopened.frameDuration(0,100,0,3).value("maximum").toInt(),160);
    QCOMPARE(reopened.frameDuration(2,1,0,1).value("minimum").toInt(),120);
    QCOMPARE(reopened.frameDuration(0,100,0,0).value("loopCount").toInt(),4);
 }
 void frameDurationsConverterRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QFile dat(dir.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat); out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0x12345678)<<quint16(100)<<quint16(1)<<quint16(1)<<quint16(1);
    const auto writeAnimated=[&](int frames){
        out<<quint8(255)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(1)
           <<quint8(1)<<quint8(1)<<quint8(frames);
        for(int frame=0;frame<frames;++frame)out<<quint16(1);
    };
    writeAnimated(3);writeAnimated(2);writeAnimated(2);writeAnimated(2);
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(dir.path(),860),qPrintable(backend.status()));
    QVERIFY(!backend.info().value("durations").toBool());
    QVERIFY(backend.convertFrameDurations(true,80,160));
    QVERIFY(backend.info().value("durations").toBool());
    QCOMPARE(backend.frameDuration(0,100,0,2).value("maximum").toInt(),160);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    OtfiReader otfi; QVERIFY(otfi.loadFromFolder(dir.path())); QVERIFY(otfi.frameDurations());
    EditorBackend reopened; QVERIFY2(reopened.openFolder(dir.path(),860),qPrintable(reopened.status()));
    for(const auto category : {0,1,2,3}) {
        const int id=category==0?100:1;
        QCOMPARE(reopened.frameDuration(category,id,0,1).value("minimum").toInt(),80);
        QCOMPARE(reopened.frameDuration(category,id,0,1).value("maximum").toInt(),160);
    }
    QVERIFY(reopened.convertFrameDurations(false));
    QVERIFY2(reopened.compile(),qPrintable(reopened.status()));
    OtfiReader withoutDurations; QVERIFY(withoutDurations.loadFromFolder(dir.path()));
    QVERIFY(!withoutDurations.frameDurations());
    EditorBackend finalProject; QVERIFY2(finalProject.openFolder(dir.path(),860),qPrintable(finalProject.status()));
    QCOMPARE(finalProject.details().value("frames").toInt(),3);
    QVERIFY(!finalProject.info().value("durations").toBool());
 }
 void realCrossVersionBulkImport() {
    const QString sourceDir=qEnvironmentVariable("OTE_TEST_IMPORT_SOURCE");
    if(sourceDir.isEmpty())QSKIP("Set OTE_TEST_IMPORT_SOURCE for a real 10.98 bulk import test");
    QTemporaryDir targetDir;QVERIFY(targetDir.isValid());
    EditorBackend target;QVERIFY(target.createAssetFiles(targetDir.path(),772,false,false,false,false,32));
    QCOMPARE(target.importItemGraphicsRange(sourceDir,1098,5090,10089,5090),5000);
    QVERIFY2(target.compile(),qPrintable(target.status()));
    EditorBackend reopened;QVERIFY2(reopened.openFolder(targetDir.path(),772),qPrintable(reopened.status()));
    reopened.jump(10089);
    QCOMPARE(reopened.details().value("itemId").toInt(),10089);
 }
 void realCrossVersionFullRangePreflight() {
    const QString sourceDir=qEnvironmentVariable("OTE_TEST_IMPORT_SOURCE");
    if(sourceDir.isEmpty())QSKIP("Set OTE_TEST_IMPORT_SOURCE for a real 10.98 range preflight");
    QTemporaryDir targetDir;QVERIFY(targetDir.isValid());
    EditorBackend target;QVERIFY(target.createAssetFiles(targetDir.path(),772,false,false,false,false,32));
    const int count=target.importItemGraphicsRange(sourceDir,1098,5090,55117,5090);
    if(count==0){
        QVERIFY2(target.status().contains("free sprite IDs"),qPrintable(target.status()));
        QCOMPARE(target.spriteCount(),0);
        QCOMPARE(target.count(),0);
    } else QCOMPARE(count,50028);
 }
 void crossVersionObjectImportCreatesMissingIds() {
    QTemporaryDir sourceDir, targetDir;
    QVERIFY(sourceDir.isValid());QVERIFY(targetDir.isValid());
    EditorBackend source;
    QVERIFY(source.createAssetFiles(sourceDir.path(),1098,true,false,true,true,32));
    source.create();
    QCOMPARE(source.details().value("itemId").toInt(),100);
    QVERIFY(source.setValue("isStackable",true));
    QVERIFY(source.setValue("hasLight",true));
    QVERIFY(source.setValue("lightColor",42));
    QVERIFY(source.setValue("isTranslucent",true));
    QVERIFY(source.setValue("forceUse",true));
    QVERIFY(source.setValue("hasMarket",true));
    QImage picture(32,32,QImage::Format_RGBA8888);picture.fill(QColor(70,130,210));
    const QString png=sourceDir.path()+"/import.png";QVERIFY(picture.save(png));
    const int sprite=source.addSprite(png);QVERIFY(sprite>0);QVERIFY(source.assignSprite(0,sprite));
    QVERIFY2(source.compile(),qPrintable(source.status()));

    EditorBackend target;
    QVERIFY(target.createAssetFiles(targetDir.path(),772,false,false,false,false,32));
    QCOMPARE(target.importItemGraphicsRange(sourceDir.path(),1098,100,100,55000),1);
    QCOMPARE(target.info().value("items").toInt(),54901);
    target.jump(55000);
    QCOMPARE(target.details().value("itemId").toInt(),55000);
    QVERIFY(target.details().value("isStackable").toBool());
    QVERIFY(target.details().value("hasLight").toBool());
    QCOMPARE(target.details().value("lightColor").toInt(),42);
    QVERIFY(!target.details().value("isTranslucent").toBool());
    QVERIFY(target.details().value("forceUse").toBool());
    QVERIFY(!target.details().value("hasMarket").toBool());
    QVERIFY2(target.compile(),qPrintable(target.status()));
    EditorBackend reopened;
    QVERIFY2(reopened.openFolder(targetDir.path(),772),qPrintable(reopened.status()));
    QCOMPARE(reopened.count(),54901);
    reopened.jump(55000);
    QCOMPARE(reopened.details().value("itemId").toInt(),55000);
    QVERIFY(reopened.details().value("isStackable").toBool());
    QVERIFY(reopened.details().value("hasLight").toBool());
    QCOMPARE(reopened.details().value("lightColor").toInt(),42);
    QVERIFY(!reopened.details().value("isTranslucent").toBool());
    QVERIFY(reopened.details().value("forceUse").toBool());
    QVERIFY(!reopened.details().value("hasMarket").toBool());
    QCOMPARE(reopened.details().value("spriteIds").toList().size(),1);
    QVERIFY(reopened.details().value("spriteIds").toList().at(0).toUInt()>0);
    reopened.jump(54999);
    QCOMPARE(reopened.details().value("spriteIds").toList().at(0).toUInt(),quint32(0));
 }
 void slicerAndSpriteOptimizer() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QImage sheet(65,33,QImage::Format_RGBA8888);sheet.fill(QColor(230,90,20));
    const QString sheetPath=dir.path()+"/sheet.png";QVERIFY(sheet.save(sheetPath));
    EditorBackend backend;QCOMPARE(backend.sliceImage(sheetPath,dir.path(),32),6);
    QCOMPARE(backend.sliceImage(sheetPath,dir.path(),32),0);
    QImage edge(dir.path()+"/tile_0001_0002.png");QCOMPARE(edge.size(),QSize(32,32));
    QCOMPARE(edge.pixelColor(1,1).alpha(),0);
    QVERIFY(backend.openFolder(dir.path(),860));
    QImage duplicate(32,32,QImage::Format_RGBA8888);duplicate.fill(QColor(230,90,20));
    const QString duplicatePath=dir.path()+"/duplicate.png";QVERIFY(duplicate.save(duplicatePath));
    const int newId=backend.addSprite(duplicatePath);QCOMPARE(newId,2);
    QVERIFY(backend.setValue("patternX",2));QVERIFY(backend.assignSprite(1,newId));
    const auto result=backend.optimizeSprites();
    QVERIFY(result.value("redirected").toInt()>=1);
    QCOMPARE(backend.details().value("spriteIds").toList().at(1).toInt(),1);
    const auto compacted=backend.optimizeSprites(true);
    QCOMPARE(compacted.value("newCount").toInt(),1);
    QCOMPARE(backend.spriteCount(),1);
    QVERIFY(backend.compile());
    EditorBackend reopened;QVERIFY(reopened.openFolder(dir.path(),860));
    QCOMPARE(reopened.details().value("spriteIds").toList().at(1).toInt(),1);
    QCOMPARE(reopened.spriteCount(),1);
    QCOMPARE(reopened.importSheetSprites(sheetPath,true),6);
    QCOMPARE(reopened.spriteCount(),7);
 }
 void interactiveSlicerMatchesObjectBuilderCutOrder() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    QImage sheet(64,64,QImage::Format_ARGB32);sheet.fill(Qt::transparent);
    sheet.fill(QColor(255,0,255));
    sheet.setPixelColor(1,1,QColor(255,0,0));
    sheet.setPixelColor(1,33,QColor(0,255,0));
    sheet.setPixelColor(33,1,QColor(0,0,255));
    const QString source=dir.path()+"/slicer.png";QVERIFY(sheet.save(source));
    EditorBackend backend;QVERIFY(backend.slicerOpen(source));
    QCOMPARE(backend.slicerWidth(),64);QCOMPARE(backend.slicerHeight(),64);
    QCOMPARE(backend.slicerCut(0,0,2,2,32,false),3);
    QCOMPARE(backend.slicerCount(),3);
    QCOMPARE(backend.image("slicer/tile/0").pixelColor(1,1),QColor(255,0,0));
    QCOMPARE(backend.image("slicer/tile/1").pixelColor(1,1),QColor(0,255,0));
    QCOMPARE(backend.image("slicer/tile/2").pixelColor(1,1),QColor(0,0,255));
    QCOMPARE(backend.image("slicer/tile/0").pixelColor(0,0).alpha(),0);
    QCOMPARE(backend.slicerCut(0,0,2,2,32,false),0);
    QVERIFY(backend.openFolder(dir.path(),860));
    QCOMPARE(backend.slicerImport(),3);QCOMPARE(backend.spriteCount(),4);
    QCOMPARE(backend.image("sprite/2").pixelColor(1,1),QColor(255,0,0));
    QCOMPARE(backend.image("sprite/3").pixelColor(1,1),QColor(0,255,0));
    QCOMPARE(backend.image("sprite/4").pixelColor(1,1),QColor(0,0,255));
    backend.slicerClear();QCOMPARE(backend.slicerCount(),0);
    QCOMPARE(backend.slicerCut(0,0,2,2,32,true),4);
    QCOMPARE(backend.image("slicer/tile/3").pixelColor(0,0).alpha(),0);
    QVERIFY(backend.slicerTransform("rotateRight"));
    QCOMPARE(backend.slicerWidth(),64);QCOMPARE(backend.slicerHeight(),64);
    QCOMPARE(backend.image("slicer/source").pixelColor(62,1),QColor(255,0,0));
 }
 void outfitAnimationToolsRoundTrip() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QString source=dir.path()+"/outfits.dat";
    QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
    out << quint32(0x12345678) << quint16(99) << quint16(1) << quint16(0) << quint16(0);
    out << quint8(255) << quint8(2);
    for(int group=0;group<2;++group){
        out << quint8(group) << quint8(1) << quint8(1) << quint8(1) << quint8(4) << quint8(1) << quint8(1) << quint8(2);
        out << quint8(0) << quint32(0) << quint8(0);
        for(int frame=0;frame<2;++frame)out << quint32(100) << quint32(100);
        for(int sprite=0;sprite<8;++sprite)out << quint32(0);
    }
    file.close();
    DatReader dat;dat.setClientVersion(1057);
    QVERIFY2(dat.loadFile(source),qPrintable(dat.errorString()));
    QCOMPARE(dat.objectAt(1,0)->frame_groups.size(),size_t(2));
    QVERIFY(dat.setFrameDuration(1,0,1,1,150,250));
    QVERIFY(dat.saveFile(dir.path()+"/edited.dat"));
    DatReader reopened;reopened.setClientVersion(1057);
    QVERIFY2(reopened.loadFile(dir.path()+"/edited.dat"),qPrintable(reopened.errorString()));
    const auto &duration=reopened.objectAt(1,0)->frame_groups[1].animation_data;
    QCOMPARE(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(duration.constData()+14)),quint32(150));
    QCOMPARE(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(duration.constData()+18)),quint32(250));
    QVERIFY(reopened.duplicateFrame(1,0,1,1));
    QCOMPARE(reopened.objectAt(1,0)->frame_groups[1].frames,quint8(3));
    QVERIFY(reopened.deleteFrame(1,0,1,2));
    QCOMPARE(reopened.objectAt(1,0)->frame_groups[1].frames,quint8(2));
    QCOMPARE(reopened.convertFrameGroups(false),1);
    reopened.setOtfiOverrides(true,true,true,false);
    QVERIFY(reopened.saveFile(dir.path()+"/single.dat"));
    DatReader single;single.setClientVersion(1057);single.setOtfiOverrides(true,true,true,false);
    QVERIFY2(single.loadFile(dir.path()+"/single.dat"),qPrintable(single.errorString()));
    QCOMPARE(single.objectAt(1,0)->frame_groups.size(),size_t(1));
 }
 void itemFrameDuplicationRoundTrip() {
    QTemporaryDir dir;QVERIFY(dir.isValid());fixture(dir.path());
    EditorBackend backend;QVERIFY(backend.openFolder(dir.path(),860));
    QVERIFY(backend.duplicateFrame(0,100,0,0));
    QCOMPARE(backend.details().value("frames").toInt(),2);
    QVERIFY(backend.compile());
    EditorBackend reopened;QVERIFY(reopened.openFolder(dir.path(),860));
    QCOMPARE(reopened.details().value("frames").toInt(),2);
    QVERIFY(reopened.deleteFrame(0,100,0,1));
    QCOMPARE(reopened.details().value("frames").toInt(),1);
 }
 void compileProgress() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    EditorBackend backend; QVERIFY(backend.openFolder(dir.path(),860));
    QSignalSpy progress(&backend,&EditorBackend::compileProgressChanged);
    QVERIFY(backend.compile());
    QVERIFY(progress.size()>=4);
    QVERIFY(!backend.compiling());
    QCOMPARE(backend.compileProgress(),100);
    QCOMPARE(backend.compileStage(),QStringLiteral("Complete"));
    progress.clear();
    QVERIFY(backend.compileAs(dir.path()+"/compiled"));
    QVERIFY(progress.size()>=4);
    QVERIFY(!backend.compiling());
    QCOMPARE(backend.compileProgress(),100);
 }
 void detectFolderVersion() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QString client = dir.path()+"/client-8.60";
    QVERIFY(QDir().mkpath(client));
    fixture(client);
    EditorBackend backend;
    QCOMPARE(backend.detectFolderVersion(client),860);
    QVERIFY(backend.inspectFolder(client,backend.detectFolderVersion(client)).value("ok").toBool());
    const QString unknown=dir.path()+"/client";
    QVERIFY(QDir().mkpath(unknown));fixture(unknown);
    QCOMPARE(backend.detectFolderVersion(unknown),0);
    const QString official=dir.path()+"/official";
    QVERIFY(QDir().mkpath(official));fixture(official);
    QFile officialDat(official+"/Tibia.dat"),officialSpr(official+"/Tibia.spr");
    QVERIFY(officialDat.open(QIODevice::ReadWrite));
    QVERIFY(officialSpr.open(QIODevice::ReadWrite));
    QDataStream datSignature(&officialDat),sprSignature(&officialSpr);
    datSignature.setByteOrder(QDataStream::LittleEndian);
    sprSignature.setByteOrder(QDataStream::LittleEndian);
    datSignature<<quint32(0x4C28B721);sprSignature<<quint32(0x4C220594);
    officialDat.close();officialSpr.close();
    QCOMPARE(backend.detectFolderVersion(official),860);
    const QString realClient = qEnvironmentVariable("OTEDITOR_TEST_CLIENT_FOLDER");
    if (!realClient.isEmpty()) {
        const int detected = backend.detectFolderVersion(realClient);
        QVERIFY2(detected>0, qPrintable(QStringLiteral("Could not detect DAT format in %1").arg(realClient)));
        const int expected = qEnvironmentVariableIntValue("OTEDITOR_TEST_CLIENT_VERSION");
        if (expected>0) QCOMPARE(detected,expected);
        QVERIFY2(backend.inspectFolder(realClient,detected).value("ok").toBool(),
                 qPrintable(QStringLiteral("Detected DAT format %1 could not open %2").arg(detected).arg(realClient)));
    }
 }
 void otbTools() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    OtbReader otb; otb.newFile();
    QVERIFY(otb.saveFile(dir.path()+"/items.otb"));
    EditorBackend backend; QVERIFY(backend.openFolder(dir.path(),860));
    QVERIFY(backend.info().value("otb").toBool());
    QCOMPARE(backend.createMissingOtbItems(),1);
    QCOMPARE(backend.createMissingOtbItems(),0);
    QVERIFY(backend.reloadItemAttributes()>=0);
    QVERIFY(backend.serverId()>=0);
    QVERIFY(backend.setServerAttributes({{"name",QStringLiteral("Test Sword")},{"speed",321},{"pickupable",true}}));
    QVERIFY(backend.copyServerAttributes());
    QVERIFY(backend.setServerAttributes({{"name",QStringLiteral("Changed")},{"speed",10},{"pickupable",false}}));
    QVERIFY(backend.pasteServerAttributes());
    QCOMPARE(backend.serverAttributes().value("name").toString(),QStringLiteral("Test Sword"));
    QCOMPARE(backend.serverAttributes().value("speed").toInt(),321);
    QVERIFY(backend.serverAttributes().value("pickupable").toBool());
    QVERIFY(!backend.setServerAttributes({{"lightLevel",256}}));
    QVERIFY(!backend.setServerAttributes({{"speed",65536}}));
    QVERIFY(!backend.setServerAttributes({{"stackOrder",128}}));
    QVERIFY(!backend.setServerAttributes({{"groupId",16}}));
    QVERIFY(!backend.setServerAttributes({{"serverId",65536}}));
    QVERIFY(!backend.setServerAttributes({{"clientId",99}}));
    const int originalServerId=backend.serverId();
    QVERIFY(backend.setServerAttributes({{"serverId",400}}));
    QCOMPARE(backend.serverId(),400);
    QVERIFY(backend.setServerAttributes({{"serverId",originalServerId}}));
    QVERIFY(backend.reloadSelectedOtbItem());
    QVERIFY(backend.setServerAttributes({{"speed",321},{"pickupable",true}}));
    QVERIFY(backend.updateOtbVersion(3,860,7));
    QCOMPARE(backend.info().value("otbBuild").toInt(),7);
    const QString otbCopy=dir.path()+"/items-copy.otb";
    QVERIFY(backend.saveOtbFile(otbCopy));
    QVERIFY(QFile::exists(otbCopy));
    QVERIFY(backend.dirty());
    const QString comparisonPath=dir.path()+"/comparison.otb";
    QVERIFY(QFile::copy(dir.path()+"/items.otb",comparisonPath));
    const auto comparison=backend.compareOtbFile(comparisonPath);
    QVERIFY(!comparison.contains("error"));
    QVERIFY(comparison.value("changed").toInt()>0);
    QVERIFY(backend.saveOtbFile());
    QVERIFY(backend.compile());
    EditorBackend reopened; QVERIFY(reopened.openFolder(dir.path(),860));
    QCOMPARE(reopened.serverAttributes().value("name").toString(),QStringLiteral("Test Sword"));
    QCOMPARE(reopened.serverAttributes().value("speed").toInt(),321);
    reopened.create();
    QCOMPARE(reopened.serverId(),-1);
    QVERIFY(reopened.createServerItem());
    QVERIFY(reopened.serverId()>=0);
    QVERIFY(!reopened.createServerItem());
 }
 void importsObjectBuilderV2Obd() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    const QByteArray sample=QByteArray::fromBase64(
        "XQAAgAD//////////wBkAAT2PgfskHDfiK8f20IGZa4qkh9W1rEeLCltEYHuRiuxAz1rw9nmJh1w//kmwAA=");
    const QString filePath=dir.path()+"/object-v2.obd";
    QFile file(filePath); QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(sample),sample.size()); file.close();
    EditorBackend backend; QVERIFY(backend.openFolder(dir.path(),860));
    QVERIFY2(backend.importObd(filePath),qPrintable(backend.status()));
    QCOMPARE(backend.spriteCount(),2);
    QCOMPARE(backend.details().value("spriteIds").toList().first().toInt(),2);
    const QImage preview=backend.image(backend.preview(0).mid(QStringLiteral("image://itempreview/").size()));
    QCOMPARE(preview.pixelColor(0,0),QColor(220,40,10));
 }
 void importsObjectBuilderV1Obd() {
    const QByteArray sample=QByteArray::fromBase64(
        "XQAAgAD//////////wACANWw2d+FUxhtWBbCY8FZfFZDONmZmNWp4D5zuBrVOCJAQVq/ZKO3lm/E6Yarnmo3EtNNtCQy///SlQAA");
    ObdObject parsed;QString error;
    QVERIFY2(ObdCodec::decode(sample,parsed,&error),qPrintable(error));
    QCOMPARE(parsed.clientVersion,772);
    QCOMPARE(parsed.category,0);
    QVERIFY(parsed.item.is_ground);
    QCOMPARE(parsed.item.ground_speed,300);
    QVERIFY(parsed.item.is_writable);
    QCOMPARE(parsed.item.max_text_length,50);
    QVERIFY(parsed.item.floor_change);
    QCOMPARE(parsed.sprites.size(),1);
    QCOMPARE(parsed.sprites.first().pixelColor(0,0),QColor(40,180,20));
 }
 void realObdFilesDecode() {
    const QString folder=qEnvironmentVariable("OTE_TEST_OBD_FOLDER");
    if (folder.isEmpty()) QSKIP("Set OTE_TEST_OBD_FOLDER for existing OBD files");
    const QStringList names=QDir(folder).entryList({"*.obd"},QDir::Files,QDir::Name);
    QVERIFY(!names.isEmpty());
    for (const QString &name:names) {
        QFile file(QDir(folder).filePath(name));QVERIFY(file.open(QIODevice::ReadOnly));
        ObdObject parsed;QString error;
        QVERIFY2(ObdCodec::decode(file.readAll(),parsed,&error),qPrintable(name+": "+error));
        QVERIFY2(!parsed.sprites.isEmpty(),qPrintable(name));
        QCOMPARE(parsed.item.sprite_ids.size(),size_t(parsed.sprites.size()));
    }
 }
 void realItemsXmlPreservesOtherEntries() {
    const QString source=qEnvironmentVariable("OTE_TEST_XML_FILE");
    if (source.isEmpty()) QSKIP("Set OTE_TEST_XML_FILE for a real items.xml test");
    QTemporaryDir output;QVERIFY(output.isValid());
    ItemsXmlReader xml;QVERIFY(xml.loadFile(source));
    const int originalCount=xml.count();
    const QString name=xml.nameForServerId(100);
    QVERIFY(xml.setAttributeForServerId(100,"oteditorTest","ok"));
    const QString copy=output.path()+"/items.xml";
    QVERIFY(xml.saveCopy(copy));
    ItemsXmlReader reopened;QVERIFY(reopened.loadFile(copy));
    QCOMPARE(reopened.count(),originalCount);
    QCOMPARE(reopened.nameForServerId(100),name);
    QCOMPARE(reopened.attributesForServerId(100).value("oteditorTest").toString(),"ok");
 }
 void legacyObdFlagVersions() {
    struct Sample {int version;const char *base64;};
    const Sample samples[]={
        {720,"XQAAgAD//////////wBoAHx8A8O/lIv0Q/GmPR+qLWhTgtfdBr+dCmxtmbhHv+sHehOwI3m0uvPu9D+pmOX//WnQAA=="},
        {740,"XQAAgAD//////////wByAHx8A8O/lIv4CUK6ufQkzGxr6khUO23O29ppE2VgYhYszWTLyOinQqbq6p2R/r7/+/PAAA=="},
        {772,"XQAAgAD//////////wACANWw2d+FUxiVFpuDWMvK84m3X9SLOIBHypvXRvB9o76eRApSEL0sFkwHemTJL///5dyQAA=="},
        {800,"XQAAgAD//////////wAQALyD4IPpgE3MQ0exKpSyjmlXsmvNEz2pP8YWa/iUjLz5rWt8ObU0ilLcJH2sI0SgVXg5f//a72AA"},
        {960,"XQAAgAD//////////wBgALx8A8O/lIv87XZwA3lBPgKDrFqORqzKJO6NwVah6AeU34rA6XP++7I8BlbtSocV9P/7L0QA"},
        {1098,"XQAAgAD//////////wAlASwAbInHEFctOcutUmgNSg6nkKydCkJchUIww6wip0k1eg+rvKjjdlhGw/zUYH//0zpAAA=="}
    };
    for (const auto &sample:samples) {
        ObdObject parsed;QString error;
        QVERIFY2(ObdCodec::decode(QByteArray::fromBase64(sample.base64),parsed,&error),qPrintable(error));
        QCOMPARE(parsed.clientVersion,sample.version);
        QCOMPARE(parsed.sprites.size(),1);
        if (sample.version==720) {QVERIFY(parsed.item.has_offset);QCOMPARE(parsed.item.offset_x,8);}
        if (sample.version==740) QVERIFY(parsed.item.is_hangable);
        if (sample.version==772) QVERIFY(parsed.item.floor_change);
        if (sample.version==800) {QVERIFY(parsed.item.extra_properties.value("chargeable").toBool());QCOMPARE(parsed.item.offset_y,-2);}
        if (sample.version==960) QCOMPARE(parsed.item.extra_properties.value("clothSlot").toInt(),7);
        if (sample.version==1098) QVERIFY(parsed.item.extra_properties.value("noMoveAnimation").toBool());
    }
 }
 void convertsProjectsAcrossClientVersions() {
    QTemporaryDir sourceDir,targetDir,backDir;
    QVERIFY(sourceDir.isValid());QVERIFY(targetDir.isValid());QVERIFY(backDir.isValid());
    fixture(sourceDir.path());
    EditorBackend source;QVERIFY(source.openFolder(sourceDir.path(),860));
    QImage replacement(32,32,QImage::Format_RGBA8888);
    replacement.fill(QColor(17,143,201));
    const QString replacementPath=sourceDir.path()+"/replacement.png";
    QVERIFY(replacement.save(replacementPath));
    QVERIFY(source.replaceSprite(1,replacementPath));
    QVERIFY2(source.convertProject(targetDir.path(),1098),qPrintable(source.status()));
    EditorBackend modern;QVERIFY2(modern.openFolder(targetDir.path(),1098),qPrintable(modern.status()));
    QCOMPARE(modern.count(),source.count());
    QCOMPARE(modern.details().value("itemId").toInt(),100);
    QCOMPARE(modern.spriteCount(),1);
    QCOMPARE(modern.image(modern.spriteSource(1).mid(QStringLiteral("image://itempreview/").size())).pixelColor(0,0),QColor(17,143,201));
    QVERIFY2(modern.convertProject(backDir.path(),772),qPrintable(modern.status()));
    EditorBackend legacy;QVERIFY2(legacy.openFolder(backDir.path(),772),qPrintable(legacy.status()));
    QCOMPARE(legacy.count(),source.count());
    QCOMPARE(legacy.spriteCount(),1);
    QCOMPARE(legacy.details().value("itemId").toInt(),100);
    QVERIFY(!source.convertProject(sourceDir.path(),1098));
    QVERIFY(!source.convertProject(targetDir.path(),1098));
 }
 void serverFileCopiesKeepActivePathsAndPendingChanges() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    OtbReader otb; otb.newFile();
    const QString otbPath=dir.path()+"/items.otb";
    QVERIFY(otb.saveFile(otbPath));
    QVERIFY(otb.createItem(100)>=0);
    QVERIFY(otb.dirty());
    QVERIFY(otb.saveCopy(dir.path()+"/items-copy.otb"));
    QCOMPARE(otb.filePath(),otbPath);
    QVERIFY(otb.dirty());

    const QString xmlPath=dir.path()+"/items.xml";
    QFile xmlFile(xmlPath); QVERIFY(xmlFile.open(QIODevice::WriteOnly));
    QVERIFY(xmlFile.write("<items><item id=\"100\" name=\"Original\"/></items>")>0);
    xmlFile.close();
    ItemsXmlReader xml; QVERIFY(xml.loadFile(xmlPath));
    xml.setNameForServerId(100,"Changed");
    QVERIFY(xml.dirty());
    const QString copyPath=dir.path()+"/items-copy.xml";
    QVERIFY(xml.saveCopy(copyPath));
    QCOMPARE(xml.filePath(),xmlPath);
    QVERIFY(xml.dirty());
    QFile copy(copyPath); QVERIFY(copy.open(QIODevice::ReadOnly));
    QVERIFY(copy.readAll().contains("Changed"));
    const QString xmlAs=dir.path()+"/renamed.xml";
    QVERIFY(xml.saveFile(xmlAs));
    QCOMPARE(xml.filePath(),xmlAs);
    QVERIFY(!xml.dirty());

    ProjectModel project;
    QVERIFY(project.open(dir.path(),860,false));
    const QString activeOtb=dir.path()+"/renamed.otb";
    QVERIFY(project.saveOtbAs(activeOtb));
    QCOMPARE(project.otb()->filePath(),activeOtb);
    const QString activeXml=dir.path()+"/active.xml";
    QVERIFY(project.saveItemsXmlAs(activeXml));
    QCOMPARE(project.itemsXml()->filePath(),activeXml);
    QVERIFY(project.compile());
    QVERIFY(QFile::exists(activeOtb));
    QVERIFY(QFile::exists(activeXml));
 }
 void editsXmlAttributesWithoutChangingOtherItems() {
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const QString source=dir.path()+"/items.xml";
    QFile file(source);QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("<items><item id=\"100\" name=\"Sword\" article=\"a\"><attribute key=\"type\" value=\"weapon\"/><attribute key=\"weight\" value=\"30\"/></item><item fromid=\"200\" toid=\"202\" name=\"Stone\"><attribute key=\"type\" value=\"ground\"/></item></items>")>0);
    file.close();
    ItemsXmlReader xml;QVERIFY(xml.loadFile(source));
    QCOMPARE(xml.attributesForServerId(100).value("weight").toString(),"30");
    QVERIFY(xml.setAttributeForServerId(100,"weight","45"));
    QVERIFY(xml.setAttributeForServerId(100,"@article","the"));
    QVERIFY(xml.removeAttributeForServerId(100,"type"));
    QVERIFY(xml.setAttributeForServerId(100,"@plural","Swords"));
    QVERIFY(xml.setAttributeForServerId(201,"speed","2"));
    const QString copy=dir.path()+"/copy.xml";
    QVERIFY(xml.saveCopy(copy));QVERIFY(xml.dirty());
    ItemsXmlReader reopened;QVERIFY(reopened.loadFile(copy));
    QCOMPARE(reopened.attributesForServerId(100).value("weight").toString(),"45");
    QCOMPARE(reopened.attributesForServerId(100).value("@article").toString(),"the");
    QCOMPARE(reopened.attributesForServerId(100).value("@plural").toString(),"Swords");
    QVERIFY(!reopened.attributesForServerId(100).contains("type"));
    QCOMPARE(reopened.attributesForServerId(200).value("type").toString(),"ground");
    QCOMPARE(reopened.attributesForServerId(201).value("speed").toString(),"2");
    QCOMPARE(reopened.attributesForServerId(202).value("type").toString(),"ground");
    QVERIFY(!xml.removeAttributeForServerId(201,"type"));
 }
 void createOtbWhenMissing() {
    QTemporaryDir client; QTemporaryDir server;
    QVERIFY(client.isValid()); QVERIFY(server.isValid()); fixture(client.path());
    EditorBackend backend;
    QVERIFY(backend.openFolder(client.path(),860,false,server.path()));
    QVERIFY(!backend.info().value("otb").toBool());
    QVERIFY(backend.createOtbFile());
    QVERIFY(QFile::exists(server.path()+"/items.otb"));
    QVERIFY(backend.info().value("otb").toBool());
    QVERIFY(backend.serverId()>=0);
    QVERIFY(!backend.createOtbFile());
    QCOMPARE(backend.createMissingOtbItems(),1);
    QVERIFY(backend.compile());
    EditorBackend reopened;
    QVERIFY(reopened.openFolder(client.path(),860,false,server.path()));
    QVERIFY(reopened.info().value("otb").toBool());
    reopened.select(1);
    QVERIFY(reopened.serverId()>=0);
    QTemporaryDir clientOnly;
    QVERIFY(clientOnly.isValid()); fixture(clientOnly.path());
    DatReader expanded;
    expanded.setClientVersion(860);
    QVERIFY(expanded.loadFile(clientOnly.path()+"/Tibia.dat"));
    for(int i=0;i<600;++i) QVERIFY(expanded.createItem()>=0);
    QVERIFY(expanded.saveFile(clientOnly.path()+"/Tibia.dat"));
    EditorBackend local;
    QVERIFY(local.openFolder(clientOnly.path(),860));
    QVERIFY(local.createOtbFile());
    QVERIFY(QFile::exists(clientOnly.path()+"/items.otb"));
    QSignalSpy progress(&local,&EditorBackend::compileProgressChanged);
    QCOMPARE(local.createMissingOtbItems(),601);
    QVERIFY(progress.count()>=4);
    QCOMPARE(local.compileProgress(),100);
    QVERIFY(!local.compiling());
    QVERIFY(local.compile());
    EditorBackend localReopened;
    QVERIFY(localReopened.openFolder(clientOnly.path(),860));
    localReopened.jump(701);
    QVERIFY(localReopened.serverId()>=0);
 }
 void createAssetFiles() {
    {
        QTemporaryDir custom;QVERIFY(custom.isValid());
        EditorBackend backend;
        QVERIFY(backend.createAssetFiles(custom.path(),800,true,false,true,true));
        QVERIFY(backend.info().value("extended").toBool());
        QVERIFY(backend.info().value("durations").toBool());
        QVERIFY(backend.info().value("groups").toBool());
        QVERIFY(backend.compile());
        EditorBackend reopened;QVERIFY(reopened.openFolder(custom.path(),800));
        QVERIFY(reopened.info().value("extended").toBool());
        QVERIFY(reopened.info().value("durations").toBool());
        QVERIFY(reopened.info().value("groups").toBool());
    }
    for (const int version : {772, 1310}) {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        EditorBackend backend;
        const bool modern=version >= 960;
        QVERIFY(backend.createAssetFiles(dir.path(),version,modern,false,modern,modern));
        QVERIFY(backend.loaded()); QCOMPARE(backend.count(),0); QCOMPARE(backend.spriteCount(),0);
        QVERIFY(QFile::exists(dir.path()+"/Tibia.dat"));
        QVERIFY(QFile::exists(dir.path()+"/Tibia.spr"));
        QVERIFY(QFile::exists(dir.path()+"/Tibia.otfi"));
        QVERIFY(!backend.createAssetFiles(dir.path(),version,modern,false,modern,modern));
        backend.create(); QCOMPARE(backend.count(),1);
        QVERIFY(backend.compile());
        EditorBackend reopened; QVERIFY(reopened.openFolder(dir.path(),version));
        QCOMPARE(reopened.count(),1);
    }
    for (const int spriteSize : {64,128,256}) {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        EditorBackend backend;
        QVERIFY(backend.createAssetFiles(dir.path(),1310,true,false,true,true,spriteSize));
        QCOMPARE(backend.info().value("spriteDimension").toString(),QString("%1x%1").arg(spriteSize));
        const auto inspection=backend.inspectFolder(dir.path(),1310);
        QVERIFY(inspection.value("ok").toBool());
        QCOMPARE(inspection.value("spriteSize").toInt(),spriteSize);
        QCOMPARE(inspection.value("sprites").toInt(),0);
        QImage image(spriteSize,spriteSize,QImage::Format_RGBA8888); image.fill(Qt::transparent);
        image.setPixelColor(spriteSize-1,spriteSize-1,QColor(230,90,20));
        const QString png=dir.path()+"/sprite.png"; QVERIFY(image.save(png));
        QCOMPARE(backend.addSprite(png),1);
        QVERIFY(backend.compile());
        SprReader sprites; sprites.setSpriteSize(spriteSize);
        QVERIFY(sprites.loadFile(dir.path()+"/Tibia.spr",0,true,false));
        QCOMPARE(sprites.spriteImage(1).size(),QSize(spriteSize,spriteSize));
        QCOMPARE(sprites.spriteImage(1).pixelColor(spriteSize-1,spriteSize-1),QColor(230,90,20));
    }
    QTemporaryDir overriddenDir; QVERIFY(overriddenDir.isValid());
    {
        EditorBackend initial;
        QVERIFY(initial.createAssetFiles(overriddenDir.path(),1310,true,false,true,true));
    }
    EditorBackend overridden;
    QCOMPARE(overridden.inspectFolder(overriddenDir.path(),1310,false,QString(),64).value("spriteSize").toInt(),64);
    QVERIFY(overridden.openFolder(overriddenDir.path(),1310,false,QString(),64));
    QVERIFY2(overridden.compile(),qPrintable(overridden.status()));
    EditorBackend reopened; QVERIFY(reopened.openFolder(overriddenDir.path(),1310));
    QCOMPARE(reopened.info().value("spriteDimension").toString(),QString("64x64"));
 }
 void objectClipboardAndReplacement() {
    QTemporaryDir dir;fixture(dir.path());EditorBackend backend;
    QVERIFY(!backend.copyObjectPart("object"));QVERIFY(backend.openFolder(dir.path(),860));
    QVERIFY(!backend.pasteObjectPart("object"));
    QVERIFY(backend.setValue("isStackable",true));QVERIFY(backend.setValue("patternX",2));
    QVERIFY(backend.copyObjectPart("object"));QVERIFY(backend.copyObjectPart("patterns"));QVERIFY(backend.copyObjectPart("properties"));
    backend.select(1);QVERIFY(backend.pasteObjectPart("properties"));
    QVERIFY(backend.details()["isStackable"].toBool());QCOMPARE(backend.details()["patternX"].toInt(),1);
    backend.undo();QVERIFY(!backend.details()["isStackable"].toBool());
    QVERIFY(backend.pasteObjectPart("patterns"));QCOMPARE(backend.details()["patternX"].toInt(),2);
    QVERIFY(!backend.details()["isStackable"].toBool());backend.undo();
    QVERIFY(backend.pasteObjectPart("object"));QCOMPARE(backend.details()["itemId"].toInt(),101);
    QVERIFY(backend.details()["isStackable"].toBool());QCOMPARE(backend.details()["patternX"].toInt(),2);
    backend.undo();QCOMPARE(backend.details()["patternX"].toInt(),1);backend.redo();QCOMPARE(backend.details()["patternX"].toInt(),2);
    backend.copyId();QCOMPARE(QGuiApplication::clipboard()->text(),QString("101"));QCOMPARE(backend.serverId(),-1);
    QVERIFY(!backend.replaceObject(999));QVERIFY(backend.replaceObject(100));QCOMPARE(backend.details()["itemId"].toInt(),101);
    QVERIFY(backend.compile());EditorBackend reopened;QVERIFY(reopened.openFolder(dir.path(),860));reopened.select(1);
    QVERIFY(reopened.details()["isStackable"].toBool());QCOMPARE(reopened.details()["patternX"].toInt(),2);
    backend.setCategory(3);QVERIFY(!backend.pasteObjectPart("object"));QVERIFY(!backend.copyObjectPart("object"));
    QVERIFY(backend.openFolder(dir.path(),860));QVERIFY(!backend.objectClipboard()["object"].toBool());
 }
 void multiSelectionAndSheetExport() {
    QTemporaryDir client; QTemporaryDir output;
    QVERIFY(client.isValid()); QVERIFY(output.isValid()); fixture(client.path());
    EditorBackend backend; QVERIFY(backend.openFolder(client.path(),860));
    for(int i=0;i<3;++i)backend.create();
    backend.select(0);
    backend.selectWithModifiers(3,Qt::ShiftModifier);
    QCOMPARE(backend.selectedCount(),4);
    backend.selectWithModifiers(1,Qt::ControlModifier);
    QCOMPARE(backend.selectedCount(),3);
    backend.activateSelected(2);
    QCOMPARE(backend.selected(),2);
    backend.selectWithModifiers(4,Qt::ControlModifier|Qt::ShiftModifier);
    QCOMPARE(backend.selectedCount(),5);
    QCOMPARE(backend.exportSelectedPngs(output.path(),true),5);
    for(int id=100;id<=104;++id){
        const QString file=output.path()+QString("/item_%1_sheet.png").arg(id);
        QVERIFY(QFile::exists(file));
        QVERIFY(!QImage(file).isNull());
    }
    QCOMPARE(backend.exportSelectedPngs(output.path(),true),5);
    QVERIFY(QFile::exists(output.path()+"/item_100_sheet_2.png"));
    backend.select(1);
    QCOMPARE(backend.selectedCount(),1);
 }
 void bulkItemOperationsAndComparison() {
    QTemporaryDir dir; QVERIFY(dir.isValid()); fixture(dir.path());
    EditorBackend backend; QVERIFY(backend.openFolder(dir.path(),860));
    backend.create();
    backend.select(0);
    backend.selectWithModifiers(2,Qt::ShiftModifier);
    QCOMPARE(backend.selectedCount(),3);
    QCOMPARE(backend.bulkSetItemAttribute("isStackable",true),3);
    QCOMPARE(backend.bulkReplaceObjects(100),3);
    backend.select(0);
    backend.selectWithModifiers(1,Qt::ControlModifier);
    const auto comparison=backend.compareSelectedObjects();
    QVERIFY(!comparison.contains("error"));
    QCOMPARE(comparison.value("firstId").toInt(),100);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY(reopened.openFolder(dir.path(),860));
    for (int row=0;row<3;++row) {
        reopened.select(row);
        QVERIFY(reopened.details().value("isStackable").toBool());
    }
 }
 void exportAllObjectsAndSprites() {
    QTemporaryDir client,objects,sprites;
    QVERIFY(client.isValid()); QVERIFY(objects.isValid()); QVERIFY(sprites.isValid());
    fixture(client.path());
    EditorBackend backend; QVERIFY(backend.openFolder(client.path(),860));
    QCOMPARE(backend.exportAllObjects(objects.path(),false),3);
    QVERIFY(QFile::exists(objects.path()+"/item_100_object.png"));
    QVERIFY(QFile::exists(objects.path()+"/missile_1_object.png"));
    QCOMPARE(backend.exportAllSprites(sprites.path()),1);
    QVERIFY(QFile::exists(sprites.path()+"/sprite_1.png"));
    QCOMPARE(backend.compileProgress(),100);
 }
 void objectBuilderObdRoundTrip() {
    QTemporaryDir client,output; QVERIFY(client.isValid()); QVERIFY(output.isValid());
    fixture(client.path());
    EditorBackend backend; QVERIFY(backend.openFolder(client.path(),860));
    QVERIFY(backend.setValue("isStackable",true));
    QCOMPARE(backend.exportObjects(output.path(),"sample","obd",true),1);
    const QString file=output.path()+"/sample.obd";
    QVERIFY(QFileInfo(file).size()>0);
    backend.select(1);
    QVERIFY(backend.importObd(file));
    QVERIFY(backend.textureEditPending());
    QVERIFY(backend.details().value("isStackable").toBool());
    QCOMPARE(backend.details().value("spriteIds").toList().first().toInt(),2);
    QVERIFY(backend.resetTextureEdit());
    QCOMPARE(backend.spriteCount(),1);
    QVERIFY(backend.importObd(file));
    QVERIFY(backend.saveTextureEdit());
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY(reopened.openFolder(client.path(),860));
    reopened.select(1);
    QVERIFY(reopened.details().value("isStackable").toBool());
    QCOMPARE(reopened.details().value("spriteIds").toList().first().toInt(),2);
 }
 void outfitObdKeepsDirections() {
    QTemporaryDir client,output; QVERIFY(client.isValid()); QVERIFY(output.isValid());
    fixture(client.path());
    QFile dat(client.path()+"/Tibia.dat"); QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat);out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0x12345678)<<quint16(99)<<quint16(2)<<quint16(0)<<quint16(0);
    for(int outfit=0;outfit<2;++outfit) {
        out<<quint8(255)<<quint8(1)<<quint8(1)<<quint8(1)<<quint8(4)<<quint8(1)<<quint8(1)<<quint8(1);
        for(int direction=0;direction<4;++direction)out<<quint16(1);
    }
    dat.close();
    EditorBackend backend; QVERIFY2(backend.openFolder(client.path(),860),qPrintable(backend.status()));
    backend.setCategory(1);
    QCOMPARE(backend.exportObjects(output.path(),"outfit","obd",true),1);
    backend.select(1);
    QVERIFY(backend.importObd(output.path()+"/outfit.obd"));
    QCOMPARE(backend.details().value("patternX").toInt(),4);
    QCOMPARE(backend.details().value("spriteIds").toList().size(),4);
    QVERIFY(backend.compile());
    EditorBackend reopened; QVERIFY(reopened.openFolder(client.path(),860));
    reopened.setCategory(1);reopened.select(1);
    QCOMPARE(reopened.details().value("spriteIds").toList().size(),4);
 }
 void mergeClientProjects() {
    QTemporaryDir target,source; QVERIFY(target.isValid()); QVERIFY(source.isValid());
    fixture(target.path()); fixture(source.path());
    EditorBackend backend; QVERIFY(backend.openFolder(target.path(),860));
    QCOMPARE(backend.mergeProject(source.path(),1098),0);
    QCOMPARE(backend.spriteCount(),1);
    QCOMPARE(backend.mergeProject(source.path(),860),3);
    QCOMPARE(backend.count(),4);
    QCOMPARE(backend.details().value("itemId").toInt(),102);
    QCOMPARE(backend.spriteCount(),2);
    QCOMPARE(backend.details().value("spriteIds").toList().first().toInt(),2);
    backend.setCategory(3);
    QCOMPARE(backend.count(),2);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend reopened; QVERIFY2(reopened.openFolder(target.path(),860),qPrintable(reopened.status()));
    QCOMPARE(reopened.count(),4);
    reopened.setCategory(3);
    QCOMPARE(reopened.count(),2);
 }
 void outfitSheetKeepsDirectionsInColumnsAndGroupsInRows() {
    QTemporaryDir client,output;QVERIFY(client.isValid());QVERIFY(output.isValid());
    {
        EditorBackend creator;
        QVERIFY(creator.createAssetFiles(client.path(),1057,true,false,true,true,32));
    }
    SprReader sprites;
    QVERIFY(sprites.loadFile(client.path()+"/Tibia.spr",0,true,false));
    for(int id=1;id<=36;++id){
        QImage sprite(32,32,QImage::Format_RGBA8888);
        sprite.fill(QColor((id*37)%256,(id*71)%256,(id*113)%256));
        QCOMPARE(sprites.addSprite(sprite),id);
    }
    QVERIFY(sprites.saveFile());
    QFile dat(client.path()+"/Tibia.dat");QVERIFY(dat.open(QIODevice::WriteOnly));
    QDataStream out(&dat);out.setByteOrder(QDataStream::LittleEndian);
    out << quint32(0x12345678) << quint16(99) << quint16(1) << quint16(0) << quint16(0);
    out << quint8(255) << quint8(2);
    int nextSprite=1;
    for(int group=0;group<2;++group){
        const int frames=group==0?1:8;
        out << quint8(group) << quint8(1) << quint8(1) << quint8(1)
            << quint8(4) << quint8(1) << quint8(1) << quint8(frames);
        if(frames>1){
            out << quint8(0) << quint32(0) << quint8(0);
            for(int frame=0;frame<frames;++frame)out << quint32(100) << quint32(100);
        }
        for(int slot=0;slot<frames*4;++slot)out << quint32(nextSprite++);
    }
    dat.close();
    EditorBackend backend;
    QVERIFY2(backend.openFolder(client.path(),1057),qPrintable(backend.status()));
    backend.setCategory(1);
    const QString file=output.path()+"/outfit.png";
    QVERIFY(backend.exportPng(file,true));
    const QImage sheet(file);
    QCOMPARE(sheet.size(),QSize(128,288));
    for(int group=0;group<2;++group){
        const int frames=group==0?1:8;
        for(int frame=0;frame<frames;++frame)for(int pattern=0;pattern<4;++pattern){
            const int id=(group==0?0:4)+frame*4+pattern+1;
            const QColor expected((id*37)%256,(id*71)%256,(id*113)%256);
            QCOMPARE(sheet.pixelColor(pattern*32+16,((group==0?0:1)+frame)*32+16),expected);
        }
    }
    QCOMPARE(backend.exportSelectedPngs(output.path(),true),1);
    QCOMPARE(QImage(output.path()+"/outfit_1_sheet.png"),sheet);
    QCOMPARE(backend.exportObjects(output.path(),"exported","png",true),1);
    QCOMPARE(QImage(output.path()+"/exported.png"),sheet);
 }
 void exportDialogFormatsAndSelection() {
    QTemporaryDir client,output;QVERIFY(client.isValid());QVERIFY(output.isValid());
    fixture(client.path());
    EditorBackend backend;QVERIFY(backend.openFolder(client.path(),860));
    QImage sparse(32,32,QImage::Format_RGBA8888);
    sparse.fill(Qt::transparent);sparse.setPixelColor(16,16,Qt::red);
    const QString input=client.path()+"/sparse.png";QVERIFY(sparse.save(input));
    const int sprite=backend.addSprite(input);QVERIFY(sprite>0);
    QVERIFY(backend.assignSprite(0,sprite));
    QCOMPARE(backend.exportObjects(output.path(),"sample","png",true),1);
    QCOMPARE(QImage(output.path()+"/sample.png").pixelColor(0,0).alpha(),0);
    QCOMPARE(backend.exportObjects(output.path(),"sample","png",false),1);
    QCOMPARE(QImage(output.path()+"/sample_2.png").pixelColor(0,0),QColor(Qt::white));
    QCOMPARE(backend.exportObjects(output.path(),"sample","jpg",true),1);
    QCOMPARE(QImage(output.path()+"/sample.jpg").pixelColor(0,0),QColor(Qt::white));
    backend.selectWithModifiers(1,Qt::ControlModifier);
    QCOMPARE(backend.exportObjects(output.path(),"batch","bmp",false),2);
    QVERIFY(!QImage(output.path()+"/batch_100.bmp").isNull());
    QVERIFY(!QImage(output.path()+"/batch_101.bmp").isNull());
    QCOMPARE(backend.exportObjects(output.path(),"bad/name","png",true),0);
    QCOMPARE(backend.exportObjects(output.path(),"batch","obd",true),2);
    QVERIFY(QFileInfo(output.path()+"/batch_100.obd").size()>0);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Backend",&backend);
    engine.addImageProvider("itempreview",new EditorImageProvider(&backend));
    engine.load(QUrl::fromLocalFile(QStringLiteral(QT_TESTCASE_SOURCEDIR "/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *dialog=engine.rootObjects().first()->findChild<QObject *>("objectExportDialog");
    QVERIFY(dialog);
    QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
    QCoreApplication::processEvents();
    QVERIFY(dialog->property("visible").toBool());
    auto *nameField=dialog->findChild<QObject *>("exportNameField");
    QVERIFY(nameField);
    QCOMPARE(nameField->property("text").toString(),QStringLiteral("item"));
 }
 void largeRangeSelection() {
    QTemporaryDir client; QVERIFY(client.isValid()); fixture(client.path());
    DatReader dat; dat.setClientVersion(860);
    QVERIFY(dat.loadFile(client.path()+"/Tibia.dat"));
    for(int i=0;i<2200;++i)QVERIFY(dat.createItem()>=0);
    QVERIFY(dat.saveFile(client.path()+"/Tibia.dat"));
    EditorBackend backend; QVERIFY(backend.openFolder(client.path(),860));
    backend.select(0);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Backend",&backend);
    engine.addImageProvider("itempreview",new EditorImageProvider(&backend));
    engine.load(QUrl::fromLocalFile(QStringLiteral(QT_TESTCASE_SOURCEDIR "/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    QSignalSpy updates(&backend,&EditorBackend::selectionChanged);
    QElapsedTimer timer; timer.start();
    backend.selectWithModifiers(2000,Qt::ShiftModifier);
    QCoreApplication::processEvents();
    QCOMPARE(backend.selectedCount(),2001);
    QVERIFY(backend.isSelected(1500));
    QCOMPARE(updates.count(),1);
    QVERIFY2(timer.elapsed()<3000,"Selecting a 2000-object range took too long");
 }
 void roundTripAndUndo() {
    QTemporaryDir dir;QTemporaryDir compiled;fixture(dir.path());EditorBackend backend;
    QAbstractItemModelTester modelTester(&backend, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QVERIFY(backend.openFolder(dir.path(),860));QCOMPARE(backend.count(),2);QCOMPARE(backend.spriteCount(),1);
    QVERIFY(backend.setValue("isStackable",true));QVERIFY(backend.dirty());QVERIFY(backend.details()["isStackable"].toBool());
    backend.undo();QVERIFY(!backend.details()["isStackable"].toBool());backend.redo();QVERIFY(backend.details()["isStackable"].toBool());
    QVERIFY(backend.compileAs(compiled.path()));QVERIFY(backend.compile());QVERIFY(!backend.dirty());
    QVERIFY(QFile::exists(compiled.path()+"/Tibia.dat"));QVERIFY(QFile::exists(compiled.path()+"/Tibia.spr"));QVERIFY(QFile::exists(compiled.path()+"/Tibia.otfi"));
    DatReader loaded;loaded.setClientVersion(860);loaded.setOtfiOverrides(true,false,false,false);QVERIFY(loaded.loadFile(compiled.path()+"/Tibia.dat"));QVERIFY(loaded.detailsAt(0)["isStackable"].toBool());
    QCOMPARE(loaded.categoryCount(3),1);
    QVERIFY(backend.exportPng(dir.path()+"/preview.png"));QImage png(dir.path()+"/preview.png");QCOMPARE(png.size(),QSize(32,32));QCOMPARE(png.pixelColor(0,0),QColor(230,90,20));
    backend.clearObject();QCOMPARE(backend.details()["spriteIds"].toList()[0].toInt(),0);backend.undo();QCOMPARE(backend.details()["spriteIds"].toList()[0].toInt(),1);
    backend.create(true);QCOMPARE(backend.count(),3);QCOMPARE(backend.details()["itemId"].toInt(),102);
    backend.filter("102",false);QCOMPARE(backend.count(),1);
    QVERIFY(!backend.assignSprite(0,2));QVERIFY(backend.assignSprite(0,0));backend.undo();
    QVERIFY(!backend.openFolder(dir.path()+"/missing",860));QVERIFY(backend.loaded());QCOMPARE(backend.details()["itemId"].toInt(),102);
 }
 void qmlWindowAndDialogs() {
    QTemporaryDir dir;fixture(dir.path());EditorBackend backend;
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("Backend",&backend);
    engine.addImageProvider("itempreview",new EditorImageProvider(&backend));
    QStringList warnings;
    connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors){for(const auto &e:errors)warnings<<e.toString();});
    engine.load(QUrl::fromLocalFile(QStringLiteral(QT_TESTCASE_SOURCEDIR "/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());auto root=engine.rootObjects().first();
    auto aboutDialog=root->findChild<QObject *>("aboutDialog"); QVERIFY(aboutDialog);
    auto supportLogo=root->findChild<QObject *>("midhemSupportLogo"); QVERIFY(supportLogo);
    QVERIFY(QMetaObject::invokeMethod(aboutDialog,"open"));
    QTRY_COMPARE(supportLogo->property("status").toInt(),1);
    QMetaObject::invokeMethod(aboutDialog,"close");
    const QStringList panelActions={"showPreviewPanelAction","showObjectsPanelAction","showSpritesPanelAction"};
    const QStringList panelNames={"previewPanel","objectsPanel","spritesPanel"};
    for(int i=0;i<panelActions.size();++i) {
        auto action=root->findChild<QObject *>(panelActions.at(i)); QVERIFY(action);
        auto panel=root->findChild<QObject *>(panelNames.at(i)); QVERIFY(panel);
        QVERIFY(action->property("checked").toBool()); QVERIFY(panel->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(action,"trigger"));
        QVERIFY(!panel->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(action,"trigger"));
        QVERIFY(panel->property("visible").toBool());
    }
    auto gridAction=root->findChild<QObject *>("showObjectsGridAction"); QVERIFY(gridAction);
    QCOMPARE(root->property("browserMode").toInt(),1);
    auto findAction=root->findChild<QObject *>("findToolAction"); QVERIFY(findAction);
    auto createOtbAction=root->findChild<QObject *>("createMissingOtbItemsAction"); QVERIFY(createOtbAction);
    auto reloadOtbAction=root->findChild<QObject *>("reloadItemAttributesAction"); QVERIFY(reloadOtbAction);
    QVERIFY(!findAction->property("enabled").toBool());
    QVERIFY(!createOtbAction->property("enabled").toBool());
    QVERIFY(!reloadOtbAction->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(gridAction,"trigger"));
    QCOMPARE(root->property("browserMode").toInt(),0);
    QVERIFY(QMetaObject::invokeMethod(gridAction,"trigger"));
    QCOMPARE(root->property("browserMode").toInt(),1);
    auto shortcutWindow=qobject_cast<QQuickWindow *>(root); QVERIFY(shortcutWindow);
    shortcutWindow->requestActivate(); QTest::qWait(30);
    QTest::keyClick(shortcutWindow,Qt::Key_F2,Qt::ShiftModifier);
    QTRY_VERIFY(!root->findChild<QObject *>("previewPanel")->property("visible").toBool());
    QTest::keyClick(shortcutWindow,Qt::Key_F2,Qt::ShiftModifier);
    QTRY_VERIFY(root->findChild<QObject *>("previewPanel")->property("visible").toBool());
    for (const auto key : {Qt::Key_F3,Qt::Key_F4}) {
        const char *name=key==Qt::Key_F3 ? "objectsPanel" : "spritesPanel";
        QTest::keyClick(shortcutWindow,key,Qt::ShiftModifier);
        QTRY_VERIFY(!root->findChild<QObject *>(name)->property("visible").toBool());
        QTest::keyClick(shortcutWindow,key,Qt::ShiftModifier);
        QTRY_VERIFY(root->findChild<QObject *>(name)->property("visible").toBool());
    }
    QTest::keyClick(shortcutWindow,Qt::Key_F5,Qt::ShiftModifier);
    QTRY_COMPARE(root->property("browserMode").toInt(),0);
    QTest::keyClick(shortcutWindow,Qt::Key_F5,Qt::ShiftModifier);
    QTRY_COMPARE(root->property("browserMode").toInt(),1);
    QVERIFY(QMetaObject::invokeMethod(root,"newProject"));
    auto newDialog=root->findChild<QObject *>("newAssetDialog"); QVERIFY(newDialog);
    QVERIFY(newDialog->property("visible").toBool());
    QMetaObject::invokeMethod(newDialog,"close");
    QVERIFY(QMetaObject::invokeMethod(root,"openProject"));QTest::qWait(100);
    auto dialog=root->findChild<QObject *>("loadDialog");QVERIFY(dialog);QVERIFY(dialog->property("visible").toBool());
    QMetaObject::invokeMethod(dialog,"close");
    QVERIFY(backend.openFolder(dir.path(),860));
    auto patternPreview=root->findChild<QObject *>("patternPreviewRepeater"); QVERIFY(patternPreview);
    auto patternSurface=root->findChild<QQuickItem *>("patternPreviewSurface"); QVERIFY(patternSurface);
    QVERIFY(backend.setValue("patternX",4));
    QTRY_COMPARE(patternPreview->property("count").toInt(),4);
    QCOMPARE(patternSurface->width(),4*32.0);
    QVERIFY(backend.setValue("patternY",2));
    QTRY_COMPARE(patternPreview->property("count").toInt(),8);
    QCOMPARE(patternSurface->height(),2*32.0);
    for(const QString &name : {QStringLiteral("lookTypeDialog"),QStringLiteral("objectViewer"),
                               QStringLiteral("slicerDialog"),QStringLiteral("animationDialog"),
                               QStringLiteral("durationOptimizer"),QStringLiteral("frameDurationsConverter"),QStringLiteral("frameGroupsConverter"),
                               QStringLiteral("spriteOptimizer")}){
        auto tool=root->findChild<QObject *>(name);QVERIFY2(tool,qPrintable(name));
        QVERIFY(QMetaObject::invokeMethod(tool,"open"));
        QTest::qWait(15);
        if(name==QStringLiteral("slicerDialog")) {
            QCOMPARE(tool->property("title").toString(),QStringLiteral("Slicer"));
            QCOMPARE(tool->property("modal").toBool(),true);
            auto dimension=tool->findChild<QObject *>("slicerDimension");
            QVERIFY(dimension);
            for(int index=0;index<4;++index) {
                QVERIFY(dimension->setProperty("currentIndex",index));
                QCOMPARE(tool->property("tilePixels").toInt(),QVector<int>({32,64,128,256}).at(index));
            }
            QVERIFY(dimension->setProperty("currentIndex",0));
            const QString capture=qEnvironmentVariable("OTE_CAPTURE_SLICER");
            if(!capture.isEmpty())QVERIFY(shortcutWindow->grabWindow().save(capture));
        }
        QVERIFY(QMetaObject::invokeMethod(tool,"close"));
    }
    QVERIFY(findAction->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(findAction,"trigger"));
    QCOMPARE(root->property("browserMode").toInt(),2);
    auto findDialog=root->findChild<QObject *>("findObjectsDialog");QVERIFY(findDialog);
    QVERIFY(QMetaObject::invokeMethod(findDialog,"close"));
    root->setProperty("browserMode",1);
    QTest::keyClick(shortcutWindow,Qt::Key_F,Qt::ControlModifier);
    QTRY_COMPARE(root->property("browserMode").toInt(),2);
    QVERIFY(QMetaObject::invokeMethod(findDialog,"close"));
    root->setProperty("browserMode",1);
    auto attributes=root->findChild<QObject *>("objectInspector");QVERIFY(attributes);
    QMetaObject::invokeMethod(attributes,"showProperties");QTest::qWait(100);
    QVERIFY(attributes->property("visible").toBool());
    auto palettePicker=attributes->findChild<QObject *>("paletteColorPicker");QVERIFY(palettePicker);
    QVERIFY(QMetaObject::invokeMethod(attributes,"openColorPicker",Q_ARG(QVariant,QVariant("lightColor"))));
    QVERIFY(palettePicker->property("visible").toBool());
    palettePicker->setProperty("selectedIndex",42);
    QVERIFY(QMetaObject::invokeMethod(palettePicker,"reject"));
    QCOMPARE(attributes->property("changes").value<QJSValue>().toVariant().toMap().size(),0);
    QVERIFY(QMetaObject::invokeMethod(attributes,"openColorPicker",Q_ARG(QVariant,QVariant("lightColor"))));
    palettePicker->setProperty("selectedIndex",42);
    QVERIFY(QMetaObject::invokeMethod(palettePicker,"accept"));
    QVERIFY(QMetaObject::invokeMethod(attributes,"openColorPicker",Q_ARG(QVariant,QVariant("minimapColor"))));
    palettePicker->setProperty("selectedIndex",135);
    QVERIFY(QMetaObject::invokeMethod(palettePicker,"accept"));
    QCOMPARE(backend.details().value("lightColor").toInt(),0);
    QCOMPARE(attributes->property("changes").value<QJSValue>().toVariant().toMap().value("lightColor").toInt(),42);
    QCOMPARE(attributes->property("changes").value<QJSValue>().toVariant().toMap().value("minimapColor").toInt(),135);
    QVERIFY(QMetaObject::invokeMethod(attributes,"saveDraft"));
    QCOMPARE(backend.details().value("lightColor").toInt(),42);
    QCOMPARE(backend.details().value("minimapColor").toInt(),135);
    for(int tab=0;tab<3;++tab){attributes->setProperty("tabIndex",tab);QTest::qWait(50);}
    QVERIFY(QMetaObject::invokeMethod(attributes,"edit",Q_ARG(QVariant,QVariant("groundSpeed")),Q_ARG(QVariant,QVariant(175))));
    QCOMPARE(backend.details().value("groundSpeed").toInt(),0);
    backend.select(1);QTest::qWait(20);
    QCOMPARE(attributes->property("draft").value<QJSValue>().toVariant().toMap().value("itemId").toInt(),101);
    QCOMPARE(backend.details().value("groundSpeed").toInt(),0);
    backend.select(0);QTest::qWait(20);
    QCOMPARE(attributes->property("draft").value<QJSValue>().toVariant().toMap().value("groundSpeed").toInt(),175);
    QMetaObject::invokeMethod(attributes,"resetDraft");QMetaObject::invokeMethod(attributes,"showProperties");
    QCOMPARE(attributes->property("changes").value<QJSValue>().toVariant().toMap().size(),0);
    QVERIFY(QMetaObject::invokeMethod(attributes,"edit",Q_ARG(QVariant,QVariant("groundSpeed")),Q_ARG(QVariant,QVariant(175))));
    QVERIFY(QMetaObject::invokeMethod(attributes,"saveDraft"));QCOMPARE(backend.details().value("groundSpeed").toInt(),175);
    QMetaObject::invokeMethod(attributes,"resetDraft");
    root->setProperty("browserMode",2);QVERIFY(QMetaObject::invokeMethod(findDialog,"open"));
    auto search=root->findChild<QObject *>("searchField");QVERIFY(search);search->setProperty("text","101");QCOMPARE(backend.count(),1);
    QVERIFY(QMetaObject::invokeMethod(findDialog,"close"));
    auto objectSize=root->findChild<QObject *>("objectSize");QVERIFY(objectSize);
    auto objectsGrid=root->findChild<QObject *>("objectsGrid");QVERIFY(objectsGrid);
    root->setProperty("browserMode",1);QTest::qWait(20);
    const qreal initialCellWidth=objectsGrid->property("cellWidth").toReal();
    objectSize->setProperty("value",96);QTest::qWait(20);
    QVERIFY(objectsGrid->property("cellWidth").toReal()>initialCellWidth);
    QCOMPARE(objectsGrid->property("cellHeight").toInt(),116);
    backend.setCategory(3);QTest::qWait(100);backend.setCategory(0);
    auto window=qobject_cast<QQuickWindow *>(root);QVERIFY(window);window->resize(1100,720);QTest::qWait(100);
    backend.filter("",false);backend.select(0);QTest::qWait(100);
    auto cell=visualItem(window->contentItem(),"objectCell101");QVERIFY(cell);
    QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,cell->mapToScene(QPointF(cell->width()/2,cell->height()/2)).toPoint());
    QTest::qWait(100);QCOMPARE(backend.selected(),1);
    auto menu=root->findChild<QObject *>("objectContextMenu");QVERIFY(menu);QVERIFY(menu->property("visible").toBool());
    auto importAction=qobject_cast<QQuickItem *>(root->findChild<QObject *>("importClientObjectsContextAction"));QVERIFY(importAction);
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                      importAction->mapToScene(QPointF(importAction->width()/2,importAction->height()/2)).toPoint());
    auto importDialog=root->findChild<QObject *>("importGraphicsDialog");QVERIFY(importDialog);
    QTRY_VERIFY(importDialog->property("visible").toBool());
    auto targetSpinBox=importDialog->findChild<QObject *>("importTargetFirstSpinBox");QVERIFY(targetSpinBox);
    QCOMPARE(targetSpinBox->property("value").toInt(),101);
    QVERIFY(QMetaObject::invokeMethod(importDialog,"close"));
    attributes->setProperty("tabIndex",0);
    QImage dropped(32,32,QImage::Format_ARGB32);dropped.fill(Qt::green);
    const QString droppedPath=dir.path()+"/dropped.png";QVERIFY(dropped.save(droppedPath));
    const int spriteId=backend.addSprite(droppedPath);QVERIFY(spriteId>0);
    QVERIFY(backend.assignSpriteToCell(0,0,0,0,0,0,spriteId));
    auto saveButton=attributes->findChild<QObject *>("inspectorSaveButton");QVERIFY(saveButton);
    auto resetButton=attributes->findChild<QObject *>("inspectorResetButton");QVERIFY(resetButton);
    QTRY_VERIFY(saveButton->property("enabled").toBool());
    QVERIFY(resetButton->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(attributes,"resetDraft"));
    QVERIFY(!backend.textureEditPending());
    QTRY_VERIFY(!saveButton->property("enabled").toBool());
    QVERIFY(backend.assignSpriteToCell(0,0,0,0,0,0,spriteId));
    QTRY_VERIFY(saveButton->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(attributes,"saveDraft"));
    QVERIFY(!backend.textureEditPending());
    QCOMPARE(backend.details().value("spriteIds").toList().first().toInt(),spriteId);
    QTRY_VERIFY(!saveButton->property("enabled").toBool());
    QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join("\n")));
 }
 void realClientRoundTrip() {
    const QString source=qEnvironmentVariable("OTE_TEST_CLIENT");
    if(source.isEmpty())QSKIP("Set OTE_TEST_CLIENT for a local real-client integration test");
    QTemporaryDir output;EditorBackend backend;
    QVERIFY(backend.openFolder(source,772));
    const auto original=backend.details();const int count=backend.count();
    QVERIFY(backend.setValue("isStackable",!original["isStackable"].toBool()));
    QVERIFY(backend.compileAs(output.path()));QVERIFY(backend.compile());
    DatReader verify;verify.setClientVersion(772);OtfiReader otfi;bool has=otfi.loadFromFolder(source);
    verify.setOtfiOverrides(has,otfi.extended(),otfi.frameDurations(),otfi.frameGroups());
    QVERIFY(verify.loadFile(output.path()+"/Tibia.dat"));QCOMPARE(verify.itemCount(),count);
    QCOMPARE(verify.detailsAt(0)["isStackable"].toBool(),!original["isStackable"].toBool());
    QCOMPARE(verify.detailsAt(0)["spriteIds"],original["spriteIds"]);
    {
        SprReader spriteVerify;QVERIFY(spriteVerify.loadFile(output.path()+"/Tibia.spr",0,otfi.extended(),otfi.transparency()));
        QCOMPARE(spriteVerify.spriteCount(),backend.spriteCount());
    }
    if(QFile::exists(source+"/items.otb"))QVERIFY(QFile::exists(output.path()+"/items.otb"));
    if(QFile::exists(source+"/items.xml"))QVERIFY(QFile::exists(output.path()+"/items.xml"));
    QVERIFY(QFile::exists(output.path()+"/Tibia.otfi"));
    const int beforeImportSprites=backend.spriteCount();
    QVERIFY2(backend.importItemGraphics(source,772,5087),qPrintable(backend.status()));
    QVERIFY(backend.spriteCount()>beforeImportSprites);
    QCOMPARE(backend.importItemGraphicsRange(source,772,5087,5089,100),3);
    QVERIFY2(backend.compile(),qPrintable(backend.status()));
    EditorBackend imported;QVERIFY2(imported.openFolder(output.path(),772),qPrintable(imported.status()));
    QCOMPARE(imported.details().value("itemId").toInt(),100);
    QVERIFY(imported.spriteCount()>beforeImportSprites);
    QVERIFY(imported.details().value("spriteIds").toList().size()>0);
    if(backend.info().value("groups").toBool()){
        QVERIFY(backend.convertFrameGroups(false));
        QVERIFY2(backend.compile(),qPrintable(backend.status()));
        OtfiReader convertedOtfi;QVERIFY(convertedOtfi.loadFromFolder(output.path()));
        QVERIFY(!convertedOtfi.frameGroups());
        EditorBackend converted;QVERIFY2(converted.openFolder(output.path(),772),qPrintable(converted.status()));
        QVERIFY(!converted.info().value("groups").toBool());
    }
 }
 void realClientConversion() {
    const QString source=qEnvironmentVariable("OTE_TEST_CLIENT");
    if (source.isEmpty()) QSKIP("Set OTE_TEST_CLIENT for a real 7.72 conversion test");
    QTemporaryDir output;QVERIFY(output.isValid());
    EditorBackend backend;QVERIFY2(backend.openFolder(source,772),qPrintable(backend.status()));
    const int items=backend.count();const int sprites=backend.spriteCount();
    QVERIFY2(backend.convertProject(output.path(),1098),qPrintable(backend.status()));
    EditorBackend reopened;QVERIFY2(reopened.openFolder(output.path(),1098),qPrintable(reopened.status()));
    QCOMPARE(reopened.count(),items);
    QVERIFY(reopened.spriteCount()>0);
    QVERIFY(reopened.spriteCount()<=sprites);
    QCOMPARE(reopened.details().value("itemId").toInt(),100);
 }
 void spriteWriterRoundTrip() {
    QTemporaryDir source;fixture(source.path());
    SprReader sprites;QVERIFY(sprites.loadFile(source.path()+"/Tibia.spr",0,false,false));
    QCOMPARE(sprites.spriteCount(),1);QCOMPARE(sprites.spriteImage(1).pixelColor(0,0),QColor(230,90,20));
    QImage green(32,32,QImage::Format_RGBA8888);green.fill(QColor(10,220,40));
    QImage blue(32,32,QImage::Format_RGBA8888);blue.fill(QColor(20,60,240));
    QVERIFY(sprites.replaceSprite(1,green));QCOMPARE(sprites.spriteImage(1).pixelColor(0,0),QColor(10,220,40));
    QCOMPARE(sprites.addSprite(blue),2);QVERIFY(sprites.removeSprite(1));QVERIFY(sprites.isDirty());
    QVERIFY(!sprites.saveFile(source.path()+"/missing/output.spr"));QVERIFY(sprites.isDirty());
    QCOMPARE(sprites.spriteImage(2).pixelColor(0,0),QColor(20,60,240));
    const QString output=source.path()+"/written.spr";QVERIFY(sprites.saveFile(output));QVERIFY(!sprites.isDirty());
    SprReader verify;QVERIFY(verify.loadFile(output,0,false,false));QCOMPARE(verify.spriteCount(),2);
    QCOMPARE(verify.spriteImage(1).pixelColor(0,0).alpha(),0);
    QCOMPARE(verify.spriteImage(2).pixelColor(0,0),QColor(20,60,240));
 }
 void backendSpriteImportAndProjectCompile() {
    QTemporaryDir source;QTemporaryDir output;fixture(source.path());EditorBackend backend;QVERIFY(backend.openFolder(source.path(),860));
    QImage image(32,32,QImage::Format_RGBA8888);image.fill(QColor(4,80,170));
    const QString png=source.path()+"/sprite.png";QVERIFY(image.save(png));
    QVERIFY(backend.replaceSprite(1,png));QCOMPARE(backend.addSprite(png),2);QVERIFY(backend.dirty());
    QVERIFY(backend.compileAs(output.path()));
    SprReader verify;QVERIFY(verify.loadFile(output.path()+"/Tibia.spr",0,false,false));QCOMPARE(verify.spriteCount(),2);
    QCOMPARE(verify.spriteImage(1).pixelColor(0,0),QColor(4,80,170));
    QCOMPARE(verify.spriteImage(2).pixelColor(0,0),QColor(4,80,170));
 }
 void spriteWriterPreservesRawDataAndAlpha() {
    QTemporaryDir source;fixture(source.path());
    QFile original(source.path()+"/Tibia.spr");QVERIFY(original.open(QIODevice::ReadOnly));const QByteArray originalBytes=original.readAll();original.close();
    SprReader unchanged;QVERIFY(unchanged.loadFile(original.fileName(),0,false,false));
    const QString copied=source.path()+"/copied.spr";QVERIFY(unchanged.saveFile(copied));
    QFile copy(copied);QVERIFY(copy.open(QIODevice::ReadOnly));QCOMPARE(copy.readAll(),originalBytes);copy.close();

    const QString alphaPath=source.path()+"/alpha.spr";QFile alphaFile(alphaPath);QVERIFY(alphaFile.open(QIODevice::WriteOnly));
    QDataStream out(&alphaFile);out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(0xAABBCCDD)<<quint32(1)<<quint32(12);
    out<<quint8(1)<<quint8(2)<<quint8(3)<<quint16(4100)<<quint16(0)<<quint16(1024);
    for(int i=0;i<1024;++i)out<<quint8(70)<<quint8(80)<<quint8(90)<<quint8(77);
    alphaFile.close();
    SprReader alpha;QVERIFY(alpha.loadFile(alphaPath,0,true,true));
    QCOMPARE(alpha.spriteImage(1).pixelColor(0,0),QColor(70,80,90,77));
    QImage replacement(32,32,QImage::Format_RGBA8888);replacement.fill(QColor(11,22,33,44));
    QVERIFY(alpha.replaceSprite(1,replacement));QCOMPARE(alpha.addSprite(replacement),2);
    const QString alphaOutput=source.path()+"/alpha-output.spr";QVERIFY(alpha.saveFile(alphaOutput));
    SprReader alphaVerify;QVERIFY(alphaVerify.loadFile(alphaOutput,0,true,true));QCOMPARE(alphaVerify.spriteCount(),2);
    QCOMPARE(alphaVerify.spriteImage(1).pixelColor(0,0),QColor(11,22,33,44));
    QCOMPARE(alphaVerify.spriteImage(2).pixelColor(31,31),QColor(11,22,33,44));
 }
 void rejectsTruncatedCategory(){
    QTemporaryDir dir;fixture(dir.path());QFile file(dir.path()+"/Tibia.dat");QVERIFY(file.open(QIODevice::ReadWrite));file.resize(file.size()-1);file.close();
    DatReader dat;dat.setClientVersion(860);QVERIFY(!dat.loadFile(file.fileName()));QVERIFY(!dat.isLoaded());
 }
 void inspectorPropertiesRoundTrip(){
    QTemporaryDir dir;fixture(dir.path());
    QFile otfi(dir.path()+"/Tibia.otfi");QVERIFY(otfi.open(QIODevice::WriteOnly));
    otfi.write("DatSpr\n  extended: false\n  transparency: false\n  frame-durations: false\n  frame-groups: false\n");otfi.close();
    EditorBackend backend;QVERIFY(backend.openFolder(dir.path(),1098));
    const QVariantMap values{{"forceUse",true},{"isOnTop",true},{"hasCloth",true},{"clothSlot",7},
      {"hasMarket",true},{"marketCategory",9},{"marketTradeAs",140},{"marketShowAs",141},
      {"marketName","parcel"},{"marketVocation",65535},{"marketLevel",20},
      {"hasAction",true},{"defaultAction",2},{"noMoveAnimation",true},{"wrappable",true},
      {"unwrappable",true},{"lensHelp",123},{"isWritable",true},{"writableOnce",true},{"maxTextLength",512},
      {"itemWidth",2},{"cropSize",48}};
    QVERIFY(backend.setValues(values));
    backend.undo();QVERIFY(!backend.details().value("hasMarket").toBool());QCOMPARE(backend.details().value("itemWidth").toInt(),1);
    backend.redo();QCOMPARE(backend.details().value("marketName").toString(),QString("parcel"));
    const auto before=backend.details();
    QVERIFY(!backend.setValues({{"groundSpeed",999},{"unknownField",1}}));QCOMPARE(backend.details(),before);
    QTemporaryDir output;QVERIFY(backend.compileAs(output.path()));
    EditorBackend verify;QVERIFY(verify.openFolder(output.path(),1098));
    for(auto it=values.cbegin();it!=values.cend();++it)QCOMPARE(verify.details().value(it.key()),it.value());
    QCOMPARE(verify.details().value("spriteIds").toList().first().toInt(),1);
    QVERIFY(verify.setValues({{"hasMarket",false},{"forceUse",false},{"hasCloth",false},{"hasAction",false},{"wrappable",false},{"unwrappable",false},{"noMoveAnimation",false}}));
    QTemporaryDir clearedOutput;QVERIFY(verify.compileAs(clearedOutput.path()));EditorBackend cleared;QVERIFY(cleared.openFolder(clearedOutput.path(),1098));
    QVERIFY(!cleared.details().value("hasMarket").toBool());QVERIFY(!cleared.details().value("forceUse").toBool());
 }
 void preservesOpaqueFlagsAndAnimation(){
    QTemporaryDir dir;QFile file(dir.path()+"/input.dat");QVERIFY(file.open(QIODevice::WriteOnly));QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
    out<<quint32(42)<<quint16(100)<<quint16(0)<<quint16(0)<<quint16(0);
    out<<quint8(6)<<quint8(255);for(int j=0;j<6;++j)out<<quint8(1);out<<quint8(2);
    out<<quint8(1)<<quint32(3)<<quint8(1)<<quint32(120)<<quint32(170)<<quint32(190)<<quint32(240)<<quint32(1)<<quint32(2);file.close();
    DatReader dat;dat.setClientVersion(1098);QVERIFY(dat.loadFile(file.fileName()));
    const auto original=*dat.objectAt(0,0);QVERIFY(!original.animation_data.isEmpty());QVERIFY(!original.extra_flags.isEmpty());
    QVERIFY(dat.setValue(0,"isStackable",true));QVERIFY(dat.saveFile(dir.path()+"/output.dat"));
    DatReader verify;verify.setClientVersion(1098);QVERIFY(verify.loadFile(dir.path()+"/output.dat"));
    QCOMPARE(verify.objectAt(0,0)->animation_data,original.animation_data);QCOMPARE(verify.objectAt(0,0)->extra_flags,original.extra_flags);
 }
};
QTEST_MAIN(EditorTests)
#include "editor_tests.moc"
