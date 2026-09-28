#include "datreader.h"
#include "canonicalflags.h"
#include <algorithm>
#include <QDataStream>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QtEndian>

using namespace CanonicalFlags;

namespace {

QVariantList toVariantList(const std::vector<uint32_t> &values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (uint32_t value : values) {
        result.append(QVariant::fromValue(static_cast<quint32>(value)));
    }
    return result;
}

void addRow(QVariantList &rows, const QString &name, const QVariant &value)
{
    QVariantMap row;
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("value"), value);
    rows.append(row);
}

QStringList collectDatFlags(const ClientItem &item)
{
    QStringList flags;
    if (item.is_ground) flags << QStringLiteral("Ground");
    if (item.is_ground_border) flags << QStringLiteral("Ground border");
    if (item.is_on_bottom) flags << QStringLiteral("On bottom");
    if (item.is_on_top) flags << QStringLiteral("On top");
    if (item.is_container) flags << QStringLiteral("Container");
    if (item.is_stackable) flags << QStringLiteral("Stackable");
    if (item.is_useable) flags << QStringLiteral("Useable");
    if (item.is_writable) flags << QStringLiteral("Writable");
    if (item.is_fluid_container) flags << QStringLiteral("Fluid container");
    if (item.is_fluid) flags << QStringLiteral("Fluid");
    if (item.is_unpassable) flags << QStringLiteral("Unpassable");
    if (item.is_unmoveable) flags << QStringLiteral("Unmoveable");
    if (item.blocks_missiles) flags << QStringLiteral("Blocks missiles");
    if (item.blocks_pathfinder) flags << QStringLiteral("Blocks pathfinder");
    if (item.is_pickupable) flags << QStringLiteral("Pickupable");
    if (item.is_hangable) flags << QStringLiteral("Hangable");
    if (item.is_horizontal) flags << QStringLiteral("Hook east");
    if (item.is_vertical) flags << QStringLiteral("Hook south");
    if (item.is_rotatable) flags << QStringLiteral("Rotatable");
    if (item.has_light) flags << QStringLiteral("Light");
    if (item.dont_hide) flags << QStringLiteral("Dont hide");
    if (item.is_translucent) flags << QStringLiteral("Translucent");
    if (item.has_offset) flags << QStringLiteral("Offset");
    if (item.has_elevation) flags << QStringLiteral("Elevation");
    if (item.is_lying_object) flags << QStringLiteral("Lying object");
    if (item.animate_always) flags << QStringLiteral("Animate always");
    if (item.has_minimap_color) flags << QStringLiteral("Minimap color");
    if (item.full_ground) flags << QStringLiteral("Full ground");
    if (item.ignore_look) flags << QStringLiteral("Ignore look");
    if (item.floor_change) flags << QStringLiteral("Floor change");
    return flags;
}

}

uint8_t DatReader::transformFlag(uint8_t raw) const
{

    if (m_clientVersion >= 1010) {

        if (raw == 0xFE) return USABLE;
        if (raw == 16) return NO_MOVE_ANIMATION;
        if (raw > 16)  return raw - 1;
        return raw;
    }
    if (m_clientVersion >= 860) {
        return raw;
    }
    if (m_clientVersion >= 780) {

        if (raw == 8) return CHARGEABLE;
        if (raw > 8)  return raw - 1;
        return raw;
    }

    return (raw == 23) ? FLOOR_CHANGE : raw;
}

uint8_t DatReader::encodeFlag(uint8_t flag) const
{
    if (m_clientVersion >= 1010) {
        if (flag == USABLE) return 0xFE;
        if (flag == NO_MOVE_ANIMATION) return 16;
        return flag >= 16 ? static_cast<uint8_t>(flag + 1) : flag;
    }
    if (m_clientVersion >= 860) return flag;
    if (m_clientVersion >= 780) {
        if (flag == CHARGEABLE) return 8;
        return flag >= 8 ? static_cast<uint8_t>(flag + 1) : flag;
    }
    return flag == FLOOR_CHANGE ? 23 : flag;
}

void DatReader::setClientVersion(int v)
{
    if (m_clientVersion == v) return;
    m_clientVersion = v;
    emit clientVersionChanged();
}

DatReader::DatReader(QObject *parent)
    : QAbstractListModel(parent)
{
}

DatReader::~DatReader() = default;

void DatReader::reset()
{
    beginResetModel();
    m_items.clear();
    m_effects.clear();
    m_missiles.clear();
    m_outfits.clear();
    m_signature = 0;
    m_maxItemId = 0;
    m_loaded = false;
    m_dirty = false;
    m_filePath.clear();
    m_categoryTail.clear();
    m_categoryStructureChanged = false;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    emit dirtyChanged();
}

void DatReader::setError(const QString &message)
{
    m_errorString = message;
    emit errorChanged();
}

bool DatReader::loadFile(const QString &path, quint32 expectedSignature)
{
    reset();
    setError({});

    BinaryReader reader(path);
    if (!reader.isOpen()) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }

    m_signature = reader.readU32();
    if (expectedSignature != 0 && m_signature != expectedSignature) {
        setError(QStringLiteral("Invalid .dat signature (expected 0x%1, got 0x%2)")
                      .arg(expectedSignature, 0, 16)
                      .arg(m_signature, 0, 16));
        return false;
    }

    m_maxItemId                   = reader.readU16();
    const uint16_t maxOutfitId    = reader.readU16();
    const uint16_t maxEffectId    = reader.readU16();
    const uint16_t maxMissileId   = reader.readU16();

    if (!reader.good()) {
        setError(QStringLiteral("Failed to read the .dat header"));
        return false;
    }

    std::vector<ClientItem> items;
    std::vector<ClientItem> missiles;
    std::vector<ClientItem> effects;
    std::vector<ClientItem> outfits;

    readCategory(reader, &items, 100, m_maxItemId);

    if (!reader.good() && reader.hasError()) {
        setError(QStringLiteral("Failed to parse items: %1").arg(reader.getError()));
        return false;
    }

    const size_t categoryTailOffset = reader.tell();
    readCategory(reader, &outfits, 1, maxOutfitId, true);
    readCategory(reader, &effects, 1, maxEffectId);
    readCategory(reader, &missiles, 1, maxMissileId);
    if (!reader.good() || reader.remaining() != 0) {
        setError(QStringLiteral("Invalid DAT category data or mismatched client version"));
        return false;
    }

    beginResetModel();
    m_items = std::move(items);
    m_effects = std::move(effects);
    m_missiles = std::move(missiles);
    m_outfits = std::move(outfits);
    m_maxOutfitId = maxOutfitId;
    m_maxEffectId = maxEffectId;
    m_maxMissileId = maxMissileId;
    m_filePath = path;
    if (reader.seek(categoryTailOffset)) {
        const auto tail = reader.readBytes(reader.remaining());
        m_categoryTail = QByteArray(reinterpret_cast<const char *>(tail.data()), static_cast<qsizetype>(tail.size()));
    }
    m_loaded = true;
    m_dirty = false;
    m_categoryStructureChanged = false;
    endResetModel();

    emit itemCountChanged();
    emit loadedChanged();
    emit dirtyChanged();

    return true;
}

bool DatReader::saveFile(const QString &path)
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (!m_loaded || target.isEmpty()) {
        setError(QStringLiteral("No DAT target file selected"));
        return false;
    }

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(QStringLiteral("Cannot write DAT file: %1").arg(target));
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out << static_cast<quint32>(m_signature)
        << static_cast<quint16>(m_items.empty() ? 99 : 99 + m_items.size())
        << static_cast<quint16>(m_maxOutfitId)
        << static_cast<quint16>(m_maxEffectId)
        << static_cast<quint16>(m_maxMissileId);

    auto flag = [&](uint8_t canonical) { out << static_cast<quint8>(encodeFlag(canonical)); };
    auto writeItem = [&](const ClientItem &item, bool outfit = false, bool preserveFlags = false) {
        if (!item.modified && !item.raw_record.isEmpty()) {
            out.writeRawData(item.raw_record.constData(), item.raw_record.size());
            return;
        }
        if(preserveFlags && !item.raw_flags.isEmpty())
            out.writeRawData(item.raw_flags.constData(),item.raw_flags.size());
        else {
        if (item.is_ground) { flag(GROUND); out << static_cast<quint16>(item.ground_speed); }
        if (item.is_ground_border) flag(GROUND_BORDER);
        if (item.is_on_bottom) flag(ON_BOTTOM);
        if (item.is_on_top) flag(ON_TOP);
        if (item.is_container) flag(CONTAINER);
        if (item.is_stackable) flag(STACKABLE);
        if (item.is_useable) flag(MULTI_USE);
        if (item.is_writable) { flag(item.writable_once ? WRITABLE_ONCE : WRITABLE); out << static_cast<quint16>(item.max_text_length); }
        if (item.is_fluid_container) flag(FLUID_CONTAINER);
        if (item.is_fluid) flag(FLUID);
        if (item.is_unpassable) flag(UNPASSABLE);
        if (item.is_unmoveable) flag(UNMOVEABLE);
        if (item.blocks_missiles) flag(BLOCK_MISSILE);
        if (item.blocks_pathfinder) flag(BLOCK_PATHFINDER);
        if (item.is_pickupable) flag(PICKUPABLE);
        if (item.is_hangable) flag(HANGABLE);
        if (item.is_vertical) flag(HOOK_SOUTH);
        if (item.is_horizontal) flag(HOOK_EAST);
        if (item.is_rotatable) flag(ROTATABLE);
        if (item.has_light) { flag(HAS_LIGHT); out << static_cast<quint16>(item.light_level) << static_cast<quint16>(item.light_color); }
        if (item.dont_hide) flag(DONT_HIDE);
        if (item.is_translucent) flag(TRANSLUCENT);
        if (item.has_offset) { flag(HAS_OFFSET); out << static_cast<quint16>(item.offset_x) << static_cast<quint16>(item.offset_y); }
        if (item.has_elevation) { flag(HAS_ELEVATION); out << static_cast<quint16>(item.elevation); }
        if (item.is_lying_object) flag(LYING_OBJECT);
        if (item.animate_always) flag(ANIMATE_ALWAYS);
        if (item.has_minimap_color) { flag(MINI_MAP); out << static_cast<quint16>(item.minimap_color); }
        if (item.lens_help) { flag(LENS_HELP); out << static_cast<quint16>(item.lens_help); }
        if (item.full_ground) flag(FULL_GROUND);
        if (item.ignore_look) flag(IGNORE_LOOK);
        if (item.floor_change) flag(FLOOR_CHANGE);
        out.writeRawData(item.extra_flags.constData(), item.extra_flags.size());
        out << static_cast<quint8>(LAST);
        }

        auto writeGroup = [&](const ClientFrameGroup &group) {
            if (outfit && frameGroups()) out << static_cast<quint8>(group.type);
            out << static_cast<quint8>(group.width) << static_cast<quint8>(group.height);
            if (group.width > 1 || group.height > 1) out << static_cast<quint8>(group.exact_size);
            out << static_cast<quint8>(group.layers)
                << static_cast<quint8>(group.pattern_x)
                << static_cast<quint8>(group.pattern_y)
                << static_cast<quint8>(group.pattern_z)
                << static_cast<quint8>(group.frames);
            if (group.frames > 1 && frameDurations()) {
                if (group.animation_data.size() == 6 + group.frames * 8) {
                    out.writeRawData(group.animation_data.constData(), group.animation_data.size());
                } else {
                    out << static_cast<quint8>(0) << static_cast<quint32>(0) << static_cast<quint8>(0);
                    for (uint32_t f = 0; f < group.frames; ++f)
                        out << static_cast<quint32>(100) << static_cast<quint32>(100);
                }
            }
            const uint32_t count = group.getTotalSprites();
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t id = i < group.sprite_ids.size() ? group.sprite_ids[i] : 0;
                if (extendedSprites()) out << static_cast<quint32>(id);
                else out << static_cast<quint16>(id);
            }
        };
        if (outfit && frameGroups())
            out << static_cast<quint8>(std::max<size_t>(1,item.frame_groups.size()));
        if (!outfit || item.frame_groups.empty()) {
            ClientFrameGroup group;
            group.width=item.width; group.height=item.height; group.exact_size=item.exact_size;
            group.layers=item.layers; group.pattern_x=item.pattern_x; group.pattern_y=item.pattern_y;
            group.pattern_z=item.pattern_z; group.frames=item.frames;
            group.animation_data=item.animation_data; group.sprite_ids=item.sprite_ids;
            writeGroup(group);
        } else {
            const size_t count=outfit && frameGroups() ? item.frame_groups.size() : 1;
            for(size_t i=0;i<count;++i) writeGroup(item.frame_groups[i]);
        }
    };

    for (const ClientItem &item : m_items) writeItem(item,false,item.preserve_raw_flags);
    const auto modified = [](const auto &items) {
        return std::any_of(items.begin(),items.end(),[](const ClientItem &item){return item.modified;});
    };
    if (!m_categoryStructureChanged && !modified(m_outfits) && !modified(m_effects) && !modified(m_missiles)) {
        if (!m_categoryTail.isEmpty()) out.writeRawData(m_categoryTail.constData(), m_categoryTail.size());
    } else {
        for (const ClientItem &item : m_outfits) writeItem(item,true,true);
        for (const ClientItem &item : m_effects) writeItem(item,false,true);
        for (const ClientItem &item : m_missiles) writeItem(item,false,true);
    }
    if (out.status() != QDataStream::Ok || !file.commit()) {
        setError(QStringLiteral("Failed to compile DAT file: %1").arg(target));
        return false;
    }
    m_filePath = target;
    m_dirty = false;
    m_categoryStructureChanged = false;
    // Modified records continue to serialize from current state on subsequent saves.
    // Keeping old raw_record with modified=false would silently revert edits.
    emit dirtyChanged();
    emit loadedChanged();
    return true;
}

void DatReader::readCategory(BinaryReader &reader,
                              std::vector<ClientItem> *outItems,
                              uint16_t minId, uint16_t maxId, bool outfits)
{
    if (outItems && maxId >= minId) {
        outItems->reserve(static_cast<size_t>(maxId) - minId + 1);
    }

    for (int id = static_cast<int>(minId); id <= static_cast<int>(maxId); ++id) {
        if (!reader.good()) break;

        ClientItem item;
        item.id = static_cast<uint16_t>(id);
        const size_t recordStart = reader.tell();
        readItemFlags(item, reader);
        const size_t flagsEnd=reader.tell();
        if(reader.seek(recordStart)){
            const auto flags=reader.readBytes(flagsEnd-recordStart);
            item.raw_flags=QByteArray(reinterpret_cast<const char *>(flags.data()),static_cast<qsizetype>(flags.size()));
            reader.seek(flagsEnd);
        }
        readSpriteData(item, reader, outfits);

        const size_t recordEnd = reader.tell();
        if (reader.seek(recordStart)) {
            const auto raw = reader.readBytes(recordEnd - recordStart);
            item.raw_record = QByteArray(reinterpret_cast<const char *>(raw.data()), static_cast<qsizetype>(raw.size()));
            reader.seek(recordEnd);
        }

        if (outItems) {
            outItems->push_back(std::move(item));
        }

    }
}

void DatReader::readItemFlags(ClientItem &item, BinaryReader &reader)
{
    while (true) {
        const size_t flagStart = reader.tell();
        const uint8_t rawFlag = reader.readU8();
        if (!reader.good()) break;
        if (rawFlag == LAST) break;

        const uint8_t flag = transformFlag(rawFlag);

        switch (flag) {
        case GROUND:
            item.is_ground = true;
            item.ground_speed = reader.readU16();
            break;
        case GROUND_BORDER:
            item.is_ground_border = true;
            break;
        case ON_BOTTOM:
            item.is_on_bottom = true;
            break;
        case ON_TOP:
            item.is_on_top = true;
            break;
        case CONTAINER:
            item.is_container = true;
            break;
        case STACKABLE:
            item.is_stackable = true;
            break;
        case FORCE_USE:
            item.extra_properties["forceUse"] = true; break;
        case MULTI_USE:
            item.is_useable = true;
            break;
        case WRITABLE:
        case WRITABLE_ONCE:
            item.writable_once = flag == WRITABLE_ONCE;
            item.is_writable = true;
            item.max_text_length = reader.readU16();
            break;
        case FLUID_CONTAINER:
            item.is_fluid_container = true;
            break;
        case FLUID:
            item.is_fluid = true;
            break;
        case UNPASSABLE:
            item.is_unpassable = true;
            break;
        case UNMOVEABLE:
            item.is_unmoveable = true;
            break;
        case BLOCK_MISSILE:
            item.blocks_missiles = true;
            break;
        case BLOCK_PATHFINDER:
            item.blocks_pathfinder = true;
            break;
        case PICKUPABLE:
            item.is_pickupable = true;
            break;
        case HANGABLE:
            item.is_hangable = true;
            break;
        case HOOK_SOUTH:
            item.is_vertical = true;
            break;
        case HOOK_EAST:
            item.is_horizontal = true;
            break;
        case ROTATABLE:
            item.is_rotatable = true;
            break;
        case HAS_LIGHT:
            item.has_light = true;
            item.light_level = reader.readU16();
            item.light_color = reader.readU16();
            break;
        case DONT_HIDE:
            item.dont_hide = true;
            break;
        case TRANSLUCENT:
            item.is_translucent = true;
            break;
        case HAS_OFFSET:
            item.has_offset = true;
            item.offset_x = static_cast<int16_t>(reader.readU16());
            item.offset_y = static_cast<int16_t>(reader.readU16());
            break;
        case HAS_ELEVATION:
            item.has_elevation = true;
            item.elevation = reader.readU16();
            break;
        case LYING_OBJECT:
            item.is_lying_object = true;
            break;
        case ANIMATE_ALWAYS:
            item.animate_always = true;
            break;
        case MINI_MAP:
            item.has_minimap_color = true;
            item.minimap_color = reader.readU16();
            break;
        case LENS_HELP:
            item.lens_help = reader.readU16();
            break;
        case FULL_GROUND:
            item.full_ground = true;
            break;
        case IGNORE_LOOK:
            item.ignore_look = true;
            break;
        case FLOOR_CHANGE:
            item.floor_change = true;
            break;

        case CLOTH:
            item.extra_properties["hasCloth"] = true;
            item.extra_properties["clothSlot"] = reader.readU16(); break;
        case MARKET_ITEM:
            item.extra_properties["hasMarket"] = true;
            item.extra_properties["marketCategory"] = reader.readU16();
            item.extra_properties["marketTradeAs"] = reader.readU16();
            item.extra_properties["marketShowAs"] = reader.readU16();
            item.extra_properties["marketName"] = reader.readString();
            item.extra_properties["marketVocation"] = reader.readU16();
            item.extra_properties["marketLevel"] = reader.readU16(); break;
        case DEFAULT_ACTION:
            item.extra_properties["hasAction"] = true;
            item.extra_properties["defaultAction"] = reader.readU16(); break;
        case WRAPPABLE: item.extra_properties["wrappable"] = true; break;
        case UNWRAPPABLE: item.extra_properties["unwrappable"] = true; break;
        case TOP_EFFECT: item.extra_properties["topEffect"] = true; break;
        case CHARGEABLE: item.extra_properties["chargeable"] = true; break;
        case NO_MOVE_ANIMATION: item.extra_properties["noMoveAnimation"] = true; break;
        case USABLE: item.extra_properties["usable"] = true; break;
        default:
            // Unknown flags may carry payloads; do not guess their length.
            reader.seek(reader.size() + 1);
            break;
        }
        if (flag == FORCE_USE || flag == CLOTH || flag == MARKET_ITEM ||
            flag == DEFAULT_ACTION || flag == WRAPPABLE || flag == UNWRAPPABLE ||
            flag == TOP_EFFECT || flag == CHARGEABLE || flag == NO_MOVE_ANIMATION ||
            flag == USABLE) {
            const size_t end = reader.tell();
            if (reader.good() && reader.seek(flagStart)) {
                const auto bytes = reader.readBytes(end - flagStart);
                item.extra_flags.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            }
        }
    }
}

void DatReader::readSpriteData(ClientItem &item, BinaryReader &reader, bool outfits)
{

    uint8_t groupCount = 1;
    const bool hasGroups = outfits && frameGroups();
    if (hasGroups) groupCount = std::max<uint8_t>(1, reader.readU8());

    item.frame_groups.clear();
    item.frame_groups.reserve(groupCount);
    for (uint8_t g = 0; g < groupCount; ++g) {
        ClientFrameGroup group;
        if (hasGroups) group.type = reader.readU8();

        group.width  = reader.readU8();
        group.height = reader.readU8();

        if (group.width > 1 || group.height > 1) {
            group.exact_size = reader.readU8();
        }

        group.layers    = reader.readU8();
        group.pattern_x = reader.readU8();
        group.pattern_y = reader.readU8();
        group.pattern_z = reader.readU8();
        group.frames    = reader.readU8();

        if (group.frames > 1 && frameDurations()) {
            const size_t animationStart = reader.tell();
            reader.readU8();
            reader.readU32();
            reader.readU8();
            for (uint32_t f = 0; f < group.frames; ++f) {
                reader.readU32();
                reader.readU32();
            }
            const size_t animationEnd = reader.tell();
            if (reader.good() && reader.seek(animationStart)) {
                const auto bytes = reader.readBytes(animationEnd - animationStart);
                group.animation_data = QByteArray(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            }
        }

        const uint64_t spriteCount = group.getTotalSprites();
        if (!group.width || !group.height || !group.layers || !group.pattern_x || !group.pattern_y || !group.pattern_z || !group.frames ||
            spriteCount > 1048576 || spriteCount > reader.remaining() / (extendedSprites() ? 4 : 2)) {
            reader.seek(reader.size() + 1);
            return;
        }
        group.sprite_ids.reserve(spriteCount);

        for (uint32_t i = 0; i < spriteCount; ++i) {
            const uint32_t sid = extendedSprites() ? reader.readU32() : reader.readU16();
            group.sprite_ids.push_back(sid);
        }
        if (g == 0) {
            item.width = group.width; item.height = group.height; item.exact_size = group.exact_size;
            item.layers = group.layers; item.pattern_x = group.pattern_x; item.pattern_y = group.pattern_y;
            item.pattern_z = group.pattern_z; item.frames = group.frames;
            item.sprite_ids = group.sprite_ids; item.animation_data = group.animation_data;
        }
        item.frame_groups.push_back(std::move(group));
    }
}

const ClientItem *DatReader::outfitByLookType(uint16_t lookType) const
{

    if (lookType == 0 || static_cast<size_t>(lookType) > m_outfits.size()) {
        return nullptr;
    }
    return &m_outfits[static_cast<size_t>(lookType) - 1];
}

QVariantMap DatReader::outfitPreview(int lookType) const
{
    QVariantMap out;
    const ClientItem *of = outfitByLookType(static_cast<uint16_t>(std::max(0, lookType)));
    if (!of || of->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, of->width);
    const int h = std::max<int>(1, of->height);
    const int patX = std::max<int>(1, of->pattern_x);
    const int layers = std::max<int>(1, of->layers);
    const int dir = std::min(2, patX - 1);

    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = ((dir * layers + 0) * h + hh) * w + ww;
            ids.push_back(idx >= 0 && idx < static_cast<int>(of->sprite_ids.size())
                              ? QVariant(of->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

QVariantMap DatReader::itemPreview(int clientId) const
{
    QVariantMap out;
    const ClientItem *ci = itemByClientId(static_cast<uint16_t>(std::max(0, clientId)));
    if (!ci || ci->sprite_ids.empty()) return out;

    const int w = std::max<int>(1, ci->width);
    const int h = std::max<int>(1, ci->height);
    QVariantList ids;
    for (int hh = 0; hh < h; ++hh)
        for (int ww = 0; ww < w; ++ww) {
            const int idx = hh * w + ww;
            ids.push_back(idx < static_cast<int>(ci->sprite_ids.size())
                              ? QVariant(ci->sprite_ids[static_cast<size_t>(idx)])
                              : QVariant(0u));
        }
    out.insert(QStringLiteral("ids"), ids);
    out.insert(QStringLiteral("width"), w);
    out.insert(QStringLiteral("height"), h);
    return out;
}

const ClientItem *DatReader::itemByClientId(uint16_t clientId) const
{
    if (clientId < 100 || clientId > m_maxItemId) {
        return nullptr;
    }

    const size_t idx = static_cast<size_t>(clientId - 100);
    if (idx >= m_items.size() || m_items[idx].id != clientId) {
        return nullptr;
    }

    return &m_items[idx];
}

const ClientItem *DatReader::effectById(int id) const
{
    if (id < 1 || static_cast<size_t>(id) > m_effects.size()) return nullptr;
    return &m_effects[static_cast<size_t>(id - 1)];
}

quint32 DatReader::previewSpriteIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].previewSpriteId();
}

int DatReader::itemIdAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return 0;
    return m_items[static_cast<size_t>(row)].id;
}

QVariantMap DatReader::detailsAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return {};

    const ClientItem &item = m_items[static_cast<size_t>(row)];
    const QStringList flags = collectDatFlags(item);

    QVariantMap details;
    details.insert(QStringLiteral("title"),          QStringLiteral("Item %1").arg(item.id));
    details.insert(QStringLiteral("category"),       QStringLiteral("Item"));
    details.insert(QStringLiteral("idLabel"),        QStringLiteral("Client ID"));
    details.insert(QStringLiteral("itemId"),         item.id);
    details.insert(QStringLiteral("clientId"),       item.id);
    details.insert(QStringLiteral("spriteIds"),      toVariantList(item.sprite_ids));
    details.insert(QStringLiteral("itemWidth"),      item.width);
    details.insert(QStringLiteral("itemHeight"),     item.height);
    details.insert(QStringLiteral("layers"),         item.layers);
    details.insert(QStringLiteral("patternX"),       item.pattern_x);
    details.insert(QStringLiteral("patternY"),       item.pattern_y);
    details.insert(QStringLiteral("patternZ"),       item.pattern_z);
    details.insert(QStringLiteral("frames"),         item.frames);
    details.insert(QStringLiteral("spriteCount"),    static_cast<int>(item.sprite_ids.size()));
    details.insert(QStringLiteral("previewSpriteId"),static_cast<quint32>(item.previewSpriteId()));
    details.insert(QStringLiteral("flags"),          flags);
    details.insert(QStringLiteral("flagsText"),      flags.isEmpty() ? QStringLiteral("-") : flags.join(QStringLiteral(", ")));
    details.insert(QStringLiteral("isGround"), item.is_ground);
    details.insert(QStringLiteral("groundSpeed"), item.ground_speed);
    details.insert(QStringLiteral("isGroundBorder"), item.is_ground_border);
    details.insert(QStringLiteral("isOnBottom"), item.is_on_bottom);
    details.insert(QStringLiteral("isContainer"), item.is_container);
    details.insert(QStringLiteral("isStackable"), item.is_stackable);
    details.insert(QStringLiteral("isUseable"), item.is_useable);
    details.insert(QStringLiteral("isWritable"), item.is_writable);
    details.insert(QStringLiteral("maxTextLength"), item.max_text_length);
    details.insert(QStringLiteral("isFluidContainer"), item.is_fluid_container);
    details.insert(QStringLiteral("isFluid"), item.is_fluid);
    details.insert(QStringLiteral("isUnpassable"), item.is_unpassable);
    details.insert(QStringLiteral("isUnmoveable"), item.is_unmoveable);
    details.insert(QStringLiteral("blocksMissiles"), item.blocks_missiles);
    details.insert(QStringLiteral("blocksPathfinder"), item.blocks_pathfinder);
    details.insert(QStringLiteral("isPickupable"), item.is_pickupable);
    details.insert(QStringLiteral("isHangable"), item.is_hangable);
    details.insert(QStringLiteral("isHorizontal"), item.is_horizontal);
    details.insert(QStringLiteral("isVertical"), item.is_vertical);
    details.insert(QStringLiteral("isRotatable"), item.is_rotatable);
    details.insert(QStringLiteral("hasLight"), item.has_light);
    details.insert(QStringLiteral("lightLevel"), item.light_level);
    details.insert(QStringLiteral("lightColor"), item.light_color);
    details.insert(QStringLiteral("dontHide"), item.dont_hide);
    details.insert(QStringLiteral("isTranslucent"), item.is_translucent);
    details.insert(QStringLiteral("hasOffset"), item.has_offset);
    details.insert(QStringLiteral("offsetX"), item.offset_x);
    details.insert(QStringLiteral("offsetY"), item.offset_y);
    details.insert(QStringLiteral("hasElevation"), item.has_elevation);
    details.insert(QStringLiteral("elevation"), item.elevation);
    details.insert(QStringLiteral("isLyingObject"), item.is_lying_object);
    details.insert(QStringLiteral("animateAlways"), item.animate_always);
    details.insert(QStringLiteral("hasMinimapColor"), item.has_minimap_color);
    details.insert(QStringLiteral("minimapColor"), item.minimap_color);
    details.insert(QStringLiteral("fullGround"), item.full_ground);
    details.insert(QStringLiteral("ignoreLook"), item.ignore_look);
    details.insert(QStringLiteral("floorChange"), item.floor_change);

    for(auto it=item.extra_properties.cbegin();it!=item.extra_properties.cend();++it) details.insert(it.key(),it.value());
    details.insert("isOnTop",item.is_on_top); details.insert("cropSize",item.exact_size); details.insert("writableOnce",item.writable_once); details.insert("lensHelp",item.lens_help);
    QVariantList rows;
    addRow(rows, QStringLiteral("Client ID"),    item.id);
    addRow(rows, QStringLiteral("Size"),         QStringLiteral("%1x%2").arg(item.width).arg(item.height));
    addRow(rows, QStringLiteral("Layers"),       item.layers);
    addRow(rows, QStringLiteral("Pattern X"),    item.pattern_x);
    addRow(rows, QStringLiteral("Pattern Y"),    item.pattern_y);
    addRow(rows, QStringLiteral("Pattern Z"),    item.pattern_z);
    addRow(rows, QStringLiteral("Frames"),       item.frames);
    addRow(rows, QStringLiteral("Sprites"),      static_cast<int>(item.sprite_ids.size()));
    addRow(rows, QStringLiteral("First sprite"), static_cast<quint32>(item.previewSpriteId()));

    if (item.is_ground)         addRow(rows, QStringLiteral("Ground speed"),  item.ground_speed);
    if (item.is_writable)       addRow(rows, QStringLiteral("Max text"),       item.max_text_length);
    if (item.has_light)         addRow(rows, QStringLiteral("Light"),          QStringLiteral("%1 / %2").arg(item.light_level).arg(item.light_color));
    if (item.has_offset)        addRow(rows, QStringLiteral("Offset"),         QStringLiteral("%1, %2").arg(item.offset_x).arg(item.offset_y));
    if (item.has_elevation)     addRow(rows, QStringLiteral("Elevation"),      item.elevation);
    if (item.has_minimap_color) addRow(rows, QStringLiteral("Minimap color"),  item.minimap_color);
    if (item.lens_help != 0)    addRow(rows, QStringLiteral("Lens help"),      item.lens_help);
    addRow(rows, QStringLiteral("Flags"), details.value(QStringLiteral("flagsText")));

    details.insert(QStringLiteral("rows"), rows);
    return details;
}

bool DatReader::setValue(int row, const QString &key, const QVariant &value)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    if ((QStringList{"isGroundBorder","isHangable","isHorizontal","isVertical","patternZ"}.contains(key) && m_clientVersion < 755) ||
        (QStringList{"dontHide","ignoreLook"}.contains(key) && m_clientVersion < 780) ||
        (key == "floorChange" && (m_clientVersion < 710 || m_clientVersion > 854)) ||
        (key == "isTranslucent" && m_clientVersion < 860)) return false;
    ClientItem &item = m_items[static_cast<size_t>(row)];
    const ClientItem original = item;
    bool known = true;
    if (key == QStringLiteral("isGround")) item.is_ground = value.toBool();
    else if (key == QStringLiteral("groundSpeed")) item.ground_speed = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("isGroundBorder")) item.is_ground_border = value.toBool();
    else if (key == QStringLiteral("isOnBottom")) item.is_on_bottom = value.toBool();
    else if (key == QStringLiteral("isContainer")) item.is_container = value.toBool();
    else if (key == QStringLiteral("isStackable")) item.is_stackable = value.toBool();
    else if (key == QStringLiteral("isUseable")) item.is_useable = value.toBool();
    else if (key == QStringLiteral("isWritable")) item.is_writable = value.toBool();
    else if (key == QStringLiteral("maxTextLength")) item.max_text_length = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("isFluidContainer")) item.is_fluid_container = value.toBool();
    else if (key == QStringLiteral("isFluid")) item.is_fluid = value.toBool();
    else if (key == QStringLiteral("isUnpassable")) item.is_unpassable = value.toBool();
    else if (key == QStringLiteral("isUnmoveable")) item.is_unmoveable = value.toBool();
    else if (key == QStringLiteral("blocksMissiles")) item.blocks_missiles = value.toBool();
    else if (key == QStringLiteral("blocksPathfinder")) item.blocks_pathfinder = value.toBool();
    else if (key == QStringLiteral("isPickupable")) item.is_pickupable = value.toBool();
    else if (key == QStringLiteral("isHangable")) item.is_hangable = value.toBool();
    else if (key == QStringLiteral("isHorizontal")) item.is_horizontal = value.toBool();
    else if (key == QStringLiteral("isVertical")) item.is_vertical = value.toBool();
    else if (key == QStringLiteral("isRotatable")) item.is_rotatable = value.toBool();
    else if (key == QStringLiteral("hasLight")) item.has_light = value.toBool();
    else if (key == QStringLiteral("lightLevel")) item.light_level = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("lightColor")) item.light_color = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("dontHide")) item.dont_hide = value.toBool();
    else if (key == QStringLiteral("isTranslucent")) item.is_translucent = value.toBool();
    else if (key == QStringLiteral("hasOffset")) item.has_offset = value.toBool();
    else if (key == QStringLiteral("offsetX")) item.offset_x = static_cast<int16_t>(qBound(-32768, value.toInt(), 32767));
    else if (key == QStringLiteral("offsetY")) item.offset_y = static_cast<int16_t>(qBound(-32768, value.toInt(), 32767));
    else if (key == QStringLiteral("hasElevation")) item.has_elevation = value.toBool();
    else if (key == QStringLiteral("elevation")) item.elevation = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("isLyingObject")) item.is_lying_object = value.toBool();
    else if (key == QStringLiteral("animateAlways")) item.animate_always = value.toBool();
    else if (key == QStringLiteral("hasMinimapColor")) item.has_minimap_color = value.toBool();
    else if (key == QStringLiteral("minimapColor")) item.minimap_color = static_cast<uint16_t>(qBound(0, value.toInt(), 65535));
    else if (key == QStringLiteral("fullGround")) item.full_ground = value.toBool();
    else if (key == QStringLiteral("ignoreLook")) item.ignore_look = value.toBool();
    else if (key == QStringLiteral("floorChange")) item.floor_change = value.toBool();
    else if (key == QStringLiteral("itemWidth")) item.width = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("itemHeight")) item.height = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("layers")) item.layers = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("patternX")) item.pattern_x = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("patternY")) item.pattern_y = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("patternZ")) item.pattern_z = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if (key == QStringLiteral("frames")) item.frames = static_cast<uint8_t>(qBound(1, value.toInt(), 255));
    else if(key=="isOnTop") item.is_on_top=value.toBool();
    else if(key=="cropSize") item.exact_size=static_cast<uint8_t>(qBound(1,value.toInt(),255));
    else if(key=="writableOnce") item.writable_once=value.toBool();
    else if(key=="lensHelp") item.lens_help=static_cast<uint16_t>(qBound(0,value.toInt(),65535));
    else if(QStringList{"forceUse","hasCloth","clothSlot","hasMarket","marketCategory","marketTradeAs","marketShowAs","marketName","marketVocation","marketLevel","hasAction","defaultAction","wrappable","unwrappable","topEffect","chargeable","noMoveAnimation","usable"}.contains(key)) {
        if((key=="noMoveAnimation" && m_clientVersion<1010) ||
           ((key=="hasMarket" || key.startsWith("market")) && m_clientVersion<940) ||
           ((key=="hasCloth" || key=="clothSlot") && m_clientVersion<900) ||
           ((key=="hasAction" || key=="defaultAction" || key=="usable") && m_clientVersion<1021) ||
           ((key=="wrappable" || key=="unwrappable" || key=="topEffect") && !((m_clientVersion>=710 && m_clientVersion<=792) || m_clientVersion>=1092)) ||
           (key=="chargeable" && (m_clientVersion<780 || m_clientVersion>854))) return false;
        if(key=="marketName") {
            const auto bytes=value.toString().toLatin1();
            if(bytes.size()>65535 || QString::fromLatin1(bytes)!=value.toString()) return false;
            item.extra_properties[key]=value.toString();
        } else if(QStringList{"clothSlot","marketCategory","marketTradeAs","marketShowAs","marketVocation","marketLevel","defaultAction"}.contains(key))
            item.extra_properties[key]=qBound(0,value.toInt(),65535);
        else item.extra_properties[key]=value.toBool();
        QByteArray encoded; QDataStream stream(&encoded,QIODevice::WriteOnly);stream.setByteOrder(QDataStream::LittleEndian);
        auto flag=[&](uint8_t f){stream<<quint8(encodeFlag(f));};
        auto number=[&](const char *k){stream<<quint16(item.extra_properties.value(k).toUInt());};
        auto enabled=[&](const char *k){return item.extra_properties.value(k).toBool();};
        if(enabled("forceUse"))flag(FORCE_USE);
        if(enabled("hasCloth")){flag(CLOTH);number("clothSlot");}
        if(enabled("hasMarket")){
            flag(MARKET_ITEM);number("marketCategory");number("marketTradeAs");number("marketShowAs");
            const auto name=item.extra_properties.value("marketName").toString().toLatin1();
            stream<<quint16(name.size());stream.writeRawData(name.constData(),name.size());number("marketVocation");number("marketLevel");
        }
        if(enabled("hasAction")){flag(DEFAULT_ACTION);number("defaultAction");}
        if(enabled("wrappable"))flag(WRAPPABLE);
        if(enabled("unwrappable"))flag(UNWRAPPABLE);
        if(enabled("topEffect"))flag(TOP_EFFECT);
        if(enabled("chargeable"))flag(CHARGEABLE);
        if(enabled("noMoveAnimation"))flag(NO_MOVE_ANIMATION);
        if(enabled("usable"))flag(USABLE);
        item.extra_flags=encoded;
    }
    else known = false;
    if (!known) return false;
    if (item.getTotalSprites() > 1048576) { item = original; return false; }
    item.modified = true;
    item.preserve_raw_flags = false;
    const size_t requiredSprites = static_cast<size_t>(item.getTotalSprites());
    if (item.sprite_ids.size() != requiredSprites)
        item.sprite_ids.resize(requiredSprites, 0);
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed);
    return true;
}

bool DatReader::setValues(int row, const QVariantMap &values)
{
    if(row<0 || row>=int(m_items.size()))return false;
    if(values.isEmpty())return true;
    const auto before=m_items[size_t(row)];const bool dirty=m_dirty;
    {
        QSignalBlocker blocker(this);
        for(auto it=values.cbegin();it!=values.cend();++it) {
            if(!setValue(row,it.key(),it.value())){m_items[size_t(row)]=before;m_dirty=dirty;return false;}
        }
        // Resizing intermediate dimensions must not discard surviving sprite slots.
        m_items[size_t(row)].sprite_ids=before.sprite_ids;
        m_items[size_t(row)].sprite_ids.resize(size_t(m_items[size_t(row)].getTotalSprites()),0);
    }
    if(!dirty)emit dirtyChanged();
    emit dataChanged(index(row),index(row));return true;
}

bool DatReader::setSpriteId(int row, int slot, quint32 spriteId)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    ClientItem &item = m_items[static_cast<size_t>(row)];
    if (slot < 0 || slot >= static_cast<int>(item.sprite_ids.size())) return false;
    if (item.sprite_ids[static_cast<size_t>(slot)] == spriteId) return true;
    item.sprite_ids[static_cast<size_t>(slot)] = spriteId;
    item.modified = true;
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {PreviewSpriteIdRole, SpriteIdsRole});
    return true;
}

int DatReader::createItem()
{
    if (!m_loaded || m_items.size() >= 65436) return -1;
    const int row = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), row, row);
    ClientItem item;
    item.id = static_cast<uint16_t>(100 + row);
    item.sprite_ids.push_back(0);
    item.modified = true;
    m_items.push_back(std::move(item));
    m_maxItemId = static_cast<uint16_t>(99 + m_items.size());
    endInsertRows();
    m_dirty = true;
    emit itemCountChanged(); emit dirtyChanged();
    return row;
}

int DatReader::duplicateItem(int row)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return -1;
    if (m_items.size() >= 65436) return -1;
    const int newRow = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newRow, newRow);
    ClientItem copy = m_items[static_cast<size_t>(row)];
    copy.id = static_cast<uint16_t>(100 + newRow);
    copy.modified = true;
    // Preserve original attributes and animation metadata in a duplicate.
    m_items.push_back(std::move(copy));
    m_maxItemId = static_cast<uint16_t>(99 + m_items.size());
    endInsertRows();
    m_dirty = true;
    emit itemCountChanged(); emit dirtyChanged();
    return newRow;
}

bool DatReader::removeItem(int row)
{
    if (row < 0 || row >= static_cast<int>(m_items.size())) return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_items.erase(m_items.begin() + row);
    for (size_t i = static_cast<size_t>(row); i < m_items.size(); ++i)
        m_items[i].id = static_cast<uint16_t>(100 + i);
    m_maxItemId = static_cast<uint16_t>(m_items.empty() ? 99 : 99 + m_items.size());
    endRemoveRows();
    if (row < static_cast<int>(m_items.size())) emit dataChanged(index(row), index(static_cast<int>(m_items.size()) - 1));
    m_dirty = true;
    emit itemCountChanged(); emit dirtyChanged();
    return true;
}

int DatReader::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant DatReader::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const ClientItem &item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case ItemIdRole:         return item.id;
    case PreviewSpriteIdRole: return item.previewSpriteId();
    case SpriteIdsRole:      return toVariantList(item.sprite_ids);
    case ItemWidthRole:      return item.width;
    case ItemHeightRole:     return item.height;
    case LayersRole:         return item.layers;
    case IsGroundRole:       return item.is_ground;
    case IsStackableRole:    return item.is_stackable;
    case IsContainerRole:    return item.is_container;
    case IsUnpassableRole:   return item.is_unpassable;
    default:                 return QVariant();
    }
}

QHash<int, QByteArray> DatReader::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ItemIdRole]          = "itemId";
    roles[PreviewSpriteIdRole] = "previewSpriteId";
    roles[SpriteIdsRole]       = "spriteIds";
    roles[ItemWidthRole]       = "itemWidth";
    roles[ItemHeightRole]      = "itemHeight";
    roles[LayersRole]          = "layers";
    roles[IsGroundRole]        = "isGround";
    roles[IsStackableRole]     = "isStackable";
    roles[IsContainerRole]     = "isContainer";
    roles[IsUnpassableRole]    = "isUnpassable";
    return roles;
}

const ClientItem *DatReader::objectAt(int category, int row) const {
    const auto &objects = category == 1 ? m_outfits : category == 2 ? m_effects : category == 3 ? m_missiles : m_items;
    return row >= 0 && row < int(objects.size()) ? &objects[size_t(row)] : nullptr;
}
int DatReader::categoryCount(int category) const {
    return int((category == 1 ? m_outfits : category == 2 ? m_effects : category == 3 ? m_missiles : m_items).size());
}
void DatReader::restoreItem(int row, const ClientItem &item) {
    if (row < 0 || row >= int(m_items.size())) return;
    m_items[size_t(row)] = item;
    m_dirty = true;
    emit dirtyChanged();
    emit dataChanged(index(row), index(row));
}

int DatReader::createObject(int category, int sourceRow)
{
    if (category == 0) return sourceRow < 0 ? createItem() : duplicateItem(sourceRow);
    if (!m_loaded || category < 1 || category > 3) return -1;
    auto &objects = category == 1 ? m_outfits : category == 2 ? m_effects : m_missiles;
    if (objects.size() >= 65535 || sourceRow >= int(objects.size())) return -1;
    ClientItem item;
    if (sourceRow >= 0) item = objects[size_t(sourceRow)];
    item.id = static_cast<uint16_t>(objects.size() + 1);
    if (item.sprite_ids.empty()) item.sprite_ids.push_back(0);
    item.modified = true;
    objects.push_back(std::move(item));
    auto &maxId = category == 1 ? m_maxOutfitId : category == 2 ? m_maxEffectId : m_maxMissileId;
    maxId = static_cast<uint16_t>(objects.size());
    m_categoryStructureChanged = true;
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
    return int(objects.size()) - 1;
}

bool DatReader::removeObject(int category, int row)
{
    if (category == 0) return removeItem(row);
    if (!m_loaded || category < 1 || category > 3) return false;
    auto &objects = category == 1 ? m_outfits : category == 2 ? m_effects : m_missiles;
    if (row < 0 || row >= int(objects.size())) return false;
    objects.erase(objects.begin() + row);
    for (size_t index = size_t(row); index < objects.size(); ++index) {
        objects[index].id = static_cast<uint16_t>(index + 1);
        objects[index].modified = true;
    }
    auto &maxId = category == 1 ? m_maxOutfitId : category == 2 ? m_maxEffectId : m_maxMissileId;
    maxId = static_cast<uint16_t>(objects.size());
    m_categoryStructureChanged = true;
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
    return true;
}
void DatReader::restoreObject(int category, int row, const ClientItem &item) {
    if (category == 0) { restoreItem(row, item); return; }
    auto *objects = category == 1 ? &m_outfits : category == 2 ? &m_effects : category == 3 ? &m_missiles : nullptr;
    if (!objects || row < 0 || row >= int(objects->size())) return;
    (*objects)[size_t(row)] = item;
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
}
bool DatReader::replaceItems(int firstRow, const std::vector<ClientItem> &items) {
    if(firstRow<0 || items.empty() || size_t(firstRow)+items.size()>m_items.size())return false;
    for(size_t index=0;index<items.size();++index)m_items[size_t(firstRow)+index]=items[index];
    if(!m_dirty){m_dirty=true;emit dirtyChanged();}
    emit dataChanged(this->index(firstRow),this->index(firstRow+int(items.size())-1));
    return true;
}
bool DatReader::ensureItemId(int clientId)
{
    if (!m_loaded || clientId < 100 || clientId > 65535) return false;
    const int currentMax = 99 + int(m_items.size());
    if (clientId <= currentMax) return true;
    const int first = int(m_items.size());
    const int count = clientId - currentMax;
    beginInsertRows(QModelIndex(), first, first + count - 1);
    m_items.reserve(size_t(first + count));
    for (int id = currentMax + 1; id <= clientId; ++id) {
        ClientItem item;
        item.id = static_cast<uint16_t>(id);
        item.sprite_ids.push_back(0);
        item.modified = true;
        m_items.push_back(std::move(item));
    }
    m_maxItemId = static_cast<uint16_t>(clientId);
    endInsertRows();
    if (!m_dirty) { m_dirty = true; emit dirtyChanged(); }
    emit itemCountChanged();
    return true;
}

void DatReader::adaptImportedItem(ClientItem &item) const
{
    item.modified = true;
    item.raw_record.clear();
    item.raw_flags.clear();
    if (m_clientVersion < 755) {
        item.is_ground_border = false; item.is_hangable = false;
        item.is_horizontal = false; item.is_vertical = false;
    }
    if (m_clientVersion < 780) { item.dont_hide = false; item.ignore_look = false; }
    if (m_clientVersion < 860) item.is_translucent = false;
    if (m_clientVersion < 710 || m_clientVersion > 854) item.floor_change = false;

    const auto properties = item.extra_properties;
    item.extra_properties.clear();
    item.extra_flags.clear();
    QDataStream out(&item.extra_flags, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    const auto addFlag = [&](uint8_t value) { out << quint8(encodeFlag(value)); };
    const auto enabled = [&](const char *key) { return properties.value(QLatin1String(key)).toBool(); };
    const auto number = [&](const char *key) { out << quint16(properties.value(QLatin1String(key)).toUInt()); };
    if (enabled("forceUse")) { addFlag(FORCE_USE); item.extra_properties.insert("forceUse", true); }
    if (m_clientVersion >= 900 && enabled("hasCloth")) {
        addFlag(CLOTH); number("clothSlot");
        item.extra_properties.insert("hasCloth", true);
        item.extra_properties.insert("clothSlot", properties.value("clothSlot"));
    }
    if (m_clientVersion >= 940 && enabled("hasMarket")) {
        addFlag(MARKET_ITEM); number("marketCategory"); number("marketTradeAs"); number("marketShowAs");
        const QByteArray name = properties.value("marketName").toString().toLatin1();
        out << quint16(name.size()); out.writeRawData(name.constData(), name.size());
        number("marketVocation"); number("marketLevel");
        for (const char *key : {"hasMarket", "marketCategory", "marketTradeAs", "marketShowAs", "marketName", "marketVocation", "marketLevel"})
            item.extra_properties.insert(QLatin1String(key), properties.value(QLatin1String(key)));
    }
    if (m_clientVersion >= 1021 && enabled("hasAction")) {
        addFlag(DEFAULT_ACTION); number("defaultAction");
        item.extra_properties.insert("hasAction", true);
        item.extra_properties.insert("defaultAction", properties.value("defaultAction"));
    }
    if ((m_clientVersion >= 710 && m_clientVersion <= 792) || m_clientVersion >= 1092) {
        for (const auto pair : {std::pair<const char *, uint8_t>{"wrappable", WRAPPABLE}, {"unwrappable", UNWRAPPABLE}, {"topEffect", TOP_EFFECT}})
            if (enabled(pair.first)) { addFlag(pair.second); item.extra_properties.insert(QLatin1String(pair.first), true); }
    }
    if (m_clientVersion >= 780 && m_clientVersion <= 854 && enabled("chargeable")) {
        addFlag(CHARGEABLE); item.extra_properties.insert("chargeable", true);
    }
    if (m_clientVersion >= 1010 && enabled("noMoveAnimation")) {
        addFlag(NO_MOVE_ANIMATION); item.extra_properties.insert("noMoveAnimation", true);
    }
    if (m_clientVersion >= 1021 && enabled("usable")) {
        addFlag(USABLE); item.extra_properties.insert("usable", true);
    }
}
namespace {
bool updateDuration(QByteArray &data, int frames, int frame, quint32 minimum, quint32 maximum) {
    if(frames<1 || frame<0 || frame>=frames || minimum<1 || minimum>maximum)return false;
    if(data.size()!=6+frames*8) {
        data=QByteArray(6+frames*8,'\0');
        for(int i=0;i<frames;++i){
            qToLittleEndian<quint32>(100,reinterpret_cast<uchar *>(data.data()+6+i*8));
            qToLittleEndian<quint32>(100,reinterpret_cast<uchar *>(data.data()+10+i*8));
        }
    }
    qToLittleEndian<quint32>(minimum,reinterpret_cast<uchar *>(data.data()+6+frame*8));
    qToLittleEndian<quint32>(maximum,reinterpret_cast<uchar *>(data.data()+10+frame*8));
    return true;
}
}
bool DatReader::setFrameDuration(int category,int row,int group,int frame,quint32 minimum,quint32 maximum) {
    if(!frameDurations() || category<0 || category>3)return false;
    auto &objects=category==1 ? m_outfits : category==2 ? m_effects : category==3 ? m_missiles : m_items;
    if(row<0 || row>=int(objects.size()))return false;
    auto &item=objects[size_t(row)];
    if(category==1 && !item.frame_groups.empty()) {
        if(group<0 || group>=int(item.frame_groups.size()))return false;
        auto &selected=item.frame_groups[size_t(group)];
        if(!updateDuration(selected.animation_data,selected.frames,frame,minimum,maximum))return false;
        if(group==0)item.animation_data=selected.animation_data;
    } else {
        if(group!=0 || !updateDuration(item.animation_data,item.frames,frame,minimum,maximum))return false;
    }
    item.modified=true;
    if(!m_dirty){m_dirty=true;emit dirtyChanged();}
    if(category==0)emit dataChanged(index(row),index(row));
    return true;
}
bool DatReader::duplicateFrame(int category,int row,int group,int frame) {
    if(category<0 || category>3)return false;
    auto &objects=category==1 ? m_outfits : category==2 ? m_effects : category==3 ? m_missiles : m_items;
    if(row<0 || row>=int(objects.size()))return false;
    auto &item=objects[size_t(row)];
    ClientFrameGroup *selected=category==1 && group>=0 && group<int(item.frame_groups.size()) ? &item.frame_groups[size_t(group)] : nullptr;
    if(category==1 && !selected)return false;
    int frames=selected?selected->frames:item.frames;
    if(frame<0 || frame>=frames || frames>=255)return false;
    auto &ids=selected?selected->sprite_ids:item.sprite_ids;
    auto &animation=selected?selected->animation_data:item.animation_data;
    const int stride=(selected?selected->width:item.width)*(selected?selected->height:item.height)
                    *(selected?selected->layers:item.layers)*(selected?selected->pattern_x:item.pattern_x)
                    *(selected?selected->pattern_y:item.pattern_y)*(selected?selected->pattern_z:item.pattern_z);
    if(stride<1 || ids.size()!=size_t(frames*stride))return false;
    const auto first=ids.begin()+frame*stride;
    std::vector<uint32_t> copy(first,first+stride);
    ids.insert(ids.begin()+(frame+1)*stride,copy.begin(),copy.end());
    if(frameDurations()){
        updateDuration(animation,frames,frame,100,100);
        animation.insert(6+(frame+1)*8,animation.mid(6+frame*8,8));
    }
    if(selected){++selected->frames;if(group==0){item.frames=selected->frames;item.sprite_ids=selected->sprite_ids;item.animation_data=selected->animation_data;}}
    else ++item.frames;
    item.modified=true;m_dirty=true;emit dirtyChanged();
    if(category==0)emit dataChanged(index(row),index(row));
    return true;
}
bool DatReader::deleteFrame(int category,int row,int group,int frame) {
    if(category<0 || category>3)return false;
    auto &objects=category==1 ? m_outfits : category==2 ? m_effects : category==3 ? m_missiles : m_items;
    if(row<0 || row>=int(objects.size()))return false;
    auto &item=objects[size_t(row)];
    ClientFrameGroup *selected=category==1 && group>=0 && group<int(item.frame_groups.size()) ? &item.frame_groups[size_t(group)] : nullptr;
    if(category==1 && !selected)return false;
    const int frames=selected?selected->frames:item.frames;
    if(frame<0 || frame>=frames || frames<=1)return false;
    auto &ids=selected?selected->sprite_ids:item.sprite_ids;
    auto &animation=selected?selected->animation_data:item.animation_data;
    const int stride=(selected?selected->width:item.width)*(selected?selected->height:item.height)
                    *(selected?selected->layers:item.layers)*(selected?selected->pattern_x:item.pattern_x)
                    *(selected?selected->pattern_y:item.pattern_y)*(selected?selected->pattern_z:item.pattern_z);
    if(stride<1 || ids.size()!=size_t(frames*stride))return false;
    ids.erase(ids.begin()+frame*stride,ids.begin()+(frame+1)*stride);
    if(animation.size()==6+frames*8)animation.remove(6+frame*8,8);
    if(frames==2)animation.clear();
    if(selected){--selected->frames;if(group==0){item.frames=selected->frames;item.sprite_ids=selected->sprite_ids;item.animation_data=selected->animation_data;}}
    else --item.frames;
    item.modified=true;m_dirty=true;emit dirtyChanged();
    if(category==0)emit dataChanged(index(row),index(row));
    return true;
}
int DatReader::optimizeFrameDurations(bool items,bool outfits,bool effects,quint32 minimum,quint32 maximum) {
    if(!frameDurations() || minimum<1 || minimum>maximum)return 0;
    int changed=0;
    for(int category=0;category<3;++category){
        if(!(category==0?items:category==1?outfits:effects))continue;
        auto &objects=category==0 ? m_items : category==1 ? m_outfits : m_effects;
        for(int row=0;row<int(objects.size());++row){
            auto &item=objects[size_t(row)];
            const int groups=category==1 && !item.frame_groups.empty()?int(item.frame_groups.size()):1;
            for(int group=0;group<groups;++group){
                const int frames=category==1 && !item.frame_groups.empty()?item.frame_groups[size_t(group)].frames:item.frames;
                for(int frame=0;frame<frames;++frame)
                    if(setFrameDuration(category,row,group,frame,minimum,maximum))++changed;
            }
        }
    }
    return changed;
}
int DatReader::convertFrameDurations(bool enabled,quint32 minimum,quint32 maximum) {
    if(!m_loaded || minimum<1 || minimum>maximum)return -1;
    int changed=0;
    const auto convert=[&](auto &objects,bool outfits){
        for(auto &item:objects){
            bool animated=false;
            if(outfits && !item.frame_groups.empty()){
                for(auto &group:item.frame_groups){
                    if(group.frames<=1)continue;
                    animated=true;
                    if(enabled){
                        group.animation_data.clear();
                        updateDuration(group.animation_data,group.frames,0,minimum,maximum);
                        for(int frame=1;frame<group.frames;++frame)
                            updateDuration(group.animation_data,group.frames,frame,minimum,maximum);
                    }else group.animation_data.clear();
                }
                item.animation_data=item.frame_groups.front().animation_data;
            }else if(item.frames>1){
                animated=true;
                if(enabled){
                    item.animation_data.clear();
                    updateDuration(item.animation_data,item.frames,0,minimum,maximum);
                    for(int frame=1;frame<item.frames;++frame)
                        updateDuration(item.animation_data,item.frames,frame,minimum,maximum);
                }else item.animation_data.clear();
            }
            if(animated){
                if(!item.modified && !item.raw_flags.isEmpty())item.preserve_raw_flags=true;
                item.modified=true;
                ++changed;
            }
        }
    };
    convert(m_items,false);
    convert(m_outfits,true);
    convert(m_effects,false);
    convert(m_missiles,false);
    if(changed){m_dirty=true;emit dirtyChanged();}
    return changed;
}
int DatReader::convertFrameGroups(bool enabled,bool removeMounts) {
    int changed=0;
    for(auto &outfit:m_outfits){
        if(outfit.frame_groups.empty())continue;
        if(!outfit.animate_always && enabled && outfit.frame_groups.size()==1){
            const ClientFrameGroup normal=outfit.frame_groups.front();
            if(normal.frames>=3 && normal.sprite_ids.size()==normal.getTotalSprites()){
                const size_t spritesPerFrame=normal.getTotalSprites()/normal.frames;
                ClientFrameGroup idle=normal;
                idle.type=0;
                idle.frames=1;
                idle.sprite_ids.assign(normal.sprite_ids.begin(),normal.sprite_ids.begin()+spritesPerFrame);
                idle.animation_data.clear();
                ClientFrameGroup walking=normal;
                walking.type=1;
                walking.frames=normal.frames-1;
                walking.sprite_ids.assign(normal.sprite_ids.begin()+spritesPerFrame,normal.sprite_ids.end());
                if(normal.animation_data.size()==6+normal.frames*8){
                    walking.animation_data=normal.animation_data.left(6);
                    walking.animation_data+=normal.animation_data.mid(14,walking.frames*8);
                }else walking.animation_data.clear();
                outfit.frame_groups={std::move(idle),std::move(walking)};
            }
        } else if(!outfit.animate_always && !enabled && outfit.frame_groups.size()>1){
            const ClientFrameGroup &idle=outfit.frame_groups.front();
            const ClientFrameGroup &walking=outfit.frame_groups[1];
            ClientFrameGroup normal=idle;
            normal.type=0;
            normal.frames=3;
            if(removeMounts)normal.pattern_z=1;
            const size_t targetPerFrame=normal.getTotalSprites()/normal.frames;
            normal.sprite_ids.clear();
            normal.sprite_ids.reserve(targetPerFrame*3);
            const auto appendFrame=[&](const ClientFrameGroup &group,int frame){
                const size_t sourcePerFrame=group.getTotalSprites()/group.frames;
                const size_t sourceOffset=sourcePerFrame*size_t(std::min(frame,int(group.frames)-1));
                for(size_t index=0;index<targetPerFrame;++index){
                    const size_t sourceIndex=sourceOffset+index;
                    normal.sprite_ids.push_back(index<sourcePerFrame && sourceIndex<group.sprite_ids.size()
                                                ? group.sprite_ids[sourceIndex] : 0);
                }
            };
            appendFrame(idle,0);
            appendFrame(walking,0);
            appendFrame(walking,walking.frames>4 ? 4 : walking.frames>1 ? 1 : 0);
            normal.animation_data.clear();
            outfit.frame_groups={std::move(normal)};
        }
        outfit.frame_groups.front().type=0;
        const auto &primary=outfit.frame_groups.front();
        outfit.width=primary.width;outfit.height=primary.height;outfit.exact_size=primary.exact_size;
        outfit.layers=primary.layers;outfit.pattern_x=primary.pattern_x;outfit.pattern_y=primary.pattern_y;
        outfit.pattern_z=primary.pattern_z;outfit.frames=primary.frames;
        outfit.sprite_ids=primary.sprite_ids;outfit.animation_data=primary.animation_data;
        outfit.modified=true;
        ++changed;
    }
    if(changed && !m_dirty){m_dirty=true;emit dirtyChanged();}
    return changed;
}
QSet<quint32> DatReader::usedSpriteIds() const {
    QSet<quint32> used;
    const auto collect=[&](const auto &objects,bool useGroups){
        for(const auto &item:objects){
            if(!useGroups || item.frame_groups.empty()){
                for(auto id:item.sprite_ids)if(id)used.insert(id);
            } else {
                for(const auto &group:item.frame_groups)
                    for(auto id:group.sprite_ids)if(id)used.insert(id);
            }
        }
    };
    collect(m_items,false);collect(m_outfits,true);collect(m_effects,false);collect(m_missiles,false);
    return used;
}
int DatReader::remapSpriteIds(const QHash<quint32,quint32> &mapping) {
    int changed=0;
    const auto remap=[&](auto &objects){
        for(auto &item:objects){
            bool touched=false;
            for(auto &id:item.sprite_ids)if(mapping.contains(id) && mapping.value(id)!=id){id=mapping.value(id);touched=true;}
            for(auto &group:item.frame_groups)
                for(auto &id:group.sprite_ids)if(mapping.contains(id) && mapping.value(id)!=id){id=mapping.value(id);touched=true;}
            if(touched){item.modified=true;++changed;}
        }
    };
    remap(m_items);remap(m_outfits);remap(m_effects);remap(m_missiles);
    if(changed){m_dirty=true;emit dirtyChanged();}
    return changed;
}
