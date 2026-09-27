#include "binaryreader.h"

#include <QFile>
#include <QtEndian>
#include <cstring>

BinaryReader::BinaryReader(const QString &path)
{
    open(path);
}

BinaryReader::~BinaryReader()
{
    close();
}

bool BinaryReader::open(const QString &path)
{
    close();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Failed to open file: %1").arg(path));
        return false;
    }
    m_data = file.readAll();
    if (m_data.size() != file.size()) {
        setError(QStringLiteral("Failed to buffer the complete file: %1").arg(path));
        m_data.clear();
        return false;
    }
    m_position = 0;
    m_fileSize = static_cast<size_t>(m_data.size());
    m_open = true;
    return true;
}

void BinaryReader::close()
{
    m_data.clear();
    m_position = 0;
    m_fileSize = 0;
    m_open = false;
    clearError();
}

uint8_t BinaryReader::readU8()
{
    if (remaining() < 1) {
        setError(QStringLiteral("Failed to read U8 at offset %1").arg(m_position));
        return 0;
    }
    return static_cast<uint8_t>(m_data.at(static_cast<qsizetype>(m_position++)));
}

uint16_t BinaryReader::readU16()
{
    if (remaining() < 2) {
        setError(QStringLiteral("Failed to read U16 at offset %1").arg(m_position));
        return 0;
    }
    const auto *ptr = reinterpret_cast<const uchar *>(m_data.constData() + m_position);
    m_position += 2;
    return qFromLittleEndian<quint16>(ptr);
}

uint32_t BinaryReader::readU32()
{
    if (remaining() < 4) {
        setError(QStringLiteral("Failed to read U32 at offset %1").arg(m_position));
        return 0;
    }
    const auto *ptr = reinterpret_cast<const uchar *>(m_data.constData() + m_position);
    m_position += 4;
    return qFromLittleEndian<quint32>(ptr);
}

uint64_t BinaryReader::readU64()
{
    if (remaining() < 8) {
        setError(QStringLiteral("Failed to read U64 at offset %1").arg(m_position));
        return 0;
    }
    const auto *ptr = reinterpret_cast<const uchar *>(m_data.constData() + m_position);
    m_position += 8;
    return qFromLittleEndian<quint64>(ptr);
}

int8_t BinaryReader::readS8() { return static_cast<int8_t>(readU8()); }
int16_t BinaryReader::readS16() { return static_cast<int16_t>(readU16()); }
int32_t BinaryReader::readS32() { return static_cast<int32_t>(readU32()); }

QString BinaryReader::readString()
{
    uint16_t length = readU16();
    if (m_error) return QString();
    return readString(length);
}

QString BinaryReader::readString(size_t length)
{
    if (length == 0) return QString();

    size_t rem = remaining();
    if (length > rem) {
        setError(QStringLiteral("String length %1 exceeds remaining file size %2").arg(length).arg(rem));
        return QString();
    }

    const QByteArray buf = m_data.mid(static_cast<qsizetype>(m_position),
                                      static_cast<qsizetype>(length));
    m_position += length;
    return QString::fromLatin1(buf);
}

std::vector<uint8_t> BinaryReader::readBytes(size_t count)
{
    size_t rem = remaining();
    if (count > rem) {
        setError(QStringLiteral("Byte count %1 exceeds remaining file size %2").arg(count).arg(rem));
        return {};
    }

    std::vector<uint8_t> result(count);
    if (count > 0) {
        std::memcpy(result.data(), m_data.constData() + m_position, count);
        m_position += count;
    }
    return result;
}

size_t BinaryReader::tell() const
{
    return m_position;
}

bool BinaryReader::seek(size_t position)
{
    if (position > m_fileSize)
        return false;
    m_position = position;
    return true;
}

bool BinaryReader::skip(size_t bytes)
{
    return seek(m_position + bytes);
}

size_t BinaryReader::remaining() const
{
    size_t pos = tell();
    if (pos == static_cast<size_t>(-1) || pos > m_fileSize) return 0;
    return m_fileSize - pos;
}

bool BinaryReader::eof() const
{
    return remaining() == 0;
}

void BinaryReader::setError(const QString &message)
{
    m_error = true;
    m_errorMessage = message;
}

void BinaryReader::clearError()
{
    m_error = false;
    m_errorMessage.clear();
}
