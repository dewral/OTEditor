#include "itemsxmlreader.h"

#include "dmedatadir.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>

ItemsXmlReader::ItemsXmlReader(QObject *parent)
    : QObject(parent)
{
}

void ItemsXmlReader::clear()
{
    const bool hadData = !m_items.isEmpty();
    m_items.clear();
    m_exactIds.clear();
    m_dirtyNames.clear();
    m_dirtyAttributes.clear();
    m_removedAttributes.clear();
    m_filePath.clear();
    if (hadData) emit loadedChanged();
}

bool ItemsXmlReader::loadForVersion(int version)
{
    return loadForDir(QString::number(version));
}

bool ItemsXmlReader::loadForDir(const QString &dirName)
{
    const QString path = QDir(dmeDataDir())
                             .filePath(QStringLiteral("%1/items.xml").arg(dirName));
    if (!QFile::exists(path)) {
        clear();
        return false;
    }
    return loadFile(path);
}

bool ItemsXmlReader::loadFile(const QString &path)
{
    m_items.clear();
    m_exactIds.clear();
    m_dirtyNames.clear();
    m_dirtyAttributes.clear();
    m_removedAttributes.clear();
    m_filePath.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit loadedChanged();
        return false;
    }

    QXmlStreamReader xml(&f);

    QVector<int> currentIds;

    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            const auto tag = xml.name();
            if (tag == QLatin1String("item")) {
                currentIds.clear();
                const auto a = xml.attributes();
                const QString name = a.value(QLatin1String("name")).toString();

                if (a.hasAttribute(QLatin1String("id"))) {
                    const int id = a.value(QLatin1String("id")).toInt();
                    if (id > 0) {currentIds.append(id);m_exactIds.insert(id);}
                } else if (a.hasAttribute(QLatin1String("fromid"))
                           && a.hasAttribute(QLatin1String("toid"))) {
                    const int from = a.value(QLatin1String("fromid")).toInt();
                    const int to = a.value(QLatin1String("toid")).toInt();

                    if (from > 0 && to >= from && (to - from) <= 65535) {
                        for (int id = from; id <= to; ++id) currentIds.append(id);
                    }
                }

                for (int id : currentIds) {
                    Entry &e = m_items[id];
                    if (!name.isEmpty()) e.name = name;
                    for (const auto &attribute:a) {
                        const QString key=attribute.name().toString();
                        if (key!="id" && key!="fromid" && key!="toid")
                            e.itemAttributes.insert(key,attribute.value().toString());
                    }
                }
            } else if (tag == QLatin1String("attribute") && !currentIds.isEmpty()) {
                const auto a = xml.attributes();
                const QString key=a.value(QLatin1String("key")).toString();
                if (!key.isEmpty() && a.hasAttribute(QLatin1String("value"))) {
                    const QString value=a.value(QLatin1String("value")).toString();
                    for (int id : currentIds) {
                        m_items[id].attributes.insert(key,value);
                        if (key.compare(QLatin1String("type"),Qt::CaseInsensitive)==0)
                            m_items[id].type=value.toLower();
                    }
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == QLatin1String("item")) currentIds.clear();
        }
    }

    const bool ok = !xml.hasError();
    if (ok) m_filePath = path;
    emit loadedChanged();
    return ok;
}

bool ItemsXmlReader::saveFile(const QString &path)
{
    if (m_filePath.isEmpty()) return false;
    const QString target = path.isEmpty() ? m_filePath : path;
    if (target == m_filePath && !dirty()) return true;

    if (!writeFile(target)) return false;
    m_dirtyNames.clear();
    m_dirtyAttributes.clear();
    m_removedAttributes.clear();
    m_filePath = target;
    emit loadedChanged();
    return true;
}

bool ItemsXmlReader::saveCopy(const QString &path)
{
    return !m_filePath.isEmpty() && !path.isEmpty() && writeFile(path);
}

bool ItemsXmlReader::writeFile(const QString &target)
{
    QFile input(m_filePath);
    if (!input.open(QIODevice::ReadOnly)) return false;

    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly)) return false;

    QXmlStreamReader xml(&input);
    QXmlStreamWriter writer(&output);
    QSet<int> handledIds;
    QSet<QString> writtenKeys;
    int currentId=0;

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement() && xml.name() == QLatin1String("item")) {
            const QXmlStreamAttributes attributes = xml.attributes();
            const bool hasExactId = attributes.hasAttribute(QLatin1String("id"));
            const int id = hasExactId
                               ? attributes.value(QLatin1String("id")).toInt()
                               : 0;
            currentId=id>0?id:0;
            writtenKeys.clear();
            if (currentId>0 && (m_dirtyNames.contains(currentId) || m_dirtyAttributes.contains(currentId) || m_removedAttributes.contains(currentId))) {
                writer.writeStartElement(xml.qualifiedName().toString());
                for (const auto &declaration : xml.namespaceDeclarations()) {
                    writer.writeNamespace(declaration.namespaceUri().toString(),
                                          declaration.prefix().toString());
                }

                bool wroteName = false;
                const auto changed=m_dirtyAttributes.value(id);
                const auto removed=m_removedAttributes.value(id);
                for (const QXmlStreamAttribute &attribute : attributes) {
                    const QString key=attribute.name().toString();
                    if (removed.contains("@"+key)) continue;
                    if (changed.contains("@"+key)) writer.writeAttribute(key,changed.value("@"+key));
                    else if (key=="name" && m_dirtyNames.contains(id)) writer.writeAttribute(key,m_dirtyNames.value(id));
                    else writer.writeAttribute(attribute);
                    if (key=="name") wroteName=true;
                    writtenKeys.insert("@"+key);
                }
                if (!wroteName && m_dirtyNames.contains(id) && !removed.contains("@name")) {
                    writer.writeAttribute(QStringLiteral("name"),
                                              m_dirtyNames.value(id));
                    writtenKeys.insert("@name");
                }
                for (auto it=changed.cbegin();it!=changed.cend();++it) {
                    if (!it.key().startsWith('@') || writtenKeys.contains(it.key())) continue;
                    writer.writeAttribute(it.key().mid(1),it.value());
                    writtenKeys.insert(it.key());
                }
                handledIds.insert(id);
                continue;
            }
        }

        if (xml.isStartElement() && xml.name()==QLatin1String("attribute") && currentId>0) {
            const QString key=xml.attributes().value(QLatin1String("key")).toString();
            if (m_removedAttributes.value(currentId).contains(key)) {
                xml.skipCurrentElement();
                continue;
            }
            if (m_dirtyAttributes.value(currentId).contains(key)) {
                writer.writeStartElement(xml.qualifiedName().toString());
                for (const auto &attribute:xml.attributes()) {
                    if (attribute.name()==QLatin1String("value"))
                        writer.writeAttribute(QStringLiteral("value"),m_dirtyAttributes.value(currentId).value(key));
                    else writer.writeAttribute(attribute);
                }
                if (!xml.attributes().hasAttribute(QLatin1String("value")))
                    writer.writeAttribute(QStringLiteral("value"),m_dirtyAttributes.value(currentId).value(key));
                writtenKeys.insert(key);
                continue;
            }
        }

        if (xml.isEndElement() && xml.name()==QLatin1String("item") && currentId>0) {
            const auto pending=m_dirtyAttributes.value(currentId);
            for (auto it=pending.cbegin();it!=pending.cend();++it) {
                if (it.key().startsWith('@') || writtenKeys.contains(it.key())) continue;
                writer.writeStartElement(QStringLiteral("attribute"));
                writer.writeAttribute(QStringLiteral("key"),it.key());
                writer.writeAttribute(QStringLiteral("value"),it.value());
                writer.writeEndElement();
            }
            currentId=0;
        }

        if (xml.isEndElement() && xml.name() == QLatin1String("items")) {
            QSet<int> pendingSet;
            for (int id:m_dirtyNames.keys()) pendingSet.insert(id);
            for (int id:m_dirtyAttributes.keys()) pendingSet.insert(id);
            QList<int> pendingIds = pendingSet.values();
            std::sort(pendingIds.begin(), pendingIds.end());
            for (const int id : pendingIds) {
                if (handledIds.contains(id)) continue;
                writer.writeStartElement(QStringLiteral("item"));
                writer.writeAttribute(QStringLiteral("id"), QString::number(id));
                const auto entry=m_items.value(id);
                for (auto it=entry.itemAttributes.cbegin();it!=entry.itemAttributes.cend();++it)
                    writer.writeAttribute(it.key(),it.value());
                for (auto it=entry.attributes.cbegin();it!=entry.attributes.cend();++it) {
                    writer.writeStartElement(QStringLiteral("attribute"));
                    writer.writeAttribute(QStringLiteral("key"),it.key());
                    writer.writeAttribute(QStringLiteral("value"),it.value());
                    writer.writeEndElement();
                }
                writer.writeEndElement();
            }
        }

        writer.writeCurrentToken(xml);
    }

    input.close();
    if (xml.hasError() || !output.commit()) return false;

    return true;
}

QString ItemsXmlReader::nameForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? QString() : it->name;
}

void ItemsXmlReader::setNameForServerId(int serverId, const QString &name)
{
    if (serverId <= 0 || m_filePath.isEmpty()) return;
    Entry &entry = m_items[serverId];
    if (entry.name == name && !m_dirtyNames.contains(serverId)) return;
    entry.name = name;
    entry.itemAttributes.insert(QStringLiteral("name"),name);
    m_dirtyNames.insert(serverId, name);
}

QString ItemsXmlReader::typeForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? QString() : it->type;
}

QVariantMap ItemsXmlReader::attributesForServerId(int serverId) const
{
    QVariantMap result;
    const auto it=m_items.constFind(serverId);
    if (it==m_items.cend()) return result;
    for (auto attribute=it->itemAttributes.cbegin();attribute!=it->itemAttributes.cend();++attribute)
        result.insert("@"+attribute.key(),attribute.value());
    for (auto attribute=it->attributes.cbegin();attribute!=it->attributes.cend();++attribute)
        result.insert(attribute.key(),attribute.value());
    return result;
}

bool ItemsXmlReader::setAttributeForServerId(int serverId,const QString &key,const QString &value)
{
    const QString normalized=key.trimmed();
    if (serverId<=0 || m_filePath.isEmpty() || normalized.isEmpty()) return false;
    if (normalized.startsWith('@') &&
        (normalized.size()==1 || QStringList{"id","fromid","toid"}.contains(normalized.mid(1),Qt::CaseInsensitive))) return false;
    Entry &entry=m_items[serverId];
    if (normalized.startsWith('@')) {
        const QString itemKey=normalized.mid(1);
        if (entry.itemAttributes.value(itemKey)==value && entry.itemAttributes.contains(itemKey)) return true;
        entry.itemAttributes.insert(itemKey,value);
        if (itemKey=="name") {entry.name=value;m_dirtyNames.insert(serverId,value);}
    } else {
        if (entry.attributes.value(normalized)==value && entry.attributes.contains(normalized)) return true;
        entry.attributes.insert(normalized,value);
        if (normalized.compare(QLatin1String("type"),Qt::CaseInsensitive)==0) entry.type=value.toLower();
    }
    m_dirtyAttributes[serverId].insert(normalized,value);
    m_removedAttributes[serverId].remove(normalized);
    if (m_removedAttributes.value(serverId).isEmpty()) m_removedAttributes.remove(serverId);
    emit loadedChanged();
    return true;
}

bool ItemsXmlReader::removeAttributeForServerId(int serverId,const QString &key)
{
    if (serverId<=0 || m_filePath.isEmpty() || !m_exactIds.contains(serverId)) return false;
    auto it=m_items.find(serverId);
    if (it==m_items.end()) return false;
    if (key.startsWith('@')) {
        const QString itemKey=key.mid(1);
        if (QStringList{"id","fromid","toid"}.contains(itemKey,Qt::CaseInsensitive) || !it->itemAttributes.contains(itemKey)) return false;
        it->itemAttributes.remove(itemKey);
        if (itemKey=="name") {it->name.clear();m_dirtyNames.remove(serverId);}
    } else {
        if (!it->attributes.contains(key)) return false;
        it->attributes.remove(key);
        if (key.compare(QLatin1String("type"),Qt::CaseInsensitive)==0) it->type.clear();
    }
    m_dirtyAttributes[serverId].remove(key);
    if (m_dirtyAttributes.value(serverId).isEmpty()) m_dirtyAttributes.remove(serverId);
    m_removedAttributes[serverId].insert(key);
    emit loadedChanged();
    return true;
}

bool ItemsXmlReader::isTeleport(int serverId) const
{
    return typeForServerId(serverId) == QLatin1String("teleport");
}
