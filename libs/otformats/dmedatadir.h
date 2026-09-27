#ifndef DMEDATADIR_H
#define DMEDATADIR_H

#include <QCoreApplication>
#include <QDir>
#include <QString>

inline QString dmeDataDir()
{
#ifdef DME_DATA_DIR
    return QStringLiteral(DME_DATA_DIR);
#else
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data"));
#endif
}

#endif
