#include "otbreader.h"

#include "datreader.h"
#include "itemsxmlreader.h"
#include "nodefilereader.h"
#include <QSaveFile>
#include <QSignalBlocker>

namespace {

enum class OtbAttribute : uint8_t {
    ServerId = 0x10,
    ClientId = 0x11,
    Name = 0x12,
    Description = 0x13,
    Speed = 0x14,
    SpriteHash = 0x20,
    MinimapColor = 0x21,
    MaxReadWriteChars = 0x22,
    MaxReadChars = 0x23,
    Light = 0x2A,
    StackOrder = 0x2B,
    TradeAs = 0x2D
};

enum class RootAttribute : uint8_t {
    Version = 0x01
};

enum ItemFlag : uint32_t {
    Unpassable = 1u << 0,
    BlockMissiles = 1u << 1,
    BlockPathfinder = 1u << 2,
    HasElevation = 1u << 3,
    Useable = 1u << 4,
    Pickupable = 1u << 5,
    Moveable = 1u << 6,
    Stackable = 1u << 7,
    AlwaysOnTop = 1u << 13,
    Readable = 1u << 14,
    Rotatable = 1u << 15,
    Hangable = 1u << 16,
    HookSouth = 1u << 17,
    HookEast = 1u << 18,
    AllowDistRead = 1u << 20,
    ClientDuration = 1u << 21,
    ClientCharges = 1u << 22,
    LookThrough = 1u << 23,
    Animation = 1u << 24,
    FullTile = 1u << 25,
    ForceUse = 1u << 26
};

uint16_t readPayloadU16(const QByteArray &payload, int offset = 0)
{
    if (offset < 0 || offset + 2 > payload.size()) {
        return 0;
    }

    const auto *raw = reinterpret_cast<const uchar *>(payload.constData());
    return static_cast<uint16_t>(raw[offset])
         | (static_cast<uint16_t>(raw[offset + 1]) << 8);
}

uint32_t readPayloadU32(const QByteArray &payload, int offset = 0)
{
    if (offset < 0 || offset + 4 > payload.size()) {
        return 0;
    }

    const auto *raw = reinterpret_cast<const uchar *>(payload.constData());
    return static_cast<uint32_t>(raw[offset])
         | (static_cast<uint32_t>(raw[offset + 1]) << 8)
         | (static_cast<uint32_t>(raw[offset + 2]) << 16)
         | (static_cast<uint32_t>(raw[offset + 3]) << 24);
}

QString readPayloadString(const QByteArray &payload)
{
    if (payload.isEmpty()) {
        return QString();
    }

    if (payload.size() >= 2) {
        const uint16_t length = readPayloadU16(payload);
        if (length <= payload.size() - 2) {
            return QString::fromLatin1(payload.constData() + 2, length);
        }
    }

    return QString::fromLatin1(payload);
}

QVariantList toVariantList(const std::vector<uint32_t> &values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (uint32_t value : values) {
        result.append(QVariant::fromValue(static_cast<quint32>(value)));
    }
    return result;
}

bool hasFlag(uint32_t flags, ItemFlag flag)
{
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void appendU16(QByteArray &out, uint16_t value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray &out, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        out.append(static_cast<char>((value >> (i * 8)) & 0xff));
}

void appendEscaped(QByteArray &out, const QByteArray &data)
{
    for (const char ch : data) {
        const uint8_t byte = static_cast<uint8_t>(ch);
        if (byte == 0xfd || byte == 0xfe || byte == 0xff)
            out.append(static_cast<char>(0xfd));
        out.append(ch);
    }
}

QByteArray u16Payload(uint16_t value)
{
    QByteArray out;
    appendU16(out, value);
    return out;
}

QByteArray stringPayload(const QString &value)
{
    const QByteArray bytes = value.toLatin1();
    QByteArray out;
    appendU16(out, static_cast<uint16_t>(std::min<qsizetype>(bytes.size(), 65535)));
    out.append(bytes.left(65535));
    return out;
}

void replaceAttribute(OtbItem &item, uint8_t type, const QByteArray &payload, bool removeEmpty = false)
{
    for (qsizetype i = 0; i < item.raw_attributes.size(); ++i) {
        if (item.raw_attributes[i].first == type) {
            if (removeEmpty && payload.isEmpty())
                item.raw_attributes.removeAt(i);
            else
                item.raw_attributes[i].second = payload;
            return;
        }
    }
    if (!removeEmpty || !payload.isEmpty())
        item.raw_attributes.append(qMakePair(type, payload));
}

void addRow(QVariantList &rows, const QString &name, const QVariant &value)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("value"), value);
    rows.append(row);
}

QString groupName(uint8_t group)
{
    switch (static_cast<OtbItemGroup>(group)) {
    case OtbItemGroup::Ground: return QStringLiteral("Ground");
    case OtbItemGroup::Container: return QStringLiteral("Container");
    case OtbItemGroup::Weapon: return QStringLiteral("Weapon");
    case OtbItemGroup::Ammunition: return QStringLiteral("Ammunition");
    case OtbItemGroup::Armor: return QStringLiteral("Armor");
    case OtbItemGroup::Changes: return QStringLiteral("Changes");
    case OtbItemGroup::Teleport: return QStringLiteral("Teleport");
    case OtbItemGroup::MagicField: return QStringLiteral("Magic field");
    case OtbItemGroup::Writeable: return QStringLiteral("Writeable");
    case OtbItemGroup::Key: return QStringLiteral("Key");
    case OtbItemGroup::Splash: return QStringLiteral("Splash");
    case OtbItemGroup::Fluid: return QStringLiteral("Fluid");
    case OtbItemGroup::Door: return QStringLiteral("Door");
    case OtbItemGroup::Deprecated: return QStringLiteral("Deprecated");
    case OtbItemGroup::Podium: return QStringLiteral("Podium");
    case OtbItemGroup::None:
    default:
        return QStringLiteral("None");
    }
}

QStringList collectOtbFlags(const OtbItem &item)
{
    QStringList flags;
    if (item.is_unpassable) flags << QStringLiteral("Unpassable");
    if (item.blocks_missiles) flags << QStringLiteral("Blocks missiles");
    if (item.blocks_pathfinder) flags << QStringLiteral("Blocks pathfinder");
    if (item.has_elevation) flags << QStringLiteral("Elevation");
    if (item.is_useable) flags << QStringLiteral("Useable");
    if (item.is_pickupable) flags << QStringLiteral("Pickupable");
    if (item.is_moveable) flags << QStringLiteral("Moveable");
    if (item.is_stackable) flags << QStringLiteral("Stackable");
    if (item.always_on_top) flags << QStringLiteral("Always on top");
    if (item.is_readable) flags << QStringLiteral("Readable");
    if (item.is_rotatable) flags << QStringLiteral("Rotatable");
    if (item.is_hangable) flags << QStringLiteral("Hangable");
    if (item.hook_east) flags << QStringLiteral("Hook east");
    if (item.hook_south) flags << QStringLiteral("Hook south");
    if (item.allow_dist_read) flags << QStringLiteral("Allow distance read");
    if (item.client_duration) flags << QStringLiteral("Client duration");
    if (item.client_charges) flags << QStringLiteral("Client charges");
    if (item.ignore_look) flags << QStringLiteral("Look through");
    if (item.animation) flags << QStringLiteral("Animation");
    if (item.full_ground) flags << QStringLiteral("Full tile");
    if (item.force_use) flags << QStringLiteral("Force use");
    return flags;
}

}

OtbReader::OtbReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

OtbReader::~OtbReader() = default;

uint8_t OtbReader::mapGroup(uint8_t group)
{
    return group <= static_cast<uint8_t>(OtbItemGroup::Podium)
               ? group
               : static_cast<uint8_t>(OtbItemGroup::None);
}

void OtbReader::setDatReader(DatReader *datReader)
{
    if (m_datReader == datReader) {
        return;
    }

    if (m_datReader) {
        disconnect(m_datReader, nullptr, this, nullptr);
    }

    m_datReader = datReader;

    if (m_datReader) {
        connect(m_datReader, &DatReader::loadedChanged, this, &OtbReader::refreshDatRoles);
        connect(m_datReader, &DatReader::itemCountChanged, this, &OtbReader::refreshDatRoles);
    }
}

void OtbReader::setItemsXml(ItemsXmlReader *itemsXml)
{
    if (m_itemsXml == itemsXml) {
        return;
    }

    if (m_itemsXml) {
        disconnect(m_itemsXml, nullptr, this, nullptr);
    }

    m_itemsXml = itemsXml;

    if (m_itemsXml) {

        connect(m_itemsXml, &ItemsXmlReader::loadedChanged, this, [this] {
            if (m_items.empty()) return;
            emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1),
                             { NameRole });
        });
    }
}

void OtbReader::reset()
{
    beginResetModel();
    m_items.clear();
    m_serverIdToRow.clear();
    m_clientIdToRow.clear();
    m_majorVersion = 0;
    m_minorVersion = 0;
    m_buildNumber = 0;
    m_loaded = false;
    m_errorString.clear();
    m_filePath.clear();
    m_rootData.clear();
    m_dirty = false;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    emit errorChanged();
    emit dirtyChanged();
}

void OtbReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool OtbReader::loadFile(const QString &path)
{
    reset();

    NodeFileReader file;
    if (!file.loadFile(path, {QByteArrayLiteral("OTBI"), QByteArray(4, '\0')})) {
        setError(file.errorString());
        return false;
    }

    BinaryNode &root = file.rootNode();
    m_rootData = root.rawData();

    root.skip(1);
    uint32_t unusedFlags = 0;
    root.getU32(unusedFlags);

    uint8_t rootAttr = 0;
    if (root.getU8(rootAttr) && rootAttr == static_cast<uint8_t>(RootAttribute::Version)) {
        uint16_t dataLength = 0;
        if (root.getU16(dataLength) && dataLength >= 12) {
            root.getU32(m_majorVersion);
            root.getU32(m_minorVersion);
            root.getU32(m_buildNumber);
            if (dataLength > 12) {
                root.skip(dataLength - 12);
            }
        }
    }

    std::vector<OtbItem> parsedItems;
    parsedItems.reserve(static_cast<size_t>(root.children().size()));

    for (const BinaryNode &sourceNode : root.children()) {
        BinaryNode node = sourceNode;
        OtbItem item;

        uint8_t group = 0;
        uint32_t flags = 0;
        if (!node.getU8(group) || !node.getU32(flags)) {
            continue;
        }

        item.group = mapGroup(group);
        item.flags = flags;

        item.is_unpassable = hasFlag(flags, Unpassable);
        item.blocks_missiles = hasFlag(flags, BlockMissiles);
        item.blocks_pathfinder = hasFlag(flags, BlockPathfinder);
        item.has_elevation = hasFlag(flags, HasElevation);
        item.is_useable = hasFlag(flags, Useable);
        item.is_pickupable = hasFlag(flags, Pickupable);
        item.is_moveable = hasFlag(flags, Moveable);
        item.is_stackable = hasFlag(flags, Stackable);
        item.always_on_top = hasFlag(flags, AlwaysOnTop);
        item.is_readable = hasFlag(flags, Readable);
        item.is_rotatable = hasFlag(flags, Rotatable);
        item.is_hangable = hasFlag(flags, Hangable);
        item.hook_east = hasFlag(flags, HookEast);
        item.hook_south = hasFlag(flags, HookSouth);
        item.allow_dist_read = hasFlag(flags, AllowDistRead);
        item.client_duration = hasFlag(flags, ClientDuration);
        item.client_charges = hasFlag(flags, ClientCharges);
        item.ignore_look = hasFlag(flags, LookThrough);
        item.animation = hasFlag(flags, Animation);
        item.full_ground = hasFlag(flags, FullTile);
        item.force_use = hasFlag(flags, ForceUse);

        while (node.bytesRemaining() > 0) {
            uint8_t attrType = 0;
            uint16_t length = 0;
            if (!node.getU8(attrType) || !node.getU16(length)) {
                break;
            }

            QByteArray payload;
            if (!node.readBytes(length, payload)) {
                break;
            }
            item.raw_attributes.append(qMakePair(attrType, payload));

            switch (static_cast<OtbAttribute>(attrType)) {
            case OtbAttribute::ServerId:
                item.server_id = readPayloadU16(payload);
                break;
            case OtbAttribute::ClientId:
                item.client_id = readPayloadU16(payload);
                break;
            case OtbAttribute::Name:
                item.name = readPayloadString(payload);
                break;
            case OtbAttribute::Description:
                item.description = readPayloadString(payload);
                break;
            case OtbAttribute::Speed:
                item.speed = readPayloadU16(payload);
                break;
            case OtbAttribute::Light:
                item.light_level = static_cast<uint8_t>(readPayloadU16(payload, 0));
                item.light_color = static_cast<uint8_t>(readPayloadU16(payload, 2));
                break;
            case OtbAttribute::StackOrder:
                item.stack_order = payload.isEmpty() ? 0 : static_cast<int8_t>(payload.at(0));
                break;
            case OtbAttribute::MaxReadWriteChars:
                item.max_read_write_length = readPayloadU16(payload);
                break;
            case OtbAttribute::MaxReadChars:
                item.max_read_length = readPayloadU16(payload);
                break;
            case OtbAttribute::MinimapColor:
                item.minimap_color = readPayloadU16(payload);
                break;
            case OtbAttribute::TradeAs:
                item.ware_id = readPayloadU16(payload);
                break;
            case OtbAttribute::SpriteHash:
            default:
                break;
            }
        }

        if (item.server_id > 0) {
            parsedItems.push_back(std::move(item));
        }
    }

    beginResetModel();
    m_items = std::move(parsedItems);
    m_serverIdToRow.clear();
    m_clientIdToRow.clear();

    for (int row = 0; row < static_cast<int>(m_items.size()); ++row) {
        const OtbItem &item = m_items[static_cast<size_t>(row)];
        m_serverIdToRow.insert(item.server_id, row);
        if (item.client_id > 0) {
            m_clientIdToRow.insert(item.client_id, row);
        }
    }

    m_loaded = true;
    m_filePath = path;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    return true;
}

int OtbReader::clientIdForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return 0;
    }

    return m_items[static_cast<size_t>(it.value())].client_id;
}

int OtbReader::rowForClientId(int clientId) const
{
    if (clientId < 0 || clientId > 65535) return -1;
    return m_clientIdToRow.value(static_cast<uint16_t>(clientId), -1);
}

int OtbReader::topOrderForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return 0;
    }
    return m_items[static_cast<size_t>(it.value())].stack_order;
}

int OtbReader::groupForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return 0;
    }
    return m_items[static_cast<size_t>(it.value())].group;
}

QString OtbReader::groupNameForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it == m_serverIdToRow.end()) {
        return QStringLiteral("(missing from items.otb)");
    }
    return groupName(m_items[static_cast<size_t>(it.value())].group);
}

bool OtbReader::isTeleportItem(int serverId) const
{

    if (groupForServerId(serverId) == static_cast<int>(OtbItemGroup::Teleport)) {
        return true;
    }

    if (m_itemsXml && m_itemsXml->isTeleport(serverId)) {
        return true;
    }

    static const int kKnownTeleportIds[] = { 1387 };
    for (int id : kKnownTeleportIds) {
        if (serverId == id) return true;
    }
    return false;
}

QString OtbReader::nameForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    if (it != m_serverIdToRow.end()) {
        const QString &otbName = m_items[static_cast<size_t>(it.value())].name;
        if (!otbName.isEmpty())
            return otbName;
    }

    if (m_itemsXml) {
        const QString xmlName = m_itemsXml->nameForServerId(serverId);
        if (!xmlName.isEmpty())
            return xmlName;
    }
    return QString();
}

int OtbReader::rowForServerId(int serverId) const
{
    auto it = m_serverIdToRow.find(static_cast<uint16_t>(serverId));
    return it == m_serverIdToRow.end() ? -1 : it.value();
}

QVariantMap OtbReader::detailsAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) {
        return {};
    }

    const OtbItem &item = m_items[static_cast<size_t>(row)];
    const ClientItem *clientItem = m_datReader ? m_datReader->itemByClientId(item.client_id) : nullptr;

    const QString name = nameForServerId(item.server_id);
    const QString title = name.isEmpty()
                              ? QStringLiteral("Item %1").arg(item.server_id)
                              : name;
    const QStringList flags = collectOtbFlags(item);

    QVariantMap details;
    details.insert(QStringLiteral("title"), title);
    details.insert(QStringLiteral("category"), QStringLiteral("Item"));
    details.insert(QStringLiteral("name"), name);
    details.insert(QStringLiteral("description"), item.description);
    details.insert(QStringLiteral("itemId"), item.server_id);
    details.insert(QStringLiteral("serverId"), item.server_id);
    details.insert(QStringLiteral("clientId"), item.client_id);
    details.insert(QStringLiteral("group"), groupName(item.group));
    details.insert(QStringLiteral("groupId"), item.group);
    details.insert(QStringLiteral("speed"), item.speed);
    details.insert(QStringLiteral("maxReadWriteLength"), item.max_read_write_length);
    details.insert(QStringLiteral("maxReadLength"), item.max_read_length);
    details.insert(QStringLiteral("minimapColor"), item.minimap_color);
    details.insert(QStringLiteral("wareId"), item.ware_id);
    details.insert(QStringLiteral("lightLevel"), item.light_level);
    details.insert(QStringLiteral("lightColor"), item.light_color);
    details.insert(QStringLiteral("stackOrder"), item.stack_order);
    details.insert(QStringLiteral("unpassable"), item.is_unpassable);
    details.insert(QStringLiteral("blockMissiles"), item.blocks_missiles);
    details.insert(QStringLiteral("blockPathfinder"), item.blocks_pathfinder);
    details.insert(QStringLiteral("hasElevation"), item.has_elevation);
    details.insert(QStringLiteral("useable"), item.is_useable);
    details.insert(QStringLiteral("pickupable"), item.is_pickupable);
    details.insert(QStringLiteral("moveable"), item.is_moveable);
    details.insert(QStringLiteral("stackable"), item.is_stackable);
    details.insert(QStringLiteral("alwaysOnTop"), item.always_on_top);
    details.insert(QStringLiteral("readable"), item.is_readable);
    details.insert(QStringLiteral("rotatable"), item.is_rotatable);
    details.insert(QStringLiteral("hangable"), item.is_hangable);
    details.insert(QStringLiteral("hookEast"), item.hook_east);
    details.insert(QStringLiteral("hookSouth"), item.hook_south);
    details.insert(QStringLiteral("allowDistRead"), item.allow_dist_read);
    details.insert(QStringLiteral("clientDuration"), item.client_duration);
    details.insert(QStringLiteral("clientCharges"), item.client_charges);
    details.insert(QStringLiteral("ignoreLook"), item.ignore_look);
    details.insert(QStringLiteral("animation"), item.animation);
    details.insert(QStringLiteral("fullGround"), item.full_ground);
    details.insert(QStringLiteral("forceUse"), item.force_use);
    details.insert(QStringLiteral("spriteIds"), clientItem ? toVariantList(clientItem->sprite_ids) : QVariantList());
    details.insert(QStringLiteral("itemWidth"), clientItem ? clientItem->width : 1);
    details.insert(QStringLiteral("itemHeight"), clientItem ? clientItem->height : 1);
    details.insert(QStringLiteral("layers"), clientItem ? clientItem->layers : 1);
    details.insert(QStringLiteral("patternX"), clientItem ? clientItem->pattern_x : 1);
    details.insert(QStringLiteral("patternY"), clientItem ? clientItem->pattern_y : 1);
    details.insert(QStringLiteral("patternZ"), clientItem ? clientItem->pattern_z : 1);
    details.insert(QStringLiteral("frames"), clientItem ? clientItem->frames : 1);
    details.insert(QStringLiteral("spriteCount"), clientItem ? static_cast<int>(clientItem->sprite_ids.size()) : 0);
    details.insert(QStringLiteral("previewSpriteId"), clientItem ? static_cast<quint32>(clientItem->previewSpriteId()) : 0);
    details.insert(QStringLiteral("renderable"), clientItem != nullptr && !clientItem->sprite_ids.empty());
    details.insert(QStringLiteral("flags"), flags);
    details.insert(QStringLiteral("flagsText"), flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(", ")));

    QVariantList rows;
    addRow(rows, QStringLiteral("Name"), name.isEmpty() ? QStringLiteral("-") : name);
    addRow(rows, QStringLiteral("Server ID"), item.server_id);
    addRow(rows, QStringLiteral("Client ID"), item.client_id);
    addRow(rows, QStringLiteral("Group"), groupName(item.group));
    addRow(rows, QStringLiteral("Size"), clientItem ? QStringLiteral("%1x%2").arg(clientItem->width).arg(clientItem->height) : QStringLiteral("-"));
    addRow(rows, QStringLiteral("Layers"), clientItem ? clientItem->layers : 0);
    addRow(rows, QStringLiteral("Pattern X"), clientItem ? clientItem->pattern_x : 0);
    addRow(rows, QStringLiteral("Pattern Y"), clientItem ? clientItem->pattern_y : 0);
    addRow(rows, QStringLiteral("Pattern Z"), clientItem ? clientItem->pattern_z : 0);
    addRow(rows, QStringLiteral("Frames"), clientItem ? clientItem->frames : 0);
    addRow(rows, QStringLiteral("Sprites"), clientItem ? static_cast<int>(clientItem->sprite_ids.size()) : 0);
    addRow(rows, QStringLiteral("First sprite"), clientItem ? static_cast<quint32>(clientItem->previewSpriteId()) : 0);
    if (item.speed != 0) addRow(rows, QStringLiteral("Speed"), item.speed);
    if (item.max_read_length != 0) addRow(rows, QStringLiteral("Read length"), item.max_read_length);
    if (item.max_read_write_length != 0) addRow(rows, QStringLiteral("Read / write length"), item.max_read_write_length);
    if (item.minimap_color != 0) addRow(rows, QStringLiteral("Minimap color"), item.minimap_color);
    if (item.ware_id != 0) addRow(rows, QStringLiteral("Ware ID"), item.ware_id);
    if (item.light_level != 0 || item.light_color != 0) {
        addRow(rows, QStringLiteral("Light"), QStringLiteral("%1 / %2").arg(item.light_level).arg(item.light_color));
    }
    addRow(rows, QStringLiteral("Stack order"), item.stack_order);
    addRow(rows, QStringLiteral("Flags"), details.value(QStringLiteral("flagsText")));

    if (!item.description.isEmpty()) {
        addRow(rows, QStringLiteral("Description"), item.description);
    }

    details.insert(QStringLiteral("rows"), rows);
    return details;
}

bool OtbReader::setValue(int row, const QString &key, const QVariant &value)
{
    if (row < 0 || row >= static_cast<int>(m_items.size()))
        return false;
    if (key == "serverId" || key == "clientId") {
        bool valid = false;
        const uint32_t id = value.toUInt(&valid);
        if (!valid || id < (key == "serverId" ? 1u : 100u) || id > 65535u) return false;
        const int existing = key == "serverId" ? rowForServerId(int(id)) : rowForClientId(int(id));
        if (existing >= 0 && existing != row) return false;
    }
    OtbItem &item = m_items[static_cast<size_t>(row)];
    auto flag = [&item](uint32_t bit, bool on) {
        item.flags = on ? (item.flags | bit) : (item.flags & ~bit);
    };
    if (key == "name") {
        item.name = value.toString();
        if (m_itemsXml)
            m_itemsXml->setNameForServerId(item.server_id, item.name);
    }
    else if (key == "description") item.description = value.toString();
    else if (key == "serverId") {
        m_serverIdToRow.remove(item.server_id);
        item.server_id = static_cast<uint16_t>(value.toUInt());
        m_serverIdToRow.insert(item.server_id, row);
    }
    else if (key == "clientId") {
        m_clientIdToRow.remove(item.client_id);
        item.client_id = static_cast<uint16_t>(value.toUInt());
        if (item.client_id > 0)
            m_clientIdToRow.insert(item.client_id, row);
    }
    else if (key == "groupId") item.group = static_cast<uint8_t>(value.toUInt());
    else if (key == "speed") item.speed = static_cast<uint16_t>(value.toUInt());
    else if (key == "maxReadWriteLength") item.max_read_write_length = static_cast<uint16_t>(value.toUInt());
    else if (key == "maxReadLength") item.max_read_length = static_cast<uint16_t>(value.toUInt());
    else if (key == "minimapColor") item.minimap_color = static_cast<uint16_t>(value.toUInt());
    else if (key == "wareId") item.ware_id = static_cast<uint16_t>(value.toUInt());
    else if (key == "lightLevel") item.light_level = static_cast<uint8_t>(value.toUInt());
    else if (key == "lightColor") item.light_color = static_cast<uint8_t>(value.toUInt());
    else if (key == "stackOrder") item.stack_order = static_cast<int8_t>(value.toInt());
#define EDIT_BOOL(KEY, MEMBER, BIT) else if (key == KEY) { item.MEMBER = value.toBool(); flag(BIT, item.MEMBER); }
    EDIT_BOOL("unpassable", is_unpassable, Unpassable)
    EDIT_BOOL("blockMissiles", blocks_missiles, BlockMissiles)
    EDIT_BOOL("blockPathfinder", blocks_pathfinder, BlockPathfinder)
    EDIT_BOOL("hasElevation", has_elevation, HasElevation)
    EDIT_BOOL("useable", is_useable, Useable)
    EDIT_BOOL("pickupable", is_pickupable, Pickupable)
    EDIT_BOOL("moveable", is_moveable, Moveable)
    EDIT_BOOL("stackable", is_stackable, Stackable)
    EDIT_BOOL("alwaysOnTop", always_on_top, AlwaysOnTop)
    EDIT_BOOL("readable", is_readable, Readable)
    EDIT_BOOL("rotatable", is_rotatable, Rotatable)
    EDIT_BOOL("hangable", is_hangable, Hangable)
    EDIT_BOOL("hookEast", hook_east, HookEast)
    EDIT_BOOL("hookSouth", hook_south, HookSouth)
    EDIT_BOOL("allowDistRead", allow_dist_read, AllowDistRead)
    EDIT_BOOL("clientDuration", client_duration, ClientDuration)
    EDIT_BOOL("clientCharges", client_charges, ClientCharges)
    EDIT_BOOL("ignoreLook", ignore_look, LookThrough)
    EDIT_BOOL("animation", animation, Animation)
    EDIT_BOOL("fullGround", full_ground, FullTile)
    EDIT_BOOL("forceUse", force_use, ForceUse)
#undef EDIT_BOOL
    else return false;
    m_dirty = true;
    emit dirtyChanged();

    QList<int> changedRoles{ItemIdRole};
    if (key == QStringLiteral("name")) {
        changedRoles.append(NameRole);
    } else if (key == QStringLiteral("serverId")) {
        changedRoles.append(ServerIdRole);
    } else if (key == QStringLiteral("clientId")) {
        changedRoles.append(ClientIdRole);
        changedRoles.append(SpriteIdsRole);
        changedRoles.append(ItemWidthRole);
        changedRoles.append(ItemHeightRole);
        changedRoles.append(LayersRole);
        changedRoles.append(IsRenderableRole);
    } else if (key == QStringLiteral("groupId")) {
        changedRoles.append(GroupRole);
        changedRoles.append(IsGroundRole);
    } else if (key == QStringLiteral("stackable")) {
        changedRoles.append(IsStackableRole);
    } else if (key == QStringLiteral("unpassable")) {
        changedRoles.append(IsUnpassableRole);
    }
    emit dataChanged(index(row), index(row), changedRoles);
    return true;
}

bool OtbReader::saveFile(const QString &path)
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (!m_loaded || target.isEmpty()) {
        setError(QStringLiteral("No OTB target file selected"));
        return false;
    }

    if (!writeFile(target)) return false;
    m_filePath = target;
    m_dirty = false;
    emit dirtyChanged();
    emit loadedChanged();
    return true;
}

bool OtbReader::saveCopy(const QString &path)
{
    if (!m_loaded || path.isEmpty()) {
        setError(QStringLiteral("No OTB target file selected"));
        return false;
    }
    return writeFile(path);
}

bool OtbReader::writeFile(const QString &target)
{
    QByteArray output(4, '\0');
    output.append(static_cast<char>(0xfe));
    appendEscaped(output, m_rootData);

    for (OtbItem &item : m_items) {
        replaceAttribute(item, 0x10, u16Payload(item.server_id));
        replaceAttribute(item, 0x11, u16Payload(item.client_id));
        replaceAttribute(item, 0x12, item.name.isEmpty() ? QByteArray() : stringPayload(item.name), true);
        replaceAttribute(item, 0x13, item.description.isEmpty() ? QByteArray() : stringPayload(item.description), true);
        replaceAttribute(item, 0x14, u16Payload(item.speed));
        replaceAttribute(item, 0x21, u16Payload(item.minimap_color));
        replaceAttribute(item, 0x22, u16Payload(item.max_read_write_length));
        replaceAttribute(item, 0x23, u16Payload(item.max_read_length));
        replaceAttribute(item, 0x2d, u16Payload(item.ware_id));
        QByteArray light;
        appendU16(light, item.light_level);
        appendU16(light, item.light_color);
        replaceAttribute(item, 0x2a, light);
        replaceAttribute(item, 0x2b, QByteArray(1, static_cast<char>(item.stack_order)));

        QByteArray node;
        node.append(static_cast<char>(item.group));
        appendU32(node, item.flags);
        for (const auto &attribute : std::as_const(item.raw_attributes)) {
            node.append(static_cast<char>(attribute.first));
            appendU16(node, static_cast<uint16_t>(attribute.second.size()));
            node.append(attribute.second);
        }
        output.append(static_cast<char>(0xfe));
        appendEscaped(output, node);
        output.append(static_cast<char>(0xff));
    }
    output.append(static_cast<char>(0xff));

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size() || !file.commit()) {
        setError(QStringLiteral("Cannot save OTB file: %1").arg(target));
        return false;
    }
    return true;
}

void OtbReader::syncNamesToItemsXml()
{
    if (!m_itemsXml || !m_itemsXml->hasData()) return;

    for (const OtbItem &item : m_items) {
        if (item.server_id == 0 || item.name.isEmpty()) continue;
        if (m_itemsXml->nameForServerId(item.server_id) != item.name)
            m_itemsXml->setNameForServerId(item.server_id, item.name);
    }
}

int OtbReader::createItem(int clientId)
{
    OtbItem item;
    item.server_id = m_items.empty() ? 100 : static_cast<uint16_t>(m_items.back().server_id + 1);
    item.client_id = static_cast<uint16_t>(std::clamp(clientId, 0, 65535));
    const int row = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    m_items.push_back(item);
    m_serverIdToRow.insert(item.server_id, row);
    if (item.client_id) m_clientIdToRow.insert(item.client_id, row);
    endInsertRows();
    m_dirty = true;
    emit itemCountChanged();
    emit dirtyChanged();
    return row;
}

int OtbReader::duplicateItem(int row)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return -1;
    OtbItem copy = m_items[static_cast<size_t>(row)];
    copy.server_id = m_items.empty() ? 100 : static_cast<uint16_t>(m_items.back().server_id + 1);
    const int newRow = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_items.push_back(copy);
    m_serverIdToRow.insert(copy.server_id, newRow);
    endInsertRows();
    m_dirty = true;
    emit itemCountChanged();
    emit dirtyChanged();
    return newRow;
}

bool OtbReader::removeItem(int row)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.erase(m_items.begin() + row);
    endRemoveRows();
    m_serverIdToRow.clear();
    m_clientIdToRow.clear();
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const OtbItem &item = m_items[static_cast<size_t>(i)];
        if (item.server_id) m_serverIdToRow.insert(item.server_id, i);
        if (item.client_id) m_clientIdToRow.insert(item.client_id, i);
    }
    m_dirty = true;
    emit itemCountChanged();
    emit dirtyChanged();
    return true;
}

bool OtbReader::reloadItem(int row)
{
    if (!m_datReader || row < 0 || row >= static_cast<int>(m_items.size())) return false;
    OtbItem &item = m_items[static_cast<size_t>(row)];
    const ClientItem *client = m_datReader->itemByClientId(item.client_id);
    if (!client) return false;
    item.is_unpassable = client->is_unpassable;
    item.blocks_missiles = client->blocks_missiles;
    item.blocks_pathfinder = client->blocks_pathfinder;
    item.is_useable = client->is_useable;
    item.is_pickupable = client->is_pickupable;
    item.is_moveable = !client->is_unmoveable;
    item.is_stackable = client->is_stackable;
    item.always_on_top = client->is_ground_border || client->is_on_bottom || client->is_on_top;
    item.stack_order = client->is_on_top ? 3
                     : client->is_on_bottom ? 2
                     : client->is_ground_border ? 1 : 0;
    item.is_readable = client->is_writable;
    item.is_rotatable = client->is_rotatable;
    item.is_hangable = client->is_hangable;
    item.hook_east = client->is_horizontal;
    item.hook_south = client->is_vertical;
    item.has_elevation = client->has_elevation;
    item.ignore_look = client->ignore_look;
    item.animation = client->animate_always;
    item.full_ground = client->full_ground;
    item.speed = client->ground_speed;
    item.light_level = static_cast<uint8_t>(client->light_level);
    item.light_color = static_cast<uint8_t>(client->light_color);
    item.max_read_write_length = client->max_text_length;
    uint32_t flags = item.flags;
    auto set = [&flags](uint32_t bit, bool on) { flags = on ? flags | bit : flags & ~bit; };
    set(Unpassable, item.is_unpassable); set(BlockMissiles, item.blocks_missiles);
    set(BlockPathfinder, item.blocks_pathfinder); set(Useable, item.is_useable);
    set(Pickupable, item.is_pickupable); set(Moveable, item.is_moveable);
    set(Stackable, item.is_stackable); set(Readable, item.is_readable);
    set(AlwaysOnTop, item.always_on_top);
    set(Rotatable, item.is_rotatable); set(Hangable, item.is_hangable);
    set(HookEast, item.hook_east); set(HookSouth, item.hook_south);
    set(HasElevation, item.has_elevation);
    set(AllowDistRead, item.allow_dist_read);
    set(ClientDuration, item.client_duration); set(ClientCharges, item.client_charges);
    set(LookThrough, item.ignore_look); set(Animation, item.animation);
    set(FullTile, item.full_ground);
    set(ForceUse, item.force_use);
    item.flags = flags;
    m_dirty = true;
    emit dirtyChanged();
    emit dataChanged(index(row), index(row));
    return true;
}

int OtbReader::reloadAllItems()
{
    int count = 0;
    for (int row = 0; row < static_cast<int>(m_items.size()); ++row)
        if (isMismatched(row) && reloadItem(row)) ++count;
    return count;
}

int OtbReader::createMissingItems()
{
    return createMissingItems({});
}

int OtbReader::createMissingItems(const std::function<void(int, int)> &progress)
{
    if (!m_datReader) return 0;
    QVector<int> missing;
    for (int clientId = 100; clientId < 65536; ++clientId) {
        if (!m_datReader->itemByClientId(static_cast<uint16_t>(clientId))) break;
        if (!m_clientIdToRow.contains(static_cast<uint16_t>(clientId))) missing.append(clientId);
    }
    const int total = missing.size();
    if (progress) progress(0, total);
    int count = 0;
    constexpr int batchSize = 256;
    while (count < total) {
        const int batch = std::min(batchSize, total - count);
        const int first = static_cast<int>(m_items.size());
        beginInsertRows(QModelIndex(), first, first + batch - 1);
        for (int offset = 0; offset < batch; ++offset) {
            OtbItem item;
            item.server_id = m_items.empty() ? 100 : static_cast<uint16_t>(m_items.back().server_id + 1);
            item.client_id = static_cast<uint16_t>(missing[count + offset]);
            m_serverIdToRow.insert(item.server_id, first + offset);
            m_clientIdToRow.insert(item.client_id, first + offset);
            m_items.push_back(std::move(item));
        }
        endInsertRows();
        {
            QSignalBlocker blocker(this);
            for (int row = first; row < first + batch; ++row) reloadItem(row);
        }
        m_dirty = true;
        emit dataChanged(index(first), index(first + batch - 1));
        emit itemCountChanged();
        emit dirtyChanged();
        count += batch;
        if (progress) progress(count, total);
    }
    return count;
}

void OtbReader::newFile(quint32 minorVersion)
{
    reset();
    m_majorVersion = 3;
    m_minorVersion = minorVersion;
    m_buildNumber = 1;
    m_rootData.append(char(0));
    appendU32(m_rootData, 0);
    m_rootData.append(char(1));
    appendU16(m_rootData, 12);
    appendU32(m_rootData, m_majorVersion);
    appendU32(m_rootData, m_minorVersion);
    appendU32(m_rootData, m_buildNumber);
    m_loaded = true;
    m_dirty = true;
    createItem(100);
    emit loadedChanged();
}

void OtbReader::setOtbVersion(quint32 major, quint32 minor, quint32 build)
{
    m_majorVersion = major;
    m_minorVersion = minor;
    m_buildNumber = build;
    if (m_rootData.size() >= 20) {
        auto write32 = [this](int pos, uint32_t value) {
            for (int i = 0; i < 4; ++i) m_rootData[pos + i] = char((value >> (i * 8)) & 0xff);
        };
        write32(8, major); write32(12, minor); write32(16, build);
    }
    m_dirty = true;
    emit dirtyChanged();
    emit loadedChanged();
}

bool OtbReader::isMismatched(int row) const
{
    if (!m_datReader || row < 0 || row >= static_cast<int>(m_items.size())) return false;
    const OtbItem &item = m_items[static_cast<size_t>(row)];
    const ClientItem *client = m_datReader->itemByClientId(item.client_id);
    if (!client) return true;
    return item.is_unpassable != client->is_unpassable
        || item.blocks_missiles != client->blocks_missiles
        || item.blocks_pathfinder != client->blocks_pathfinder
        || item.is_pickupable != client->is_pickupable
        || item.is_moveable == client->is_unmoveable
        || item.is_stackable != client->is_stackable
        || item.always_on_top != (client->is_ground_border || client->is_on_bottom || client->is_on_top)
        || item.stack_order != (client->is_on_top ? 3
                              : client->is_on_bottom ? 2
                              : client->is_ground_border ? 1 : 0);
}

int OtbReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant OtbReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const OtbItem &item = m_items[static_cast<size_t>(index.row())];
    const ClientItem *clientItem = m_datReader ? m_datReader->itemByClientId(item.client_id) : nullptr;

    switch (role) {
    case ItemIdRole:
    case ServerIdRole:
        return item.server_id;
    case ClientIdRole:
        return item.client_id;
    case NameRole:

        return nameForServerId(item.server_id);
    case GroupRole:
        return item.group;
    case SpriteIdsRole:
        return clientItem ? toVariantList(clientItem->sprite_ids) : QVariantList();
    case ItemWidthRole:
        return clientItem ? clientItem->width : 1;
    case ItemHeightRole:
        return clientItem ? clientItem->height : 1;
    case LayersRole:
        return clientItem ? clientItem->layers : 1;
    case IsRenderableRole:
        return clientItem != nullptr && !clientItem->sprite_ids.empty();
    case IsGroundRole:
        return item.group == static_cast<uint8_t>(OtbItemGroup::Ground);
    case IsStackableRole:
        return item.is_stackable;
    case IsContainerRole:
        return item.group == static_cast<uint8_t>(OtbItemGroup::Container);
    case IsUnpassableRole:
        return item.is_unpassable;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> OtbReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemIdRole] = "itemId";
    roles[ServerIdRole] = "serverId";
    roles[ClientIdRole] = "clientId";
    roles[NameRole] = "itemName";
    roles[GroupRole] = "itemGroup";
    roles[SpriteIdsRole] = "spriteIds";
    roles[ItemWidthRole] = "itemWidth";
    roles[ItemHeightRole] = "itemHeight";
    roles[LayersRole] = "layers";
    roles[IsRenderableRole] = "isRenderable";
    roles[IsGroundRole] = "isGround";
    roles[IsStackableRole] = "isStackable";
    roles[IsContainerRole] = "isContainer";
    roles[IsUnpassableRole] = "isUnpassable";
    return roles;
}

void OtbReader::refreshDatRoles()
{
    if (m_items.empty()) {
        return;
    }

    const QModelIndex first = index(0, 0);
    const QModelIndex last = index(static_cast<int>(m_items.size()) - 1, 0);
    emit dataChanged(first, last, {SpriteIdsRole, ItemWidthRole, ItemHeightRole, LayersRole, IsRenderableRole});
}
