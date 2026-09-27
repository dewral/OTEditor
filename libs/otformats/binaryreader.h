#ifndef BINARYREADER_H
#define BINARYREADER_H

#include <QString>
#include <QByteArray>
#include <cstdint>
#include <vector>

class BinaryReader
{
public:
    BinaryReader() = default;
    explicit BinaryReader(const QString &path);
    ~BinaryReader();

    BinaryReader(const BinaryReader &) = delete;
    BinaryReader &operator=(const BinaryReader &) = delete;

    bool open(const QString &path);
    void close();
    bool isOpen() const { return m_open; }

    uint8_t readU8();
    uint16_t readU16();
    uint32_t readU32();
    uint64_t readU64();

    int8_t readS8();
    int16_t readS16();
    int32_t readS32();

    QString readString();
    QString readString(size_t length);

    std::vector<uint8_t> readBytes(size_t count);

    size_t tell() const;
    bool seek(size_t position);
    bool skip(size_t bytes);

    size_t size() const { return m_fileSize; }
    size_t remaining() const;
    bool eof() const;

    bool good() const { return !m_error && m_open; }
    bool hasError() const { return m_error; }
    const QString &getError() const { return m_errorMessage; }
    void clearError();

private:
    void setError(const QString &message);

    QByteArray m_data;
    size_t m_position = 0;
    size_t m_fileSize = 0;
    bool m_open = false;
    bool m_error = false;
    QString m_errorMessage;
};

#endif
