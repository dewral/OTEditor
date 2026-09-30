#include "obdcodec.h"

#include <QBuffer>
#include <QDataStream>
#include <QtEndian>
#include <lzma.h>
#include <array>

namespace {
constexpr int kSpriteBytes = 32 * 32 * 4;
constexpr qsizetype kMaximumDecodedBytes = 128 * 1024 * 1024;

QByteArray lzmaData(const QByteArray &input, bool compress, QString *error)
{
    lzma_stream stream = LZMA_STREAM_INIT;
    lzma_ret status;
    if (compress) {
        lzma_options_lzma options;
        status = lzma_lzma_preset(&options, 6) ? LZMA_OPTIONS_ERROR : lzma_alone_encoder(&stream, &options);
    } else {
        status = lzma_alone_decoder(&stream, 256u * 1024u * 1024u);
    }
    if (status != LZMA_OK) {
        if (error) *error = QStringLiteral("Could not initialize LZMA codec (%1)").arg(status);
        return {};
    }
    stream.next_in = reinterpret_cast<const uint8_t *>(input.constData());
    stream.avail_in = size_t(input.size());
    QByteArray result;
    char output[16384];
    do {
        stream.next_out = reinterpret_cast<uint8_t *>(output);
        stream.avail_out = sizeof(output);
        status = lzma_code(&stream, compress ? LZMA_FINISH : LZMA_RUN);
        result.append(output, qsizetype(sizeof(output) - stream.avail_out));
        if (result.size() > kMaximumDecodedBytes) {
            status = LZMA_MEMLIMIT_ERROR;
            break;
        }
    } while (status == LZMA_OK);
    lzma_end(&stream);
    if (status != LZMA_STREAM_END) {
        if (error) *error = QStringLiteral("Invalid or oversized OBD LZMA data (%1)").arg(status);
        return {};
    }
    return result;
}

QByteArray spritePixels(const QImage &source)
{
    const QImage image=source.convertToFormat(QImage::Format_RGBA8888);
    QByteArray pixels(kSpriteBytes,'\0');
    if (image.size()!=QSize(32,32)) return pixels;
    for (int y=0;y<32;++y) {
        const uchar *line=image.constScanLine(y);
        for (int x=0;x<32;++x) {
            const int index=(y*32+x)*4;
            pixels[index]=char(line[x*4+3]);
            pixels[index+1]=char(line[x*4]);
            pixels[index+2]=char(line[x*4+1]);
            pixels[index+3]=char(line[x*4+2]);
        }
    }
    return pixels;
}

QImage imageFromPixels(const QByteArray &pixels)
{
    QImage image(32,32,QImage::Format_RGBA8888);
    for (int y=0;y<32;++y) {
        uchar *line=image.scanLine(y);
        for (int x=0;x<32;++x) {
            const int index=(y*32+x)*4;
            line[x*4]=uchar(pixels[index+1]);
            line[x*4+1]=uchar(pixels[index+2]);
            line[x*4+2]=uchar(pixels[index+3]);
            line[x*4+3]=uchar(pixels[index]);
        }
    }
    return image;
}

void writeFlags(QDataStream &out,const ClientItem &item)
{
    auto flag=[&](quint8 value){out<<value;};
    if (item.is_ground) {flag(0);out<<quint16(item.ground_speed);}
    else if (item.is_ground_border) flag(1);
    else if (item.is_on_bottom) flag(2);
    else if (item.is_on_top) flag(3);
    if (item.is_container) flag(4);
    if (item.is_stackable) flag(5);
    if (item.extra_properties.value("forceUse").toBool()) flag(6);
    if (item.is_useable) flag(7);
    if (item.is_writable) {flag(item.writable_once?9:8);out<<quint16(item.max_text_length);}
    if (item.is_fluid_container) flag(10);
    if (item.is_fluid) flag(11);
    if (item.is_unpassable) flag(12);
    if (item.is_unmoveable) flag(13);
    if (item.blocks_missiles) flag(14);
    if (item.blocks_pathfinder) flag(15);
    if (item.extra_properties.value("noMoveAnimation").toBool()) flag(16);
    if (item.is_pickupable) flag(17);
    if (item.is_hangable) flag(18);
    if (item.is_vertical) flag(19);
    if (item.is_horizontal) flag(20);
    if (item.is_rotatable) flag(21);
    if (item.has_light) {flag(22);out<<quint16(item.light_level)<<quint16(item.light_color);}
    if (item.dont_hide) flag(23);
    if (item.is_translucent) flag(24);
    if (item.has_offset) {flag(25);out<<quint16(item.offset_x)<<quint16(item.offset_y);}
    if (item.has_elevation) {flag(26);out<<quint16(item.elevation);}
    if (item.is_lying_object) flag(27);
    if (item.animate_always) flag(28);
    if (item.has_minimap_color) {flag(29);out<<quint16(item.minimap_color);}
    if (item.lens_help) {flag(30);out<<quint16(item.lens_help);}
    if (item.full_ground) flag(31);
    if (item.ignore_look) flag(32);
    if (item.extra_properties.value("hasCloth").toBool()) {flag(33);out<<quint16(item.extra_properties.value("clothSlot").toUInt());}
    if (item.extra_properties.value("hasMarket").toBool()) {
        flag(34);
        out<<quint16(item.extra_properties.value("marketCategory").toUInt())
           <<quint16(item.extra_properties.value("marketTradeAs").toUInt())
           <<quint16(item.extra_properties.value("marketShowAs").toUInt());
        const QByteArray name=item.extra_properties.value("marketName").toString().toLatin1();
        out<<quint16(qMin(name.size(),65535));out.writeRawData(name.constData(),qMin(name.size(),65535));
        out<<quint16(item.extra_properties.value("marketVocation").toUInt())
           <<quint16(item.extra_properties.value("marketLevel").toUInt());
    }
    if (item.extra_properties.value("hasAction").toBool()) {flag(35);out<<quint16(item.extra_properties.value("defaultAction").toUInt());}
    if (item.extra_properties.value("wrappable").toBool()) flag(36);
    if (item.extra_properties.value("unwrappable").toBool()) flag(37);
    if (item.extra_properties.value("topEffect").toBool()) flag(38);
    if (item.extra_properties.value("chargeable").toBool()) flag(0xfc);
    if (item.floor_change) flag(0xfd);
    if (item.extra_properties.value("usable").toBool()) flag(0xfe);
    flag(0xff);
}

bool readFlags(QDataStream &in,ClientItem &item,qint64 textureOffset)
{
    for (int count=0;count<256 && in.device()->pos()<textureOffset;++count) {
        quint8 flag=0;in>>flag;
        switch (flag) {
        case 0: item.is_ground=true;in>>item.ground_speed;break;
        case 1:item.is_ground_border=true;break;
        case 2:item.is_on_bottom=true;break;
        case 3:item.is_on_top=true;break;
        case 4:item.is_container=true;break;
        case 5:item.is_stackable=true;break;
        case 6:item.extra_properties["forceUse"]=true;break;
        case 7:item.is_useable=true;break;
        case 8:case 9:item.is_writable=true;item.writable_once=flag==9;in>>item.max_text_length;break;
        case 10:item.is_fluid_container=true;break;
        case 11:item.is_fluid=true;break;
        case 12:item.is_unpassable=true;break;
        case 13:item.is_unmoveable=true;break;
        case 14:item.blocks_missiles=true;break;
        case 15:item.blocks_pathfinder=true;break;
        case 16:item.extra_properties["noMoveAnimation"]=true;break;
        case 17:item.is_pickupable=true;break;
        case 18:item.is_hangable=true;break;
        case 19:item.is_vertical=true;break;
        case 20:item.is_horizontal=true;break;
        case 21:item.is_rotatable=true;break;
        case 22:item.has_light=true;in>>item.light_level>>item.light_color;break;
        case 23:item.dont_hide=true;break;
        case 24:item.is_translucent=true;break;
        case 25:{quint16 x=0,y=0;item.has_offset=true;in>>x>>y;item.offset_x=int16_t(x);item.offset_y=int16_t(y);break;}
        case 26:item.has_elevation=true;in>>item.elevation;break;
        case 27:item.is_lying_object=true;break;
        case 28:item.animate_always=true;break;
        case 29:item.has_minimap_color=true;in>>item.minimap_color;break;
        case 30:in>>item.lens_help;break;
        case 31:item.full_ground=true;break;
        case 32:item.ignore_look=true;break;
        case 33:{quint16 value=0;in>>value;item.extra_properties["hasCloth"]=true;item.extra_properties["clothSlot"]=value;break;}
        case 34:{
            quint16 category=0,trade=0,show=0,length=0,vocation=0,level=0;
            in>>category>>trade>>show>>length;
            if (in.status()!=QDataStream::Ok || in.device()->pos()+length+4>textureOffset) return false;
            QByteArray name(length,'\0');in.readRawData(name.data(),length);in>>vocation>>level;
            item.extra_properties["hasMarket"]=true;
            item.extra_properties["marketCategory"]=category;
            item.extra_properties["marketTradeAs"]=trade;
            item.extra_properties["marketShowAs"]=show;
            item.extra_properties["marketName"]=QString::fromLatin1(name);
            item.extra_properties["marketVocation"]=vocation;
            item.extra_properties["marketLevel"]=level;
            break;
        }
        case 35:{quint16 value=0;in>>value;item.extra_properties["hasAction"]=true;item.extra_properties["defaultAction"]=value;break;}
        case 36:item.extra_properties["wrappable"]=true;break;
        case 37:item.extra_properties["unwrappable"]=true;break;
        case 38:item.extra_properties["topEffect"]=true;break;
        case 39:{
            item.has_bones=true;
            for (int direction=0;direction<4;++direction) {
                quint16 x=0,y=0;in>>x>>y;
                item.bone_offset_x[size_t(direction)]=int16_t(x);
                item.bone_offset_y[size_t(direction)]=int16_t(y);
            }
            break;
        }
        case 0xfc:item.extra_properties["chargeable"]=true;break;
        case 0xfd:item.floor_change=true;break;
        case 0xfe:item.extra_properties["usable"]=true;break;
        case 0xff:return in.status()==QDataStream::Ok && in.device()->pos()==textureOffset;
        default:return false;
        }
        if (in.status()!=QDataStream::Ok || in.device()->pos()>textureOffset) return false;
    }
    return false;
}

int legacyFlag(int version,quint8 raw)
{
    if (raw==0xff) return 0xff;
    if (raw>=0x24 && raw<=0x26 && (version<=750 || (version>=780 && version<=854 && raw<=0x25) || version>=987)) return int(raw);
    if (raw==0x27 && version>=780) return 39;
    if (raw==0xfe && version>=987) return 0xfe;
    static constexpr std::array<int,33> first={
        0,2,3,4,5,7,6,8,9,10,11,12,13,14,15,17,22,0xfd,31,26,25,-1,29,21,27,28,30
    };
    static constexpr std::array<int,33> second={
        0,2,3,4,5,7,6,8,9,10,11,12,13,14,15,17,22,0xfd,31,26,25,-1,29,21,27,18,19,20,28,30
    };
    static constexpr std::array<int,33> third={
        0,1,2,3,4,5,7,6,8,9,10,11,12,13,14,15,17,18,19,20,21,22,-1,0xfd,25,26,27,28,29,30,31
    };
    static constexpr std::array<int,33> fourth={
        0,1,2,3,4,5,6,7,0xfc,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,0xfd,25,26,27,28,29,30,31,32
    };
    static constexpr std::array<int,35> fifth={
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34
    };
    if (version<=730) return raw<=26?first[raw]:-1;
    if (version<=750) return raw<=29?second[raw]:-1;
    if (version<=772) return raw<=30?third[raw]:-1;
    if (version<=854) return raw<=32?fourth[raw]:-1;
    if (version<=986) return raw<=33?fifth[raw]:-1;
    return raw<=38?raw:-1;
}

bool readLegacyFlags(QDataStream &input,ClientItem &item,int version)
{
    QByteArray normalized;
    QDataStream output(&normalized,QIODevice::WriteOnly);
    output.setByteOrder(QDataStream::LittleEndian);
    for (int count=0;count<256;++count) {
        quint8 raw=0;input>>raw;
        if (input.status()!=QDataStream::Ok) return false;
        const int flag=legacyFlag(version,raw);
        if (flag<0) return false;
        output<<quint8(flag);
        if (flag==0xff) {
            QBuffer buffer(&normalized);buffer.open(QIODevice::ReadOnly);
            QDataStream decoded(&buffer);decoded.setByteOrder(QDataStream::LittleEndian);
            return readFlags(decoded,item,normalized.size());
        }
        const int fixedBytes=(flag==0 || flag==8 || flag==9 || flag==26 || flag==29 || flag==30 || flag==33 || flag==35)
            ? 2 : (flag==22 || flag==25 ? 4 : flag==39 ? 16 : 0);
        if (flag==25 && version<=750) {
            output<<quint16(8)<<quint16(8);
            continue;
        }
        int payloadBytes=fixedBytes;
        if (flag==34) {
            const QByteArray prefix=input.device()->read(8);
            if (prefix.size()!=8) return false;
            output.writeRawData(prefix.constData(),prefix.size());
            const quint16 nameLength=qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(prefix.constData()+6));
            payloadBytes=int(nameLength)+4;
        }
        if (payloadBytes) {
            const QByteArray payload=input.device()->read(payloadBytes);
            if (payload.size()!=payloadBytes) return false;
            output.writeRawData(payload.constData(),payload.size());
        }
    }
    return false;
}

ClientFrameGroup defaultGroup(const ClientItem &item)
{
    ClientFrameGroup group;
    group.width=item.width;group.height=item.height;group.exact_size=item.exact_size;
    group.layers=item.layers;group.pattern_x=item.pattern_x;group.pattern_y=item.pattern_y;
    group.pattern_z=item.pattern_z;group.frames=item.frames;
    group.sprite_ids=item.sprite_ids;group.animation_data=item.animation_data;
    return group;
}

bool writeGroup(QDataStream &out,int category,const ClientFrameGroup &group,SprReader &sprites,QString *error)
{
    const qint64 total=qint64(group.getTotalSprites());
    if (total<1 || total>32768 || group.sprite_ids.size()!=size_t(total)) {
        if(error)*error="Invalid OBD sprite layout";return false;
    }
    if (category==1) out<<quint8(group.type);
    out<<quint8(group.width)<<quint8(group.height);
    if (group.width>1 || group.height>1) out<<quint8(group.exact_size);
    out<<quint8(group.layers)<<quint8(group.pattern_x)<<quint8(group.pattern_y)
       <<quint8(group.pattern_z)<<quint8(group.frames);
    if (group.frames>1) {
        if (group.animation_data.size()==6+group.frames*8)
            out.writeRawData(group.animation_data.constData(),group.animation_data.size());
        else {
            out<<quint8(0)<<quint32(0)<<quint8(0);
            for (int frame=0;frame<group.frames;++frame)out<<quint32(100)<<quint32(100);
        }
    }
    for (quint32 id:group.sprite_ids) {
        if (id>quint32(sprites.spriteCount())) {if(error)*error="Object references a missing sprite";return false;}
        const QImage image=id?sprites.spriteImage(int(id)):QImage();
        if (id && image.size()!=QSize(32,32)) {if(error)*error="Object references an unreadable sprite";return false;}
        const QByteArray pixels=id?spritePixels(image):QByteArray(kSpriteBytes,'\0');
        out<<id<<quint32(pixels.size());
        out.writeRawData(pixels.constData(),pixels.size());
    }
    return out.status()==QDataStream::Ok;
}

bool readGroup(QDataStream &in,int category,int version,ClientFrameGroup &group,QVector<QImage> &sprites,QString *error)
{
    if (category==1 && version==300) in>>group.type;
    else if (category==1) group.type=1;
    in>>group.width>>group.height;
    if (group.width>1 || group.height>1) in>>group.exact_size;
    in>>group.layers>>group.pattern_x>>group.pattern_y>>group.pattern_z>>group.frames;
    const quint64 total=group.getTotalSprites();
    if (in.status()!=QDataStream::Ok || total<1 || total>32768 || group.frames<1) {
        if(error)*error="Invalid OBD sprite dimensions";return false;
    }
    if (group.frames>1 && version>=200) {
        group.animation_data.resize(6+group.frames*8);
        if (in.readRawData(group.animation_data.data(),group.animation_data.size())!=group.animation_data.size()) return false;
    }
    group.sprite_ids.reserve(size_t(total));
    for (quint64 slot=0;slot<total;++slot) {
        quint32 id=0,length=kSpriteBytes;
        in>>id;
        if (version!=200) in>>length;
        if (in.status()!=QDataStream::Ok || length>kSpriteBytes || in.device()->bytesAvailable()<length) {
            if(error)*error="Invalid OBD sprite data";return false;
        }
        QByteArray pixels(kSpriteBytes,'\0');
        if (in.readRawData(pixels.data(),int(length))!=int(length)) return false;
        group.sprite_ids.push_back(id);
        sprites.append(imageFromPixels(pixels));
    }
    return true;
}
}

QByteArray ObdCodec::encode(int clientVersion,int category,const ClientItem &item,SprReader &sprites,QString *error)
{
    if (category<0 || category>3 || sprites.spriteSize()!=32 || clientVersion<0 || clientVersion>65535) {
        if(error)*error="OBD v3 requires 32x32 sprites and a valid client version";return {};
    }
    QByteArray plain;
    QBuffer buffer(&plain);buffer.open(QIODevice::ReadWrite);
    QDataStream out(&buffer);out.setByteOrder(QDataStream::LittleEndian);
    out<<quint16(300)<<quint16(clientVersion)<<quint8(category+1)<<quint32(0);
    writeFlags(out,item);
    const quint32 textureOffset=quint32(buffer.pos());
    buffer.seek(5);out<<textureOffset;buffer.seek(plain.size());
    if (category==1) {
        const int count=qMax(1,int(item.frame_groups.size()));
        if (count>255) {if(error)*error="Too many outfit frame groups";return {};}
        out<<quint8(count);
        if (item.frame_groups.empty()) {
            ClientFrameGroup group=defaultGroup(item);group.type=1;
            if (!writeGroup(out,category,group,sprites,error)) return {};
        } else {
            for (const auto &group:item.frame_groups)
                if (!writeGroup(out,category,group,sprites,error)) return {};
        }
    } else if (!writeGroup(out,category,defaultGroup(item),sprites,error)) return {};
    if (out.status()!=QDataStream::Ok) {if(error)*error="Could not encode OBD object";return {};}
    return lzmaData(plain,true,error);
}

bool ObdCodec::decode(const QByteArray &fileData,ObdObject &object,QString *error)
{
    const QByteArray plain=lzmaData(fileData,false,error);
    if (plain.isEmpty()) return false;
    QBuffer buffer;buffer.setData(plain);buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);in.setByteOrder(QDataStream::LittleEndian);
    quint16 obdVersion=0,clientVersion=0;quint8 categoryValue=0;quint32 textureOffset=0;
    in>>obdVersion;
    object={};
    if (obdVersion>=710 && obdVersion<=1310) {
        clientVersion=obdVersion;
        obdVersion=100;
        quint16 nameLength=0;in>>nameLength;
        if (nameLength>7 || nameLength<4) nameLength=qbswap(nameLength);
        if (nameLength<4 || nameLength>7 || in.device()->bytesAvailable()<nameLength) {
            if(error)*error="Invalid OBD v1 category";return false;
        }
        QByteArray category(nameLength,'\0');
        if (in.readRawData(category.data(),nameLength)!=nameLength) return false;
        const int parsed=QStringList{"item","outfit","effect","missile"}.indexOf(QString::fromLatin1(category));
        if (parsed<0) {if(error)*error="Invalid OBD v1 category";return false;}
        object.category=parsed;
        if (!readLegacyFlags(in,object.item,clientVersion)) {
            if(error)*error="Invalid OBD v1 object properties";return false;
        }
    } else {
        in>>clientVersion>>categoryValue>>textureOffset;
        if ((obdVersion!=200 && obdVersion!=300) || categoryValue<1 || categoryValue>4 || textureOffset<9 || textureOffset>=quint32(plain.size())) {
            if(error)*error="Unsupported or invalid OBD file";return false;
        }
        object.category=int(categoryValue)-1;
        if (!readFlags(in,object.item,textureOffset)) {if(error)*error="Invalid OBD object properties";return false;}
    }
    object.clientVersion=clientVersion;
    int groupCount=1;
    if (object.category==1 && obdVersion==300) {quint8 count=0;in>>count;groupCount=count;}
    if (groupCount<1 || groupCount>2) {if(error)*error="Invalid OBD frame groups";return false;}
    for (int index=0;index<groupCount;++index) {
        ClientFrameGroup group;
        if (!readGroup(in,object.category,obdVersion,group,object.sprites,error)) return false;
        if (index==0) {
            object.item.width=group.width;object.item.height=group.height;object.item.exact_size=group.exact_size;
            object.item.layers=group.layers;object.item.pattern_x=group.pattern_x;object.item.pattern_y=group.pattern_y;
            object.item.pattern_z=group.pattern_z;object.item.frames=group.frames;
            object.item.sprite_ids=group.sprite_ids;object.item.animation_data=group.animation_data;
        }
        if (object.category==1) object.item.frame_groups.push_back(std::move(group));
    }
    if (in.status()!=QDataStream::Ok || !in.atEnd()) {if(error)*error="Unexpected data at end of OBD file";return false;}
    object.item.modified=true;
    return true;
}
