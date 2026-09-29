#ifndef ITEMSXMLREADER_H
#define ITEMSXMLREADER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class ItemsXmlReader : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(int count READ count NOTIFY loadedChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY loadedChanged)

public:
    explicit ItemsXmlReader(QObject *parent = nullptr);

    Q_INVOKABLE bool loadForVersion(int version);

    Q_INVOKABLE bool loadForDir(const QString &dirName);
    Q_INVOKABLE bool loadFile(const QString &path);
    Q_INVOKABLE bool saveFile(const QString &path = QString());
    bool saveCopy(const QString &path);
    Q_INVOKABLE void clear();

    int count() const { return m_items.size(); }
    bool hasData() const { return !m_items.isEmpty(); }
    bool dirty() const { return !m_dirtyNames.isEmpty(); }
    QString filePath() const { return m_filePath; }

    QString nameForServerId(int serverId) const;
    void setNameForServerId(int serverId, const QString &name);

    QString typeForServerId(int serverId) const;
    bool isTeleport(int serverId) const;

signals:
    void loadedChanged();

private:
    bool writeFile(const QString &target);
    struct Entry {
        QString name;
        QString type;
    };

    QHash<int, Entry> m_items;
    QHash<int, QString> m_dirtyNames;
    QString m_filePath;
};

#endif
