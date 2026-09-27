#ifndef NODEFILEREADER_H
#define NODEFILEREADER_H

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>
#include <functional>

class BinaryNode
{
public:
    bool getU8(uint8_t &value);
    bool getU16(uint16_t &value);
    bool getU32(uint32_t &value);
    bool getString(QString &value);
    bool skip(qsizetype count);
    bool readBytes(qsizetype count, QByteArray &out);

    qsizetype bytesRemaining() const;
    const QVector<BinaryNode> &children() const { return m_children; }
    const QByteArray &rawData() const { return m_data; }

private:
    QByteArray m_data;
    QVector<BinaryNode> m_children;
    qsizetype m_readOffset = 0;

    friend class NodeFileReader;
};

class NodeFileReader
{
public:
    using ProgressCallback = std::function<void(double)>;

    bool loadFile(const QString &path,
                  const QVector<QByteArray> &acceptedIdentifiers,
                  ProgressCallback progressCallback = {});

    bool isOk() const { return m_ok; }
    QString errorString() const { return m_errorString; }
    BinaryNode &rootNode() { return m_root; }
    const BinaryNode &rootNode() const { return m_root; }

private:
    enum Marker : uint8_t {
        Start = 0xFE,
        End = 0xFF,
        Escape = 0xFD
    };

    static constexpr int kMaxDepth = 512;
    bool parseNode(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth);
    bool parseChildNodes(const QByteArray &data, qsizetype &pos, BinaryNode &node, int depth);
    void reportProgress(qsizetype position, qsizetype total);
    void setError(const QString &message);

    BinaryNode m_root;
    bool m_ok = false;
    QString m_errorString;
    ProgressCallback m_progressCallback;
    qsizetype m_lastProgressPosition = 0;
};

#endif
