#ifndef DATREADER_H
#define DATREADER_H

#include "binaryreader.h"
#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QByteArray>
#include <QtQml/qqmlregistration.h>
#include <cstdint>
#include <array>
#include <vector>

struct ClientFrameGroup {
    uint8_t type = 0;
    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;
    uint8_t exact_size = 32;
    std::vector<uint32_t> sprite_ids;
    QByteArray animation_data;

    uint64_t getTotalSprites() const {
        return static_cast<uint64_t>(width) * height * layers
             * pattern_x * pattern_y * pattern_z * frames;
    }
};

struct ClientItem {
    uint16_t id = 0;

    uint8_t width = 1;
    uint8_t height = 1;
    uint8_t layers = 1;
    uint8_t pattern_x = 1;
    uint8_t pattern_y = 1;
    uint8_t pattern_z = 1;
    uint8_t frames = 1;

    std::vector<uint32_t> sprite_ids;

    bool is_ground = false;
    uint16_t ground_speed = 0;
    bool is_ground_border = false;
    bool is_on_bottom = false;
    bool is_on_top = false;
    bool is_container = false;
    bool is_stackable = false;
    bool is_useable = false;
    bool is_writable = false;
    uint16_t max_text_length = 0;
    bool is_fluid_container = false;
    bool is_fluid = false;
    bool is_unpassable = false;
    bool is_unmoveable = false;
    bool blocks_missiles = false;
    bool blocks_pathfinder = false;
    bool is_pickupable = false;
    bool is_hangable = false;
    bool is_horizontal = false;
    bool is_vertical = false;
    bool is_rotatable = false;
    bool has_light = false;
    uint16_t light_level = 0;
    uint16_t light_color = 0;
    bool dont_hide = false;
    bool is_translucent = false;
    bool has_offset = false;
    bool has_bones = false;
    std::array<int16_t,4> bone_offset_x{};
    std::array<int16_t,4> bone_offset_y{};
    int bone_flag_offset = -1;
    int16_t offset_x = 0;
    int16_t offset_y = 0;
    bool has_elevation = false;
    uint16_t elevation = 0;
    bool is_lying_object = false;
    bool animate_always = false;
    bool has_minimap_color = false;
    uint16_t minimap_color = 0;
    bool full_ground = false;
    bool ignore_look = false;
    uint16_t lens_help = 0;
    bool floor_change = false;
    bool modified = false;
    bool preserve_raw_flags = false;
    QByteArray raw_record;
    QByteArray raw_flags;
    QByteArray extra_flags;
    QVariantMap extra_properties;
    QByteArray animation_data;
    std::vector<ClientFrameGroup> frame_groups;
    uint8_t exact_size = 32;
    bool writable_once = false;

    uint32_t previewSpriteId() const {
        return sprite_ids.empty() ? 0 : sprite_ids.front();
    }

    uint64_t getTotalSprites() const {
        return static_cast<uint64_t>(width) * height * layers
             * pattern_x * pattern_y * pattern_z * frames;
    }
};

class DatReader : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY loadedChanged)

public:
    enum ItemRoles {
        ItemIdRole = Qt::UserRole + 1,
        PreviewSpriteIdRole,
        SpriteIdsRole,
        ItemWidthRole,
        ItemHeightRole,
        LayersRole,
        IsGroundRole,
        IsStackableRole,
        IsContainerRole,
        IsUnpassableRole
    };

    explicit DatReader(QObject *parent = nullptr);
    ~DatReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int itemCount() const { return static_cast<int>(m_items.size()); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }
    bool isDirty() const { return m_dirty; }
    QString filePath() const { return m_filePath; }

    Q_PROPERTY(int clientVersion READ clientVersion WRITE setClientVersion NOTIFY clientVersionChanged)

    int clientVersion() const { return m_clientVersion; }
    Q_INVOKABLE void setClientVersion(int v);

    Q_INVOKABLE void setOtfiOverrides(bool has, bool extended, bool frameDurations, bool frameGroups) {
        m_otfiActive = has;
        m_otfiExtended = extended;
        m_otfiFrameDurations = frameDurations;
        m_otfiFrameGroups = frameGroups;
    }

    Q_INVOKABLE bool loadFile(const QString &path, quint32 expectedSignature = 0);
    Q_INVOKABLE bool saveFile(const QString &path = QString());

    Q_INVOKABLE quint32 previewSpriteIdAt(int row) const;
    Q_INVOKABLE int itemIdAt(int row) const;
    Q_INVOKABLE QVariantMap detailsAt(int row) const;
    Q_INVOKABLE bool setValue(int row, const QString &key, const QVariant &value);
    bool setValues(int row, const QVariantMap &values);
    Q_INVOKABLE bool setSpriteId(int row, int slot, quint32 spriteId);
    Q_INVOKABLE int createItem();
    bool ensureItemId(int clientId);
    void adaptImportedItem(ClientItem &item) const;
    Q_INVOKABLE int duplicateItem(int row);
    Q_INVOKABLE bool removeItem(int row);
    int createObject(int category, int sourceRow = -1);
    bool removeObject(int category, int row);

    const ClientItem *itemByClientId(uint16_t clientId) const;

    const ClientItem *outfitByLookType(uint16_t lookType) const;

    Q_INVOKABLE QVariantMap outfitPreview(int lookType) const;

    Q_INVOKABLE QVariantMap itemPreview(int clientId) const;

    const ClientItem *effectById(int id) const;
    const ClientItem *objectAt(int category, int row) const;
    int categoryCount(int category) const;
    quint32 signature() const { return m_signature; }
    void restoreItem(int row, const ClientItem &item);
    void restoreObject(int category, int row, const ClientItem &item);
    bool replaceItems(int firstRow, const std::vector<ClientItem> &items);
    bool setFrameDuration(int category, int row, int group, int frame, quint32 minimum, quint32 maximum);
    bool duplicateFrame(int category, int row, int group, int frame);
    bool setAnimationSettings(int category, int row, int group, int mode, int loopCount, int startFrame);
    bool setOutfitBones(int row, bool enabled, int direction, int x, int y);
    bool deleteFrame(int category, int row, int group, int frame);
    int optimizeFrameDurations(bool items, bool outfits, bool effects, quint32 minimum, quint32 maximum);
    int convertFrameDurations(bool enabled, quint32 minimum, quint32 maximum);
    int convertFrameGroups(bool enabled, bool removeMounts=false);
    QSet<quint32> usedSpriteIds() const;
    int remapSpriteIds(const QHash<quint32, quint32> &mapping);


signals:
    void itemCountChanged();
    void loadedChanged();
    void errorChanged();
    void clientVersionChanged();
    void dirtyChanged();

private:

    void readCategory(BinaryReader &reader,
                      std::vector<ClientItem> *outItems,
                      uint16_t minId, uint16_t maxId, bool outfits = false);

    void readItemFlags(ClientItem &item, BinaryReader &reader);
    void readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits);

    uint8_t transformFlag(uint8_t raw) const;
    uint8_t encodeFlag(uint8_t canonical) const;

    bool extendedSprites() const { return m_otfiActive ? m_otfiExtended : m_clientVersion >= 960; }
    bool frameDurations() const  { return m_otfiActive ? m_otfiFrameDurations : m_clientVersion >= 1050; }
    bool frameGroups() const     { return m_otfiActive ? m_otfiFrameGroups : m_clientVersion >= 1057; }

    void setError(const QString &message);
    void reset();

    std::vector<ClientItem> m_items;
    std::vector<ClientItem> m_missiles;
    std::vector<ClientItem> m_effects;
    std::vector<ClientItem> m_outfits;
    int m_clientVersion = 772;
    bool m_otfiActive = false;
    bool m_otfiExtended = false;
    bool m_otfiFrameDurations = false;
    bool m_otfiFrameGroups = false;
    uint32_t m_signature = 0;
    uint16_t m_maxItemId = 0;
    bool m_loaded = false;
    bool m_dirty = false;
    QString m_errorString;
    QString m_filePath;
    QByteArray m_categoryTail;
    uint16_t m_maxOutfitId = 0;
    uint16_t m_maxEffectId = 0;
    uint16_t m_maxMissileId = 0;
    bool m_categoryStructureChanged = false;
};

#endif
