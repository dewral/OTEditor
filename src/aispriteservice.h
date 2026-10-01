#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <functional>

class QNetworkReply;

// Owns one generation at a time. Credentials and downloaded files live only
// for this application session; settings never contain the API key.
class AiSpriteService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QUrl imageUrl READ imageUrl NOTIFY changed)
    Q_PROPERTY(QString jobId READ jobId NOTIFY changed)
    Q_PROPERTY(bool canRetrySubmission READ canRetrySubmission NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
public:
    struct Timing { int pollMs = 3000; int deadlineMs = 600000; int requestMs = 30000; };
    explicit AiSpriteService(QObject *parent = nullptr);
    AiSpriteService(Timing timing, bool persistSettings, QObject *parent = nullptr);
    ~AiSpriteService() override;
    QString state() const { return m_state; }
    bool busy() const;
    QString errorString() const { return m_error; }
    int progress() const { return m_progress; }
    QUrl imageUrl() const { return m_imageUrl; }
    QString jobId() const { return m_jobId; }
    bool canRetrySubmission() const { return m_uncertainSubmission && !busy() && !m_cooldown.isActive(); }
    QVariantMap settings() const { return m_settings; }
    Q_INVOKABLE bool configure(const QVariantMap &settings);
    Q_INVOKABLE void generate(const QString &prompt, const QString &modelId, const QString &apiKey);
    Q_INVOKABLE void checkResult(const QString &apiKey);
    Q_INVOKABLE void retrySubmission(const QString &apiKey);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool savePng(const QUrl &destination);
signals:
    void changed();
    void settingsChanged();
private:
    using Handler = std::function<void(const QByteArray &)>;
    void request(const QUrl &url, const QByteArray &body, bool image, Handler handler, int redirects = 0);
    void processJob(const QByteArray &payload);
    void poll();
    void receiveImage(const QByteArray &payload);
    void beginWait();
    void fail(const QString &message);
    void stopRequests();
    void setState(const QString &state);
    bool validateSettings(const QVariantMap &settings, QString &error) const;
    bool validUrl(const QUrl &url) const;
    bool sameOrigin(const QUrl &a, const QUrl &b) const;
    QString safeMessage(QString message) const;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_pollTimer, m_deadlineTimer;
    QTimer m_cooldown;
    Timing m_timing;
    bool m_persistSettings;
    QVariantMap m_settings;
    QTemporaryDir m_results;
    QString m_state = "idle", m_error, m_jobId, m_key;
    QByteArray m_submissionBody, m_idempotencyKey;
    bool m_uncertainSubmission = false;
    quint64 m_epoch = 0;
    int m_progress = 0;
    QUrl m_imageUrl, m_apiUrl;
};
