#pragma once

#include "datreader.h"
#include "sprreader.h"
#include <QImage>
#include <QString>
#include <QVector>

// Object Builder Data v3 stores one DAT object and its 32x32 ARGB sprites.
struct ObdObject {
    int category = -1;
    int clientVersion = 0;
    ClientItem item;
    QVector<QImage> sprites;
};

class ObdCodec {
public:
    static QByteArray encode(int clientVersion, int category, const ClientItem &item,
                             SprReader &sprites, QString *error=nullptr);
    static bool decode(const QByteArray &fileData, ObdObject &object, QString *error=nullptr);
};
