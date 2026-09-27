#ifndef OTBREADER_H
#define OTBREADER_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QPair>
#include <QtQml/qqmlregistration.h>
#include <cstdint>
#include <vector>
#include <functional>

class DatReader;
class ItemsXmlReader;

enum class OtbItemGroup : uint8_t {
    None = 0,
    Ground = 1,
    Container = 2,
    Weapon = 3,
    Ammunition = 4,
    Armor = 5,
    Changes = 6,
    Teleport = 7,
    MagicField = 8,
    Writeable = 9,
    Key = 10,
    Splash = 11,
    Fluid = 12,
    Door = 13,
    Deprecated = 14,
    Podium = 15
};

struct OtbItem {
    uint16_t server_id = 0;
    uint16_t client_id = 0;
    uint8_t group = 0;
    uint32_t flags = 0;
    QString name;
    QString description;
    uint16_t speed = 0;
    uint16_t max_read_write_length = 0;
    uint16_t max_read_length = 0;
    uint16_t minimap_color = 0;
    uint16_t ware_id = 0;
    uint8_t light_level = 0;
    uint8_t light_color = 0;
    int8_t stack_order = 0;

    bool is_unpassable = false;
    bool blocks_missiles = false;
    bool blocks_pathfinder = false;
    bool has_elevation = false;
    bool is_useable = false;
    bool is_pickupable = false;
    bool is_moveable = true;
    bool is_stackable = false;
    bool always_on_top = false;
    bool is_readable = false;
    bool is_rotatable = false;
    bool is_hangable = false;
    bool hook_east = false;
    bool hook_south = false;
    bool ignore_look = false;
    bool full_ground = false;
    bool allow_dist_read = false;
    bool client_duration = false;
    bool client_charges = false;
    bool animation = false;
    bool force_use = false;
    QVector<QPair<uint8_t, QByteArray>> raw_attributes;
};

class OtbReader : public QAbstractListModel
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(quint32 majorVersion READ majorVersion NOTIFY loadedChanged)
    Q_PROPERTY(quint32 minorVersion READ minorVersion NOTIFY loadedChanged)
    Q_PROPERTY(quint32 buildNumber READ buildNumber NOTIFY loadedChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY loadedChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)

public:
    enum ItemRoles {
        ItemIdRole = Qt::UserRole + 1,
        ServerIdRole,
        ClientIdRole,
        NameRole,
        GroupRole,
        SpriteIdsRole,
        ItemWidthRole,
        ItemHeightRole,
        LayersRole,
        IsRenderableRole,
        IsGroundRole,
        IsStackableRole,
        IsContainerRole,
        IsUnpassableRole
    };

    explicit OtbReader(QObject *parent = nullptr);
    ~OtbReader() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int itemCount() const { return static_cast<int>(m_items.size()); }
    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_errorString; }
    quint32 majorVersion() const { return m_majorVersion; }
    quint32 minorVersion() const { return m_minorVersion; }
    quint32 buildNumber() const { return m_buildNumber; }
    QString filePath() const { return m_filePath; }
    bool dirty() const { return m_dirty; }

    void setDatReader(DatReader *datReader);

    void setItemsXml(ItemsXmlReader *itemsXml);

    Q_INVOKABLE bool loadFile(const QString &path);
    Q_INVOKABLE int clientIdForServerId(int serverId) const;
    Q_INVOKABLE int rowForClientId(int clientId) const;
    Q_INVOKABLE QString nameForServerId(int serverId) const;

    Q_INVOKABLE int rowForServerId(int serverId) const;

    int topOrderForServerId(int serverId) const;

    Q_INVOKABLE int groupForServerId(int serverId) const;

    Q_INVOKABLE QString groupNameForServerId(int serverId) const;

    Q_INVOKABLE bool isTeleportItem(int serverId) const;
    Q_INVOKABLE QVariantMap detailsAt(int row) const;
    Q_INVOKABLE bool setValue(int row, const QString &key, const QVariant &value);
    Q_INVOKABLE bool saveFile(const QString &path = QString());
    void syncNamesToItemsXml();
    Q_INVOKABLE int createItem(int clientId = 100);
    Q_INVOKABLE int duplicateItem(int row);
    Q_INVOKABLE bool removeItem(int row);
    Q_INVOKABLE bool reloadItem(int row);
    Q_INVOKABLE int reloadAllItems();
    Q_INVOKABLE int createMissingItems();
    int createMissingItems(const std::function<void(int, int)> &progress);
    Q_INVOKABLE void newFile(quint32 minorVersion = 1);
    Q_INVOKABLE void setOtbVersion(quint32 major, quint32 minor, quint32 build);
    Q_INVOKABLE bool isMismatched(int row) const;

signals:
    void itemCountChanged();
    void loadedChanged();
    void errorChanged();
    void dirtyChanged();

private:
    static uint8_t mapGroup(uint8_t group);

    void reset();
    void setError(const QString &message);
    void refreshDatRoles();

    std::vector<OtbItem> m_items;
    QHash<uint16_t, int> m_serverIdToRow;
    QHash<uint16_t, int> m_clientIdToRow;
    DatReader *m_datReader = nullptr;
    ItemsXmlReader *m_itemsXml = nullptr;

    uint32_t m_majorVersion = 0;
    uint32_t m_minorVersion = 0;
    uint32_t m_buildNumber = 0;
    bool m_loaded = false;
    QString m_errorString;
    QString m_filePath;
    QByteArray m_rootData;
    bool m_dirty = false;
};

#endif
