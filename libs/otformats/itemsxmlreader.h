#ifndef ITEMSXMLREADER_H
#define ITEMSXMLREADER_H

#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>
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
    bool dirty() const { return !m_dirtyNames.isEmpty() || !m_dirtyAttributes.isEmpty() || !m_removedAttributes.isEmpty(); }
    QString filePath() const { return m_filePath; }

    QString nameForServerId(int serverId) const;
    void setNameForServerId(int serverId, const QString &name);

    QString typeForServerId(int serverId) const;
    QVariantMap attributesForServerId(int serverId) const;
    bool setAttributeForServerId(int serverId,const QString &key,const QString &value);
    bool removeAttributeForServerId(int serverId,const QString &key);
    bool isTeleport(int serverId) const;

signals:
    void loadedChanged();

private:
    bool writeFile(const QString &target);
    struct Entry {
        QString name;
        QString type;
        QMap<QString,QString> itemAttributes;
        QMap<QString,QString> attributes;
    };

    QHash<int, Entry> m_items;
    QSet<int> m_exactIds;
    QHash<int, QString> m_dirtyNames;
    QHash<int, QMap<QString,QString>> m_dirtyAttributes;
    QHash<int, QSet<QString>> m_removedAttributes;
    QString m_filePath;
};

#endif
