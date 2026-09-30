#pragma once

#include "datreader.h"
#include "itemsxmlreader.h"
#include "otbreader.h"
#include "sprreader.h"

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <memory>
#include <functional>

class ProjectModel final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool otbLoaded READ otbLoaded NOTIFY changed)
    Q_PROPERTY(bool itemsXmlLoaded READ itemsXmlLoaded NOTIFY changed)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)

public:
    explicit ProjectModel(QObject *parent = nullptr);

    bool open(const QString &folder, int clientVersion, bool alphaWithoutOtfi,
              QString *error = nullptr, const QString &serverFolder = QString(),
              int spriteSizeOverride = 0);
    using CompileProgress = std::function<void(int, const QString &)>;
    bool compile(QString *error = nullptr, const CompileProgress &progress = {});
    bool compileAs(const QString &folder, QString *error = nullptr, const CompileProgress &progress = {});
    bool createOtb(QString *error = nullptr);
    bool saveOtbAs(const QString &target, QString *error = nullptr);
    bool saveItemsXmlAs(const QString &target, QString *error = nullptr);
    bool convertFrameGroups(bool enabled, bool removeMounts=false);
    bool convertFrameDurations(bool enabled, int minimum=100, int maximum=100);
    void close();

    bool loaded() const { return m_dat && m_dat->isLoaded() && m_sprites && m_sprites->isLoaded(); }
    bool dirty() const;
    bool otbLoaded() const { return m_otb && m_otb->isLoaded(); }
    bool itemsXmlLoaded() const { return m_itemsXml && m_itemsXml->hasData(); }
    QString folder() const { return m_folder; }

    DatReader *dat() const { return m_dat.get(); }
    SprReader *sprites() const { return m_sprites.get(); }
    OtbReader *otb() const { return m_otb.get(); }
    ItemsXmlReader *itemsXml() const { return m_itemsXml.get(); }

    int clientVersion() const { return m_clientVersion; }
    bool extended() const { return m_extended; }
    bool transparency() const { return m_transparency; }
    bool frameDurations() const { return m_frameDurations; }
    bool frameGroups() const { return m_frameGroups; }
    bool hasOtfi() const { return m_hasOtfi; }
    QString attributeServer() const { return m_attributeServer; }
    QString metadataController() const { return m_metadataController; }
    int spriteSize() const { return m_spriteSize; }

signals:
    void changed();

private:
    static QString findFile(const QDir &folder, const QString &preferred,
                            const QString &suffix);
    static QString findServerFile(const QString &folder, const QString &fileName,
                                  int clientVersion);
    bool writeOtfi(const QString &targetPath, QString *error);
    void connectStateSignals();

    std::unique_ptr<DatReader> m_dat;
    std::unique_ptr<SprReader> m_sprites;
    std::unique_ptr<OtbReader> m_otb;
    std::unique_ptr<ItemsXmlReader> m_itemsXml;

    QString m_folder;
    QString m_serverFolder;
    QString m_datPath;
    QString m_sprPath;
    QString m_otbPath;
    QString m_itemsXmlPath;
    QString m_otfiPath;
    QByteArray m_otfiData;
    QString m_datName = QStringLiteral("Tibia.dat");
    QString m_sprName = QStringLiteral("Tibia.spr");
    int m_clientVersion = 1098;
    bool m_hasOtfi = false;
    bool m_otfiSizeModified = false;
    bool m_otfiGroupsModified = false;
    bool m_otfiDurationsModified = false;
    bool m_extended = false;
    bool m_transparency = false;
    bool m_frameDurations = false;
    bool m_frameGroups = false;
    QString m_attributeServer;
    QString m_metadataController;
    int m_spriteSize = 32;
};
