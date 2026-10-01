#include "aispriteservice.h"

#include <QBuffer>
#include <QFile>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QUuid>
#include <memory>

namespace {
constexpr qint64 MaxImageBytes = 32 * 1024 * 1024;
constexpr qint64 MaxJsonBytes = 1024 * 1024;
constexpr qint64 MaxPixels = 16000000;
QVariantMap defaults() {
    return {{"generationUrl", "https://aispritestudio.com/api/v1/generations"},
            {"authHeader", "Authorization"}, {"authPrefix", "Bearer "},
            {"statusUrl", "https://aispritestudio.com/api/v1/generations/{jobId}"},
            {"modelId", "qwen21-midhem-256"}};
}
void applyProviderAuthentication(QVariantMap &settings) {
    const QUrl url(settings.value("generationUrl").toString());
    if (url.scheme() == "https" && url.host().compare("aispritestudio.com", Qt::CaseInsensitive) == 0) {
        settings["authHeader"] = "Authorization";
        settings["authPrefix"] = "Bearer ";
    }
}
}

AiSpriteService::AiSpriteService(QObject *parent) : AiSpriteService(Timing{}, true, parent) {}

AiSpriteService::AiSpriteService(Timing timing, bool persistSettings, QObject *parent)
    : QObject(parent), m_timing(timing), m_persistSettings(persistSettings), m_settings(defaults()) {
    if (m_persistSettings) {
        QSettings stored;
        stored.beginGroup("aiSpriteGenerator");
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it)
            it.value() = stored.value(it.key(), it.value());
        // Upgrade the previous release's empty official-provider defaults.
        if (m_settings.value("generationUrl") == defaults().value("generationUrl")) {
            if (m_settings.value("authHeader").toString().isEmpty()) {
                m_settings["authHeader"] = "Authorization";
                m_settings["authPrefix"] = "Bearer ";
            }
            if (m_settings.value("statusUrl").toString().isEmpty())
                m_settings["statusUrl"] = defaults().value("statusUrl");
        }
    }
    applyProviderAuthentication(m_settings);
    m_pollTimer.setSingleShot(true);
    m_deadlineTimer.setSingleShot(true);
    m_cooldown.setSingleShot(true);
    connect(&m_cooldown, &QTimer::timeout, this, &AiSpriteService::changed);
    connect(&m_pollTimer, &QTimer::timeout, this, &AiSpriteService::poll);
    connect(&m_deadlineTimer, &QTimer::timeout, this, [this] {
        if (m_state == "submitting") m_uncertainSubmission = true;
        fail(m_jobId.isEmpty()
             ? "Submission timed out. Use Retry Submission to recover it without a second charge."
             : "Generation timed out. The service may still finish the job; use Check Result to resume.");
    });
}

AiSpriteService::~AiSpriteService() { stopRequests(); }

bool AiSpriteService::busy() const {
    return m_state == "submitting" || m_state == "waiting" || m_state == "downloading";
}

bool AiSpriteService::validUrl(const QUrl &url) const {
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) return false;
    // HTTP is restricted to loopback, allowing offline tests and local adapters.
    return url.scheme() == "https" || (url.scheme() == "http"
        && (url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1"));
}

bool AiSpriteService::sameOrigin(const QUrl &a, const QUrl &b) const {
    return a.scheme() == b.scheme() && a.host().compare(b.host(), Qt::CaseInsensitive) == 0
        && a.port(a.scheme() == "https" ? 443 : 80) == b.port(b.scheme() == "https" ? 443 : 80);
}

bool AiSpriteService::validateSettings(const QVariantMap &settings, QString &error) const {
    const QUrl api(settings.value("generationUrl").toString());
    if (!validUrl(api)) { error = "Enter a valid HTTPS generation URL."; return false; }
    const QString header = settings.value("authHeader").toString();
    static const QRegularExpression headerName("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$");
    if (!header.isEmpty() && (!headerName.match(header).hasMatch()
        || QStringList{"host", "content-length", "content-type", "connection", "transfer-encoding"}.contains(header.toLower()))) {
        error = "Enter a valid authentication header name."; return false;
    }
    const QString prefix = settings.value("authPrefix").toString();
    if (prefix.contains('\r') || prefix.contains('\n')) { error = "The key prefix cannot contain line breaks."; return false; }
    const QString status = settings.value("statusUrl").toString();
    if (!status.isEmpty()) {
        QString sample = status;
        sample.replace("{jobId}", "example-job");
        const QUrl url(sample);
        if (!status.contains("{jobId}") || !validUrl(url) || !sameOrigin(url, api)) {
            error = "The status URL must use {jobId} and the same server as the generation URL."; return false;
        }
    }
    return true;
}

bool AiSpriteService::configure(const QVariantMap &values) {
    if (busy()) return false;
    QVariantMap next = defaults();
    for (auto it = next.begin(); it != next.end(); ++it)
        it.value() = values.value(it.key(), m_settings.value(it.key()));
    applyProviderAuthentication(next);
    QString error;
    if (!validateSettings(next, error)) { fail(error); return false; }
    // A job belongs to the server that accepted it, never to a newly entered URL.
    if (next.value("generationUrl") != m_settings.value("generationUrl")) {
        m_jobId.clear();
        m_uncertainSubmission = false;
        m_submissionBody.clear();
        m_idempotencyKey.clear();
    }
    m_settings = next;
    m_error.clear();
    if (m_state == "error") m_state = "idle";
    if (m_persistSettings) {
        QSettings stored;
        stored.beginGroup("aiSpriteGenerator");
        for (auto it = next.cbegin(); it != next.cend(); ++it) stored.setValue(it.key(), it.value());
    }
    emit settingsChanged();
    emit changed();
    return true;
}

void AiSpriteService::beginWait() {
    m_error.clear();
    m_deadlineTimer.start(m_timing.deadlineMs);
}

void AiSpriteService::generate(const QString &prompt, const QString &modelId, const QString &apiKey) {
    if (busy()) return;
    if (m_cooldown.isActive()) { fail("The service rate limit is active. Wait before generating again."); return; }
    QString error;
    if (!validateSettings(m_settings, error)) { fail(error); return; }
    if (prompt.trimmed().isEmpty() || modelId.trimmed().isEmpty()) { fail("Enter a prompt and model ID."); return; }
    if (prompt.trimmed().toUcs4().size() > 2000) { fail("The prompt must contain 1–2000 characters."); return; }
    if (m_settings.value("authHeader").toString().isEmpty() || apiKey.isEmpty()) {
        fail("Set the authentication header in API Settings and enter your API key."); return;
    }
    if (apiKey.contains('\r') || apiKey.contains('\n')) { fail("The API key cannot contain line breaks."); return; }
    m_apiUrl = QUrl(m_settings.value("generationUrl").toString());
    m_key = apiKey;
    m_jobId.clear();
    m_progress = 0;
    m_uncertainSubmission = false;
    m_idempotencyKey = QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1();
    m_submissionBody = QJsonDocument(QJsonObject{{"modelId", modelId.trimmed()}, {"prompt", prompt.trimmed()}}).toJson(QJsonDocument::Compact);
    if (m_submissionBody.size() > 16 * 1024) { fail("The request body exceeds the API limit of 16 KiB."); return; }
    beginWait();
    setState("submitting");
    request(m_apiUrl, m_submissionBody, false, [this](const QByteArray &payload) { processJob(payload); });
}

void AiSpriteService::retrySubmission(const QString &apiKey) {
    if (!canRetrySubmission()) return;
    if (apiKey.isEmpty() || apiKey.contains('\r') || apiKey.contains('\n')) { fail("Enter a valid API key."); return; }
    m_key = apiKey;
    beginWait();
    setState("submitting");
    request(m_apiUrl, m_submissionBody, false, [this](const QByteArray &payload) { processJob(payload); });
}

void AiSpriteService::checkResult(const QString &apiKey) {
    if (busy() || m_jobId.isEmpty()) return;
    if (m_cooldown.isActive()) { fail("The service rate limit is active. Wait before checking again."); return; }
    if (apiKey.isEmpty() || apiKey.contains('\r') || apiKey.contains('\n')) { fail("Enter a valid API key."); return; }
    m_key = apiKey;
    beginWait();
    setState("waiting");
    poll();
}

QString AiSpriteService::safeMessage(QString message) const {
    if (!m_key.isEmpty()) message.replace(m_key, "[hidden]");
    return message.left(500);
}

void AiSpriteService::request(const QUrl &url, const QByteArray &body, bool image, Handler handler, int redirects) {
    if (!validUrl(url)) { fail("The API returned an invalid or insecure URL."); return; }
    QNetworkRequest req(url);
    req.setTransferTimeout(m_timing.requestMs);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Accept", image ? "image/png" : "application/json");
    if (sameOrigin(url, m_apiUrl)) {
        req.setRawHeader(m_settings.value("authHeader").toString().toLatin1(),
                         (m_settings.value("authPrefix").toString() + m_key).toUtf8());
    }
    QNetworkReply *reply;
    if (!body.isEmpty()) {
        req.setRawHeader("Idempotency-Key", m_idempotencyKey);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        reply = m_network.post(req, body);
    } else reply = m_network.get(req);
    m_reply = reply;
    const qint64 limit = image ? MaxImageBytes : MaxJsonBytes;
    auto data = std::make_shared<QByteArray>();
    reply->setReadBufferSize(64 * 1024);
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, reply, limit] {
        if (m_reply == reply && reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > limit)
            fail("The API response exceeds the download size limit.");
    });
    connect(reply, &QIODevice::readyRead, this, [this, reply, data, limit] {
        if (m_reply != reply) return;
        if (reply->isOpen()) data->append(reply->readAll());
        if (data->size() > limit) fail("The API response exceeds the download size limit.");
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, data, limit, url, body, image, handler, redirects] {
        reply->deleteLater();
        if (m_reply != reply) return;
        m_reply.clear();
        if (reply->isOpen()) data->append(reply->readAll());
        if (data->size() > limit) { fail("The API response exceeds the download size limit."); return; }
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code >= 300 && code < 400) {
            // Never replay a charged POST or send credentials across origins.
            const QUrl next = url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            if (!body.isEmpty() || redirects >= 5 || (!image && !sameOrigin(next, m_apiUrl))) {
                fail("The API redirected the request. Check the configured endpoint."); return;
            }
            request(next, {}, image, handler, redirects + 1);
            return;
        }
        if (reply->error() != QNetworkReply::NoError || code < 200 || code >= 300) {
            QString message;
            const auto apiError = QJsonDocument::fromJson(*data).object();
            const QString detail = apiError.value("message").toString();
            if (!body.isEmpty()) m_uncertainSubmission = code == 0 || code == 429 || code >= 500;
            if (code == 401 || code == 403) message = "API access denied. Check the API key and authentication settings.";
            else if (code == 402) message = "The service requires more credits for this generation.";
            else if (code == 409) message = "Idempotency conflict. The service rejected this request; start a new generation.";
            else if (code == 429) {
                const QByteArray header = reply->rawHeader("Retry-After");
                bool numeric = false;
                qint64 seconds = header.toLongLong(&numeric);
                if (!numeric) {
                    const QDateTime date = QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date);
                    seconds = date.isValid() ? QDateTime::currentDateTimeUtc().secsTo(date) : 60;
                }
                seconds = qBound(qint64(1), seconds, qint64(86400));
                const int delay = int(seconds * 1000);
                m_cooldown.start(delay);
                message = QString("The service rate limit was reached. Retry after %1 seconds.").arg(seconds);
                if (body.isEmpty()) {
                    m_error = message;
                    emit changed();
                    const quint64 epoch = m_epoch;
                    QTimer::singleShot(delay, this, [this, epoch, url, image, handler] {
                        if (busy() && m_epoch == epoch) {
                            m_error.clear(); emit changed();
                            request(url, {}, image, handler);
                        }
                    });
                    return;
                }
            }
            else message = QString("API request failed (HTTP %1): %2").arg(code).arg(reply->errorString());
            if (!detail.isEmpty()) message += " " + detail;
            fail(safeMessage(message));
            return;
        }
        handler(*data);
    });
}

void AiSpriteService::processJob(const QByteArray &payload) {
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        if (m_state == "submitting") m_uncertainSubmission = true;
        fail("The API returned invalid JSON."); return;
    }
    const auto job = doc.object();
    const QString id = job.value("jobId").toString();
    if (!id.isEmpty()) {
        if (!m_jobId.isEmpty() && m_jobId != id) { fail("The API returned a different job ID."); return; }
        m_jobId = id;
        m_uncertainSubmission = false;
    }
    m_progress = qBound(0, job.value("progress").toInt(), 100);
    const QString status = job.value("status").toString().toLower();
    if (status == "failed" || status == "error" || status == "cancelled" || status == "canceled") {
        fail("The service could not complete the generation."); return;
    }
    if (status == "completed") {
        const QString imagePath = job.value("imageUrl").toString();
        if (imagePath.isEmpty()) { fail("The completed generation has no image URL."); return; }
        setState("downloading");
        request(m_apiUrl.resolved(QUrl(imagePath)), {}, true,
                [this](const QByteArray &data) { receiveImage(data); });
        return;
    }
    if (m_jobId.isEmpty() || status.isEmpty()) {
        if (m_state == "submitting" && m_jobId.isEmpty()) m_uncertainSubmission = true;
        fail("The API response is missing a job ID or status."); return;
    }
    if (m_settings.value("statusUrl").toString().isEmpty()) {
        fail("This job is still running. Set the status URL with {jobId} in API Settings, then use Check Result."); return;
    }
    setState("waiting");
    m_pollTimer.start(m_timing.pollMs);
}

void AiSpriteService::poll() {
    QString status = m_settings.value("statusUrl").toString();
    if (status.isEmpty()) { fail("Set the status URL with {jobId} in API Settings."); return; }
    status.replace("{jobId}", QString::fromLatin1(QUrl::toPercentEncoding(m_jobId)));
    const QUrl url(status);
    if (!validUrl(url) || !sameOrigin(url, m_apiUrl)) { fail("The status URL must use the same server as the generation URL."); return; }
    request(url, {}, false, [this](const QByteArray &data) { processJob(data); });
}

void AiSpriteService::receiveImage(const QByteArray &payload) {
    QBuffer buffer;
    buffer.setData(payload);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "PNG");
    const QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > MaxPixels) {
        fail("The generated image is invalid or exceeds 16 million pixels."); return;
    }
    const QImage image = reader.read();
    if (image.isNull()) { fail("The downloaded image is not a valid PNG."); return; }
    if (!m_results.isValid()) { fail("Cannot create the temporary image folder."); return; }
    const QString path = m_results.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
        fail("Cannot write the generated PNG."); return;
    }
    m_imageUrl = QUrl::fromLocalFile(path);
    m_progress = 100;
    m_deadlineTimer.stop();
    setState("completed");
}

bool AiSpriteService::savePng(const QUrl &destination) {
    if (m_imageUrl.isEmpty() || !destination.isLocalFile()) return false;
    QFile source(m_imageUrl.toLocalFile());
    QSaveFile output(destination.toLocalFile());
    if (!source.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        m_error = "Cannot open the destination file."; emit changed(); return false;
    }
    const QByteArray bytes = source.readAll();
    if (output.write(bytes) != bytes.size() || !output.commit()) {
        m_error = "Cannot save the PNG."; emit changed(); return false;
    }
    m_error.clear(); emit changed(); return true;
}

void AiSpriteService::stopRequests() {
    ++m_epoch;
    m_pollTimer.stop();
    m_deadlineTimer.stop();
    if (m_reply) {
        auto *reply = m_reply.data();
        m_reply.clear();
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}

void AiSpriteService::fail(const QString &message) {
    stopRequests();
    m_error = safeMessage(message);
    setState("error");
}

void AiSpriteService::cancel() {
    if (!busy()) return;
    if (m_state == "submitting") m_uncertainSubmission = true;
    stopRequests();
    m_error.clear();
    setState("cancelled");
}

void AiSpriteService::setState(const QString &state) { m_state = state; emit changed(); }
