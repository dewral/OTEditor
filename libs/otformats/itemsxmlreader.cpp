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
    m_dirtyNames.clear();
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
    m_dirtyNames.clear();
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
                    if (id > 0) currentIds.append(id);
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
                }
            } else if (tag == QLatin1String("attribute") && !currentIds.isEmpty()) {
                const auto a = xml.attributes();

                if (a.value(QLatin1String("key")).toString().compare(
                        QLatin1String("type"), Qt::CaseInsensitive) == 0) {
                    const QString type = a.value(QLatin1String("value")).toString().toLower();
                    for (int id : currentIds) m_items[id].type = type;
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
    if (target == m_filePath && m_dirtyNames.isEmpty()) return true;

    if (!writeFile(target)) return false;
    m_dirtyNames.clear();
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

    while (!xml.atEnd()) {
        xml.readNext();

        if (xml.isStartElement() && xml.name() == QLatin1String("item")) {
            const QXmlStreamAttributes attributes = xml.attributes();
            const bool hasExactId = attributes.hasAttribute(QLatin1String("id"));
            const int id = hasExactId
                               ? attributes.value(QLatin1String("id")).toInt()
                               : 0;
            if (id > 0 && m_dirtyNames.contains(id)) {
                writer.writeStartElement(xml.qualifiedName().toString());
                for (const auto &declaration : xml.namespaceDeclarations()) {
                    writer.writeNamespace(declaration.namespaceUri().toString(),
                                          declaration.prefix().toString());
                }

                bool wroteName = false;
                for (const QXmlStreamAttribute &attribute : attributes) {
                    if (attribute.name() == QLatin1String("name")) {
                        writer.writeAttribute(QStringLiteral("name"),
                                              m_dirtyNames.value(id));
                        wroteName = true;
                    } else {
                        writer.writeAttribute(attribute);
                    }
                }
                if (!wroteName) {
                    writer.writeAttribute(QStringLiteral("name"),
                                          m_dirtyNames.value(id));
                }
                handledIds.insert(id);
                continue;
            }
        }

        if (xml.isEndElement() && xml.name() == QLatin1String("items")) {
            QList<int> pendingIds = m_dirtyNames.keys();
            std::sort(pendingIds.begin(), pendingIds.end());
            for (const int id : pendingIds) {
                if (handledIds.contains(id)) continue;
                writer.writeStartElement(QStringLiteral("item"));
                writer.writeAttribute(QStringLiteral("id"), QString::number(id));
                writer.writeAttribute(QStringLiteral("name"), m_dirtyNames.value(id));
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
    m_dirtyNames.insert(serverId, name);
}

QString ItemsXmlReader::typeForServerId(int serverId) const
{
    auto it = m_items.constFind(serverId);
    return it == m_items.cend() ? QString() : it->type;
}

bool ItemsXmlReader::isTeleport(int serverId) const
{
    return typeForServerId(serverId) == QLatin1String("teleport");
}
