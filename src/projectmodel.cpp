#include "projectmodel.h"
#include "otfireader.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QTextStream>

ProjectModel::ProjectModel(QObject *parent)
    : QObject(parent)
{
}

QString ProjectModel::findFile(const QDir &folder, const QString &preferred,
                               const QString &suffix)
{
    if (!preferred.isEmpty()) {
        const QString candidate = folder.filePath(preferred);
        if (QFileInfo::exists(candidate)) return QFileInfo(candidate).absoluteFilePath();
    }
    const QFileInfoList files = folder.entryInfoList({QStringLiteral("*.%1").arg(suffix)},
                                                     QDir::Files, QDir::Name);
    return files.isEmpty() ? QString() : files.constFirst().absoluteFilePath();
}

QString ProjectModel::findServerFile(const QString &folder, const QString &fileName,
                                     int clientVersion)
{
    const QString direct=QDir(folder).filePath(fileName);
    if(QFileInfo::exists(direct))return QFileInfo(direct).absoluteFilePath();
    QDirIterator iterator(folder, {fileName}, QDir::Files, QDirIterator::Subdirectories);
    QStringList matches;
    while(iterator.hasNext())matches.append(QFileInfo(iterator.next()).absoluteFilePath());
    const QString numeric=QString::number(clientVersion);
    const QString dotted=QString::number(clientVersion/100)+"."
                         +QString::number(clientVersion%100).rightJustified(2,'0');
    if(matches.size()==1){
        const QString parentName=QFileInfo(matches.constFirst()).dir().dirName();
        const QRegularExpression versionFolder(QStringLiteral("^(?:\\d{3,5}|\\d{1,2}[._]\\d{2})$"));
        if(versionFolder.match(parentName).hasMatch() && parentName!=numeric && parentName!=dotted)
            return {};
        return matches.constFirst();
    }
    QStringList versionMatches;
    for(const QString &candidate:matches){
        const QStringList parts=QDir::fromNativeSeparators(QFileInfo(candidate).absolutePath()).split('/');
        if(parts.contains(numeric,Qt::CaseInsensitive) || parts.contains(dotted,Qt::CaseInsensitive))
            versionMatches.append(candidate);
    }
    if(versionMatches.size()==1)return versionMatches.constFirst();
    return {};
}

bool ProjectModel::dirty() const
{
    return loaded() && (m_dat->isDirty() || m_sprites->isDirty()
                        || m_otfiGroupsModified
                        || m_otfiDurationsModified
                        || (m_otb && m_otb->dirty())
                        || (m_itemsXml && m_itemsXml->dirty()));
}
bool ProjectModel::convertFrameGroups(bool enabled,bool removeMounts) {
    if(!loaded() || m_frameGroups==enabled)return false;
    m_dat->convertFrameGroups(enabled,removeMounts);
    m_frameGroups=enabled;
    m_dat->setOtfiOverrides(true,m_extended,m_frameDurations,m_frameGroups);
    m_otfiGroupsModified=true;
    if(m_otfiPath.isEmpty())m_otfiPath=QDir(m_folder).filePath(QStringLiteral("Tibia.otfi"));
    if(!m_otfiData.isEmpty()) {
        QString data=QString::fromUtf8(m_otfiData);
        const QRegularExpression line(QStringLiteral("(?m)^\\s*frame-groups\\s*:\\s*[^\\r\\n]*"));
        const QString replacement=QStringLiteral("  frame-groups: %1").arg(enabled?QStringLiteral("true"):QStringLiteral("false"));
        if(data.contains(line))data.replace(line,replacement);
        else data+=QStringLiteral("\n")+replacement+QStringLiteral("\n");
        m_otfiData=data.toUtf8();
    }
    emit changed();
    return true;
}
bool ProjectModel::convertFrameDurations(bool enabled,int minimum,int maximum) {
    if(!loaded() || m_frameDurations==enabled || minimum<1 || minimum>maximum)return false;
    if(m_dat->convertFrameDurations(enabled,quint32(minimum),quint32(maximum))<0)return false;
    m_frameDurations=enabled;
    m_dat->setOtfiOverrides(true,m_extended,m_frameDurations,m_frameGroups);
    m_otfiDurationsModified=true;
    if(m_otfiPath.isEmpty())m_otfiPath=QDir(m_folder).filePath(QStringLiteral("Tibia.otfi"));
    if(!m_otfiData.isEmpty()) {
        QString data=QString::fromUtf8(m_otfiData);
        const QRegularExpression line(QStringLiteral("(?m)^[ \\t]*frame-durations[ \\t]*:[ \\t]*[^\\r\\n]*"));
        const QString replacement=QStringLiteral("  frame-durations: %1").arg(enabled?QStringLiteral("true"):QStringLiteral("false"));
        if(data.contains(line))data.replace(line,replacement);
        else data+=QStringLiteral("\n")+replacement+QStringLiteral("\n");
        m_otfiData=data.toUtf8();
    }
    emit changed();
    return true;
}

void ProjectModel::connectStateSignals()
{
    connect(m_dat.get(), &DatReader::dirtyChanged, this, &ProjectModel::changed);
    connect(m_sprites.get(), &SprReader::dirtyChanged, this, &ProjectModel::changed);
    connect(m_otb.get(), &OtbReader::dirtyChanged, this, &ProjectModel::changed);
    connect(m_itemsXml.get(), &ItemsXmlReader::loadedChanged, this, &ProjectModel::changed);
}

bool ProjectModel::open(const QString &folder, int clientVersion, bool alphaWithoutOtfi,
                        QString *error, const QString &serverFolder, int spriteSizeOverride)
{
    if (spriteSizeOverride != 0 && spriteSizeOverride != 32 && spriteSizeOverride != 64
        && spriteSizeOverride != 128 && spriteSizeOverride != 256) {
        if (error) *error = QStringLiteral("Unsupported sprite dimension");
        return false;
    }
    const QDir directory(folder);
    if (!directory.exists()) {
        if (error) *error = QStringLiteral("Folder does not exist");
        return false;
    }

    OtfiReader otfi;
    const bool hasOtfi = otfi.loadFromFolder(directory.absolutePath());
    const QString datName = hasOtfi ? otfi.metadataFile() : QStringLiteral("Tibia.dat");
    const QString sprName = hasOtfi ? otfi.spritesFile() : QStringLiteral("Tibia.spr");
    const QString datPath = findFile(directory, datName, QStringLiteral("dat"));
    const QString sprPath = findFile(directory, sprName, QStringLiteral("spr"));
    if (datPath.isEmpty() || sprPath.isEmpty()) {
        if (error) *error = QStringLiteral("Missing DAT or SPR file in the selected folder");
        return false;
    }

    const bool extended = hasOtfi ? otfi.extended() : clientVersion >= 960;
    const bool transparency = hasOtfi ? otfi.transparency() : alphaWithoutOtfi;
    const bool durations = hasOtfi ? otfi.frameDurations() : clientVersion >= 1050;
    const bool groups = hasOtfi ? otfi.frameGroups() : clientVersion >= 1057;

    auto dat = std::make_unique<DatReader>();
    dat->setClientVersion(clientVersion);
    dat->setOtfiOverrides(hasOtfi, extended, durations, groups);
    if (!dat->loadFile(datPath)) {
        if (error) *error = QStringLiteral("DAT: %1. Check the selected DAT version / OTFI.")
                                .arg(dat->errorString());
        return false;
    }

    auto sprites = std::make_unique<SprReader>();
    const int detectedSpriteSize = hasOtfi ? otfi.spriteSize() : 32;
    const int spriteSize = spriteSizeOverride ? spriteSizeOverride : detectedSpriteSize;
    sprites->setSpriteSize(spriteSize);
    if (!sprites->loadFile(sprPath, 0, extended, transparency)) {
        if (error) *error = QStringLiteral("SPR: %1").arg(sprites->errorString());
        return false;
    }

    auto itemsXml = std::make_unique<ItemsXmlReader>();
    const QString itemsXmlPath = findServerFile(serverFolder.isEmpty() ? directory.absolutePath() : serverFolder,
                                                QStringLiteral("items.xml"),clientVersion);
    if (!itemsXmlPath.isEmpty()) itemsXml->loadFile(itemsXmlPath);

    QString otbPath = serverFolder.isEmpty()
        ? findFile(directory, QStringLiteral("items.otb"), QStringLiteral("otb"))
        : findServerFile(serverFolder, QStringLiteral("items.otb"),clientVersion);
    if (otbPath.isEmpty() && serverFolder.isEmpty()) {
        const QDir parent(directory.absolutePath() + QStringLiteral("/.."));
        otbPath = findFile(parent, QStringLiteral("items.otb"), QStringLiteral("otb"));
    }
    auto otb = std::make_unique<OtbReader>();
    otb->setDatReader(dat.get());
    otb->setItemsXml(itemsXml.get());
    if (!otbPath.isEmpty() && !otb->loadFile(otbPath)) {
        if (error) *error = QStringLiteral("OTB: %1").arg(otb->errorString());
        return false;
    }

    m_dat = std::move(dat);
    m_sprites = std::move(sprites);
    m_itemsXml = std::move(itemsXml);
    m_otb = std::move(otb);
    m_folder = directory.absolutePath();
    m_serverFolder = serverFolder.isEmpty() ? directory.absolutePath() : QDir(serverFolder).absolutePath();
    m_datPath = datPath;
    m_sprPath = sprPath;
    m_otbPath = otbPath;
    m_itemsXmlPath = itemsXmlPath;
    m_datName = QFileInfo(datPath).fileName();
    m_sprName = QFileInfo(sprPath).fileName();
    const QStringList otfiFiles = directory.entryList({QStringLiteral("*.otfi")}, QDir::Files, QDir::Name);
    m_otfiPath = otfiFiles.isEmpty() ? QString() : directory.filePath(otfiFiles.constFirst());
    m_otfiData.clear();
    if (!m_otfiPath.isEmpty()) {
        QFile otfiFile(m_otfiPath);
        if (otfiFile.open(QIODevice::ReadOnly)) m_otfiData = otfiFile.readAll();
    }
    m_otfiSizeModified = spriteSize != detectedSpriteSize;
    m_otfiGroupsModified = false;
    m_otfiDurationsModified = false;
    if (m_otfiSizeModified) {
        if (m_otfiPath.isEmpty()) m_otfiPath = directory.filePath(QStringLiteral("Tibia.otfi"));
        if (!m_otfiData.isEmpty()) {
            QString data = QString::fromUtf8(m_otfiData);
            auto replaceSetting = [&data](const QString &key, const QString &value) {
                const QRegularExpression re(QStringLiteral("(?m)^([ \\t]*%1:[ \\t]*)[^\\r\\n]*").arg(key));
                if (data.contains(re)) data.replace(re,QStringLiteral("\\1")+value);
                else data.replace(QStringLiteral("DatSpr\n"),QStringLiteral("DatSpr\n  %1: %2\n").arg(key,value));
            };
            replaceSetting(QStringLiteral("sprite-size"),QString::number(spriteSize));
            replaceSetting(QStringLiteral("sprite-data-size"),QString::number(spriteSize*spriteSize*(transparency?4:3)));
            m_otfiData = data.toUtf8();
        }
    }
    m_clientVersion = clientVersion;
    m_hasOtfi = hasOtfi;
    m_extended = extended;
    m_transparency = transparency;
    m_frameDurations = durations;
    m_frameGroups = groups;
    m_attributeServer = hasOtfi ? otfi.attributeServer() : QString();
    m_metadataController = hasOtfi ? otfi.metadataController() : QString();
    m_spriteSize = spriteSize;
    connectStateSignals();
    emit changed();
    return true;
}

bool ProjectModel::writeOtfi(const QString &targetPath, QString *error)
{
    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot write OTFI: %1").arg(targetPath);
        return false;
    }
    if (!m_otfiData.isEmpty()) {
        if (file.write(m_otfiData) != m_otfiData.size()) {
            if (error) *error = QStringLiteral("Cannot write OTFI: %1").arg(targetPath);
            return false;
        }
    } else {
        QTextStream out(&file);
        out << "DatSpr\n"
            << "  extended: " << (m_extended ? "true" : "false") << '\n'
            << "  transparency: " << (m_transparency ? "true" : "false") << '\n'
            << "  frame-durations: " << (m_frameDurations ? "true" : "false") << '\n'
            << "  frame-groups: " << (m_frameGroups ? "true" : "false") << '\n'
            << "  metadata-controller: default\n"
            << "  attribute-server: tfs1.4\n"
            << "  metadata-file: " << m_datName << '\n'
            << "  sprites-file: " << m_sprName << '\n'
            << "  sprite-size: " << m_spriteSize << '\n'
            << "  sprite-data-size: " << (m_spriteSize * m_spriteSize * (m_transparency ? 4 : 3)) << '\n';
        out.flush();
    }
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Cannot commit OTFI: %1").arg(targetPath);
        return false;
    }
    return true;
}

bool ProjectModel::compile(QString *error, const CompileProgress &progress)
{
    if (!loaded()) {
        if (error) *error = QStringLiteral("No project loaded");
        return false;
    }
    if (m_otb && m_otb->isLoaded()) m_otb->syncNamesToItemsXml();
    if (progress) progress(5, QStringLiteral("Saving DAT"));
    if (!m_dat->saveFile(m_datPath)) { if (error) *error = m_dat->errorString(); return false; }
    if (progress) progress(25, QStringLiteral("Saving sprites"));
    if (m_sprites->isDirty() && !m_sprites->saveFile(m_sprPath, [&](int value) {
            if (progress) progress(25 + value * 60 / 100, QStringLiteral("Saving sprites"));
        })) { if (error) *error = m_sprites->errorString(); return false; }
    if (progress) progress(85, QStringLiteral("Saving server files"));
    if (m_otb && m_otb->isLoaded() && !m_otb->saveFile(m_otbPath)) {
        if (error) *error = m_otb->errorString();
        return false;
    }
    if (m_itemsXml && m_itemsXml->hasData() && !m_itemsXml->saveFile()) {
        if (error) *error = QStringLiteral("Cannot save items.xml");
        return false;
    }
    if (progress) progress(96, QStringLiteral("Finishing"));
    if ((m_otfiSizeModified || m_otfiGroupsModified || m_otfiDurationsModified) && !writeOtfi(m_otfiPath,error)) return false;
    m_otfiSizeModified = false;
    m_otfiGroupsModified = false;
    m_otfiDurationsModified = false;
    if (progress) progress(100, QStringLiteral("Complete"));
    emit changed();
    return true;
}

bool ProjectModel::createOtb(QString *error)
{
    if (!loaded() || !m_otb || m_otb->isLoaded()) {
        if (error) *error = QStringLiteral("Open a client without items.otb first");
        return false;
    }
    const QString target = QDir(m_serverFolder.isEmpty() ? m_folder : m_serverFolder)
                               .filePath(QStringLiteral("items.otb"));
    if (QFileInfo::exists(target)) {
        if (error) *error = QStringLiteral("items.otb already exists: %1").arg(target);
        return false;
    }
    m_otb->newFile();
    m_otb->reloadItem(0);
    if (!m_otb->saveFile(target)) {
        if (error) *error = m_otb->errorString();
        return false;
    }
    m_otbPath = target;
    emit changed();
    return true;
}

bool ProjectModel::saveOtbAs(const QString &target, QString *error)
{
    if (!otbLoaded() || target.isEmpty() || !m_otb->saveFile(target)) {
        if (error) *error = m_otb ? m_otb->errorString() : QStringLiteral("No items.otb loaded");
        return false;
    }
    m_otbPath = target;
    emit changed();
    return true;
}

bool ProjectModel::saveItemsXmlAs(const QString &target, QString *error)
{
    if (!itemsXmlLoaded() || target.isEmpty() || !m_itemsXml->saveFile(target)) {
        if (error) *error = QStringLiteral("Could not save items.xml: %1").arg(target);
        return false;
    }
    m_itemsXmlPath = target;
    emit changed();
    return true;
}

bool ProjectModel::compileAs(const QString &folder, QString *error, const CompileProgress &progress)
{
    if (!loaded()) {
        if (error) *error = QStringLiteral("No project loaded");
        return false;
    }
    QDir target(folder);
    if (!target.exists() && !QDir().mkpath(target.absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create output folder: %1").arg(folder);
        return false;
    }
    const QString datTarget = target.filePath(m_datName);
    const QString sprTarget = target.filePath(m_sprName);
    const QString otbTarget = target.filePath(QStringLiteral("items.otb"));
    const QString xmlTarget = target.filePath(QStringLiteral("items.xml"));
    const QString otfiTarget = target.filePath(m_otfiPath.isEmpty()
                                                   ? QStringLiteral("Tibia.otfi")
                                                   : QFileInfo(m_otfiPath).fileName());

    if (m_otb && m_otb->isLoaded()) m_otb->syncNamesToItemsXml();
    if (progress) progress(5, QStringLiteral("Saving DAT"));
    if (!m_dat->saveFile(datTarget)) { if (error) *error = m_dat->errorString(); return false; }
    if (progress) progress(25, QStringLiteral("Saving sprites"));
    if (!m_sprites->saveFile(sprTarget, [&](int value) {
            if (progress) progress(25 + value * 60 / 100, QStringLiteral("Saving sprites"));
        })) { if (error) *error = m_sprites->errorString(); return false; }
    if (progress) progress(85, QStringLiteral("Saving server files"));
    if (m_otb && m_otb->isLoaded() && !m_otb->saveFile(otbTarget)) {
        if (error) *error = m_otb->errorString();
        return false;
    }
    if (m_itemsXml && m_itemsXml->hasData() && !m_itemsXml->saveFile(xmlTarget)) {
        if (error) *error = QStringLiteral("Cannot save items.xml");
        return false;
    }
    if (progress) progress(96, QStringLiteral("Finishing"));
    if (!writeOtfi(otfiTarget, error)) return false;

    m_folder = target.absolutePath();
    m_datPath = datTarget;
    m_sprPath = sprTarget;
    if (m_otb && m_otb->isLoaded()) m_otbPath = otbTarget;
    if (m_itemsXml && m_itemsXml->hasData()) m_itemsXmlPath = xmlTarget;
    m_otfiPath = otfiTarget;
    m_hasOtfi = true;
    m_otfiSizeModified = false;
    m_otfiGroupsModified = false;
    m_otfiDurationsModified = false;
    if (progress) progress(100, QStringLiteral("Complete"));
    emit changed();
    return true;
}

void ProjectModel::close()
{
    m_dat.reset();
    m_sprites.reset();
    m_otb.reset();
    m_itemsXml.reset();
    m_folder.clear();
    m_serverFolder.clear();
    m_datPath.clear();
    m_sprPath.clear();
    m_otbPath.clear();
    m_itemsXmlPath.clear();
    m_otfiPath.clear();
    m_otfiData.clear();
    m_otfiSizeModified = false;
    m_otfiGroupsModified = false;
    m_otfiDurationsModified = false;
    emit changed();
}
