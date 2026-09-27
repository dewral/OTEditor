#include "nodefilereader.h"

#include <QFile>
#include <algorithm>
#include <utility>

namespace {

uint16_t readLe16(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint16_t>(raw[offset])
         | static_cast<uint16_t>(raw[offset + 1] << 8);
}

uint32_t readLe32(const QByteArray &data, qsizetype offset)
{
    const auto *raw = reinterpret_cast<const uchar *>(data.constData());
    return static_cast<uint32_t>(raw[offset])
         | (static_cast<uint32_t>(raw[offset + 1]) << 8)
         | (static_cast<uint32_t>(raw[offset + 2]) << 16)
         | (static_cast<uint32_t>(raw[offset + 3]) << 24);
}

}

bool BinaryNode::getU8(uint8_t &value)
{
    if (m_readOffset + 1 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = static_cast<uint8_t>(m_data.at(m_readOffset));
    ++m_readOffset;
    return true;
}

bool BinaryNode::getU16(uint16_t &value)
{
    if (m_readOffset + 2 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = readLe16(m_data, m_readOffset);
    m_readOffset += 2;
    return true;
}

bool BinaryNode::getU32(uint32_t &value)
{
    if (m_readOffset + 4 > m_data.size()) {
        m_readOffset = m_data.size();
        return false;
    }

    value = readLe32(m_data, m_readOffset);
    m_readOffset += 4;
    return true;
}

bool BinaryNode::getString(QString &value)
{
    uint16_t length = 0;
    if (!getU16(length)) {
        return false;
    }

    QByteArray bytes;
    if (!readBytes(length, bytes)) {
        return false;
    }

    value = QString::fromLatin1(bytes);
    return true;
}

bool BinaryNode::skip(qsizetype count)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_data.size();
        return false;
    }

    m_readOffset += count;
    return true;
}

bool BinaryNode::readBytes(qsizetype count, QByteArray &out)
{
    if (count < 0 || count > bytesRemaining()) {
        m_readOffset = m_data.size();
        return false;
    }

    out = m_data.mid(m_readOffset, count);
    m_readOffset += count;
    return true;
}

qsizetype BinaryNode::bytesRemaining() const
{
    return m_readOffset < m_data.size() ? m_data.size() - m_readOffset : 0;
}

bool NodeFileReader::loadFile(const QString &path,
                              const QVector<QByteArray> &acceptedIdentifiers,
                              ProgressCallback progressCallback)
{
    m_root = BinaryNode();
    m_ok = false;
    m_errorString.clear();
    m_progressCallback = std::move(progressCallback);
    m_lastProgressPosition = 0;
    reportProgress(0, 1);

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot open file: %1").arg(path));
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 5) {
        setError(QStringLiteral("The file is too small to be a valid node file"));
        return false;
    }

    const QByteArray identifier = data.left(4);
    const bool wildcardIdentifier = identifier == QByteArray(4, '\0');
    bool accepted = wildcardIdentifier || acceptedIdentifiers.isEmpty();
    for (const QByteArray &candidate : acceptedIdentifiers) {
        if (candidate.size() == 4 && identifier == candidate) {
            accepted = true;
            break;
        }
    }

    if (!accepted) {
        setError(QStringLiteral("Invalid file identifier"));
        return false;
    }

    qsizetype pos = 4;
    if (static_cast<uint8_t>(data.at(pos)) != Start) {
        setError(QStringLiteral("Invalid node file structure"));
        return false;
    }
    ++pos;

    if (!parseNode(data, pos, m_root, 0)) {
        return false;
    }

    m_ok = true;
    reportProgress(data.size(), data.size());
    return true;
}

bool NodeFileReader::parseNode(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth)
{
    if (depth > kMaxDepth) {
        setError(QStringLiteral("Node nesting is too deep (corrupt file?)"));
        return false;
    }

    while (pos < data.size()) {
        reportProgress(pos, data.size());
        uint8_t byte = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (byte == Escape) {
            if (pos >= data.size()) {
                setError(QStringLiteral("Truncated escape sequence in node file"));
                return false;
            }
            node.m_data.append(data.at(pos));
            ++pos;
            continue;
        }

        if (byte == Start) {
            --pos;
            return parseChildNodes(data, pos, node, depth);
        }

        if (byte == End) {
            return true;
        }

        node.m_data.append(static_cast<char>(byte));
    }

    setError(QStringLiteral("Unexpected end of node file"));
    return false;
}

void NodeFileReader::reportProgress(qsizetype position, qsizetype total)
{
    if (!m_progressCallback || total <= 0) {
        return;
    }
    constexpr qsizetype kReportInterval = 64 * 1024;
    if (position < total && position - m_lastProgressPosition < kReportInterval) {
        return;
    }
    m_lastProgressPosition = position;
    m_progressCallback(std::clamp(static_cast<double>(position) / static_cast<double>(total),
                                  0.0, 1.0));
}

bool NodeFileReader::parseChildNodes(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth)
{
    while (pos < data.size()) {
        uint8_t marker = static_cast<uint8_t>(data.at(pos));
        ++pos;

        if (marker == Start) {
            BinaryNode child;
            if (!parseNode(data, pos, child, depth + 1)) {
                return false;
            }
            node.m_children.append(std::move(child));
            continue;
        }

        if (marker == End) {
            return true;
        }

        setError(QStringLiteral("Invalid child marker in node file"));
        return false;
    }

    setError(QStringLiteral("Unexpected end of child list in node file"));
    return false;
}

void NodeFileReader::setError(const QString &message)
{
    m_errorString = message;
    m_ok = false;
}
