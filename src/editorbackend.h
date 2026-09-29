#pragma once
#include "projectmodel.h"
#include <QAbstractListModel>
#include <QQuickImageProvider>
#include <QSet>
#include <QColor>
#include <memory>
#include <optional>

class EditorBackend final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int selected READ selected WRITE select NOTIFY changed)
    Q_PROPERTY(QVariantList selectedRows READ selectedRows NOTIFY selectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(int category READ category WRITE setCategory NOTIFY changed)
    Q_PROPERTY(int spriteCount READ spriteCount NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
    Q_PROPERTY(QVariantMap info READ info NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool textureEditPending READ textureEditPending NOTIFY changed)
    Q_PROPERTY(QVariantMap objectClipboard READ objectClipboard NOTIFY changed)
    Q_PROPERTY(int serverId READ serverId NOTIFY changed)
    Q_PROPERTY(QVariantMap serverAttributes READ serverAttributes NOTIFY changed)
    Q_PROPERTY(bool hasCopiedServerAttributes READ hasCopiedServerAttributes NOTIFY changed)
    Q_PROPERTY(bool compiling READ compiling NOTIFY compileProgressChanged)
    Q_PROPERTY(int compileProgress READ compileProgress NOTIFY compileProgressChanged)
    Q_PROPERTY(QString compileStage READ compileStage NOTIFY compileProgressChanged)
    Q_PROPERTY(int slicerWidth READ slicerWidth NOTIFY slicerChanged)
    Q_PROPERTY(int slicerHeight READ slicerHeight NOTIFY slicerChanged)
    Q_PROPERTY(int slicerCount READ slicerCount NOTIFY slicerChanged)
    Q_PROPERTY(int slicerRevision READ slicerRevision NOTIFY slicerChanged)
    Q_PROPERTY(bool pixelEditActive READ pixelEditActive NOTIFY pixelEditChanged)
    Q_PROPERTY(int pixelEditRevision READ pixelEditRevision NOTIFY pixelEditChanged)
    Q_PROPERTY(int pixelEditSpriteId READ pixelEditSpriteId NOTIFY pixelEditChanged)
    Q_PROPERTY(int pixelEditSize READ pixelEditSize NOTIFY pixelEditChanged)
public:
    enum Roles { ObjectId = Qt::UserRole+1, SourceRow, ImageSource, Description };
    explicit EditorBackend(QObject *parent=nullptr);
    int rowCount(const QModelIndex &parent={}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    bool loaded() const { return m_project.loaded(); }
    bool dirty() const { return m_project.dirty(); }
    int count() const { return m_rows.size(); }
    int selected() const { return m_selected; }
    QVariantList selectedRows() const;
    int selectedCount() const { return m_selectedRows.size(); }
    int selectionRevision() const { return m_selectionRevision; }
    int category() const { return m_category; }
    int spriteCount() const { return m_project.sprites() ? m_project.sprites()->spriteCount() : 0; }
    int revision() const { return m_revision; }
    QString status() const { return m_status; }
    QString log() const { return m_log; }
    QVariantMap details() const;
    QVariantMap info() const;
    bool canUndo() const { return textureEditPending() || !m_undo.empty(); }
    bool canRedo() const { return m_textureEdits.isEmpty() && !m_redo.empty(); }
    bool textureEditPending() const;
    QVariantMap objectClipboard() const;
    int serverId() const;
    QVariantMap serverAttributes() const;
    bool hasCopiedServerAttributes() const { return !m_serverAttributesCopy.isEmpty(); }
    Q_INVOKABLE bool setServerAttributes(const QVariantMap &values);
    Q_INVOKABLE bool copyServerAttributes();
    Q_INVOKABLE bool pasteServerAttributes();
    Q_INVOKABLE bool createServerItem();
    Q_INVOKABLE bool createOtbFile();
    Q_INVOKABLE bool reloadSelectedOtbItem();
    Q_INVOKABLE bool updateOtbVersion(int major, int minor, int build);
    Q_INVOKABLE QVariantMap compareOtbFile(const QString &fileUrl) const;
    bool compiling() const { return m_compiling; }
    int compileProgress() const { return m_compileProgress; }
    QString compileStage() const { return m_compileStage; }
    int slicerWidth() const { return m_slicerImage.width(); }
    int slicerHeight() const { return m_slicerImage.height(); }
    int slicerCount() const { return m_slicerTiles.size(); }
    int slicerRevision() const { return m_slicerRevision; }
    bool pixelEditActive() const { return !m_pixelEditImage.isNull(); }
    int pixelEditRevision() const { return m_pixelEditRevision; }
    int pixelEditSpriteId() const { return m_pixelEditSpriteId; }
    int pixelEditSize() const { return m_pixelEditImage.width(); }
    Q_INVOKABLE bool copyObjectPart(const QString &part);
    Q_INVOKABLE bool pasteObjectPart(const QString &part);
    Q_INVOKABLE bool replaceObject(int clientId);
    Q_INVOKABLE int bulkReplaceObjects(int sourceId);
    Q_INVOKABLE int bulkSetItemAttribute(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariantMap compareSelectedObjects() const;
    Q_INVOKABLE bool importItemGraphics(const QString &folderUrl, int sourceVersion, int sourceId);
    Q_INVOKABLE int importItemGraphicsRange(const QString &folderUrl, int sourceVersion,
                                           int sourceFirstId, int sourceLastId, int targetFirstId);
    Q_INVOKABLE void copyId(bool server=false);
    Q_INVOKABLE bool openFolder(const QString &url, int version, bool alpha=false,
                               const QString &serverFolder=QString(), int spriteSizeOverride=0);
    Q_INVOKABLE QString localPath(const QString &url) const;
    Q_INVOKABLE bool openNewWindow() const;
    Q_INVOKABLE int detectFolderVersion(const QString &url) const;
    Q_INVOKABLE QVariantMap inspectFolder(const QString &url, int version, bool alpha=false,
                                          const QString &serverFolder=QString(), int spriteSizeOverride=0);
    Q_INVOKABLE bool createAssetFiles(const QString &folderUrl, int version, bool extended,
                                     bool transparency, bool durations, bool groups, int spriteSize=32);
    Q_INVOKABLE void select(int row);
    Q_INVOKABLE void selectWithModifiers(int row, int modifiers);
    Q_INVOKABLE void activateSelected(int row);
    Q_INVOKABLE void setCategory(int category);
    Q_INVOKABLE void filter(const QString &query, bool hideEmpty);
    Q_INVOKABLE void jump(int id);
    Q_INVOKABLE int visibleIndex() const { return m_visiblePositions.value(m_selected,-1); }
    Q_INVOKABLE bool isSelected(int row) const { return m_selectedRows.contains(row); }
    Q_INVOKABLE void step(int offset);
    Q_INVOKABLE QString preview(int row, int frame=0, int pattern=0, int group=0, int layer=-1) const;
    Q_INVOKABLE QString previewObject(int category, int id, int frame=0, int pattern=0, int group=0, int layer=-1) const;
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE int sliceImage(const QString &sourceUrl, const QString &folderUrl, int tileSize);
    Q_INVOKABLE int importSheetSprites(const QString &sourceUrl, bool skipEmpty=true);
    Q_INVOKABLE bool slicerOpen(const QString &sourceUrl);
    Q_INVOKABLE bool slicerTransform(const QString &operation);
    Q_INVOKABLE int slicerCut(int offsetX, int offsetY, int columns, int rows, int tileSize, bool includeEmpty);
    Q_INVOKABLE void slicerClear();
    Q_INVOKABLE int slicerImport();
    Q_INVOKABLE QVariantMap frameDuration(int category, int id, int group, int frame) const;
    Q_INVOKABLE bool setFrameDuration(int category, int id, int group, int frame, int minimum, int maximum);
    Q_INVOKABLE bool setAnimationSettings(int category, int id, int group, int mode, int loopCount, int startFrame);
    Q_INVOKABLE bool duplicateFrame(int category, int id, int group, int frame);
    Q_INVOKABLE bool deleteFrame(int category, int id, int group, int frame);
    Q_INVOKABLE int optimizeFrameDurations(bool items, bool outfits, bool effects, int minimum, int maximum);
    Q_INVOKABLE bool convertFrameDurations(bool enabled, int minimum=100, int maximum=100);
    Q_INVOKABLE bool convertFrameGroups(bool enabled, bool removeMounts=false);
    Q_INVOKABLE QVariantMap optimizeSprites(bool compactIds=false);
    Q_INVOKABLE QString spriteSource(int id) const;
    Q_INVOKABLE bool setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE bool setValues(const QVariantMap &values);
    Q_INVOKABLE bool setTextureValue(int group, const QString &key, int value);
    Q_INVOKABLE bool setOutfitBones(bool enabled, int direction, int x, int y);
    Q_INVOKABLE bool beginPixelEdit(int group, int frame, int pattern, int layer, int x, int y);
    Q_INVOKABLE bool paintPixel(int x, int y, const QColor &color);
    Q_INVOKABLE void resetPixelEdit();
    Q_INVOKABLE bool savePixelEdit();
    Q_INVOKABLE void cancelPixelEdit();
    Q_INVOKABLE bool assignSprite(int slot, int id);
    Q_INVOKABLE bool assignSpriteToCell(int group, int frame, int pattern, int layer,
                                       int tileX, int tileY, int spriteId);
    Q_INVOKABLE bool importObjectImage(const QString &fileUrl, int group, int frame,
                                      int pattern, int layer, int tileX, int tileY);
    Q_INVOKABLE bool saveTextureEdit();
    Q_INVOKABLE bool resetTextureEdit();
    Q_INVOKABLE void create(bool duplicate=false);
    Q_INVOKABLE void clearObject();
    Q_INVOKABLE bool removeObject();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool compile();
    Q_INVOKABLE bool compileAs(const QString &folderUrl);
    Q_INVOKABLE bool save(const QString &url=QString());
    Q_INVOKABLE int createMissingOtbItems();
    Q_INVOKABLE int reloadItemAttributes();
    Q_INVOKABLE bool replaceSprite(int spriteId, const QString &fileUrl);
    Q_INVOKABLE int addSprite(const QString &fileUrl);
    Q_INVOKABLE bool removeSprite(int spriteId);
    Q_INVOKABLE bool exportPng(const QString &url, bool sheet=false);
    Q_INVOKABLE int exportSelectedPngs(const QString &folderUrl, bool sheet=false);
    Q_INVOKABLE int exportAllObjects(const QString &folderUrl, bool sheet=false);
    Q_INVOKABLE int exportAllSprites(const QString &folderUrl);
    Q_INVOKABLE int exportObjects(const QString &folderUrl, const QString &name,
                                 const QString &format, bool transparentBackground);
    Q_INVOKABLE QString defaultExportFolder() const;
    Q_INVOKABLE QVariantMap preferences() const;
    Q_INVOKABLE bool setPreferences(const QVariantMap &values);
    Q_INVOKABLE bool exportSprite(const QString &url, int id);
    Q_INVOKABLE void clearLog();
    QImage image(const QString &id);
    void message(const QString &text);
private:
    struct Edit { int category; int row; ClientItem before, after; };
    static QString path(const QString &url);
    void refresh();
    void rebuild();
    void notifySelection();
    void remember(const ClientItem &before);
    void stageTextureEdit(const ClientItem &before);
    void commitTextureEdits();
    quint64 textureEditKey() const;
    bool applyObject(ClientItem item);
    bool exportPngAt(const QString &filePath, bool sheet, int row);
    QImage renderObjectAt(bool sheet, int row, int category=-1);
    QString previewForCategory(int category, int row, int frame, int pattern, int group, int layer) const;
    std::optional<ClientItem> m_objectCopy, m_patternsCopy, m_propertiesCopy;
    QVariantMap m_serverAttributesCopy;
    ProjectModel m_project;
    QVector<int> m_rows;
    QHash<int,int> m_visiblePositions;
    std::vector<Edit> m_undo,m_redo;
    QHash<quint64, ClientItem> m_textureEdits;
    QHash<quint64, QSet<quint32>> m_textureImportedSprites;
    int m_selected=-1, m_category=0, m_revision=0, m_version=1098;
    QSet<int> m_selectedRows;
    int m_selectionAnchor=-1;
    int m_selectionRevision=0;
    bool m_hideEmpty=false, m_alpha=false, m_extended=false, m_durations=false, m_groups=false;
    QString m_query, m_folder, m_status="Open a client folder to begin", m_log;
    bool m_compiling=false;
    int m_compileProgress=0;
    QString m_compileStage;
    QImage m_slicerImage;
    QVector<QImage> m_slicerTiles;
    int m_slicerRevision=0;
    QImage m_pixelEditImage;
    QImage m_pixelEditOriginal;
    int m_pixelEditSpriteId=0;
    int m_pixelEditSlot=-1;
    int m_pixelEditCategory=-1;
    int m_pixelEditRow=-1;
    int m_pixelEditGroup=0;
    int m_pixelEditRevision=0;
    QString m_slicerLastCut;
signals:
    void changed();
    void selectionChanged();
    void logChanged();
    void compileProgressChanged();
    void slicerChanged();
    void pixelEditChanged();
};
class EditorImageProvider final : public QQuickImageProvider {
public:
    explicit EditorImageProvider(EditorBackend *backend):QQuickImageProvider(Image),m_backend(backend){}
    QImage requestImage(const QString &id,QSize *size,const QSize &) override {
        QImage result=m_backend->image(id); if(size)*size=result.size(); return result;
    }
private: EditorBackend *m_backend;
};
