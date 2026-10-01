#include "aispriteservice.h"
#include "editorbackend.h"
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

namespace {
struct Request { QByteArray method, path, headers, body; };
struct Response { int code = 200; QByteArray body; QByteArray headers; bool hold = false; };

// Small HTTP fixture: no external requests or service credits are used.
class Server : public QTcpServer {
public:
    QList<Request> requests;
    std::function<Response(const Request &)> handler;
    Server() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                auto input = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket, input] {
                    input->append(socket->readAll());
                    const auto boundary = input->indexOf("\r\n\r\n");
                    if (boundary < 0) return;
                    const QByteArray header = input->left(boundary);
                    int length = 0;
                    for (const auto &line : header.split('\n'))
                        if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                    if (input->size() < boundary + 4 + length) return;
                    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
                    const auto first = header.split('\n').first().trimmed().split(' ');
                    Request request{first.value(0), first.value(1), header, input->mid(boundary + 4, length)};
                    requests.append(request);
                    const Response result = handler ? handler(request) : Response{404, "missing"};
                    if (result.hold) return;
                    QByteArray response = "HTTP/1.1 " + QByteArray::number(result.code) + " Test\r\nConnection: close\r\n";
                    response += result.headers;
                    if (!result.headers.toLower().contains("content-length:"))
                        response += "Content-Length: " + QByteArray::number(result.body.size()) + "\r\n";
                    response += "\r\n" + result.body;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QString url(const QString &path = "/generations") const { return QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path); }
};

QByteArray png() {
    QImage image(2, 2, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    image.setPixelColor(1, 1, QColor(20, 80, 150, 180));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
QByteArray job(const QString &status, const QString &image = "/image") {
    return QJsonDocument(QJsonObject{{"jobId", "test-job"}, {"status", status}, {"progress", 75}, {"imageUrl", image}}).toJson();
}
bool configure(AiSpriteService &service, Server &server, bool status = true) {
    return service.configure({{"generationUrl", server.url()}, {"authHeader", "X-API-Key"}, {"authPrefix", ""},
                              {"statusUrl", status ? server.url("/jobs/{jobId}") : QString()}});
}
void generate(AiSpriteService &service) { service.generate("emerald dragon", "qwen21-midhem-256", "test-secret"); }
QByteArray headerValue(const Request &request, const QByteArray &name) {
    for (const auto &line : request.headers.split('\n'))
        if (line.toLower().startsWith(name.toLower() + ':')) return line.mid(name.size() + 1).trimmed();
    return {};
}
}

class AiSpriteTests : public QObject {
    Q_OBJECT
private slots:
    void documentedDefaultsAnd202() {
        Server server;
        server.handler = [](const Request &req) {
            if (req.method == "POST") return Response{202, "{\"jobId\":\"test-job\",\"status\":\"pending\",\"creditsDeducted\":4}"};
            if (req.path.endsWith("/image")) return Response{200, png()};
            return Response{200, job("completed", "/generations/test-job/image")};
        };
        AiSpriteService service({10, 3000, 1000}, false);
        QCOMPARE(service.settings().value("authHeader").toString(), QString("Authorization"));
        QCOMPARE(service.settings().value("authPrefix").toString(), QString("Bearer "));
        QCOMPARE(service.settings().value("statusUrl").toString(), QString("https://aispritestudio.com/api/v1/generations/{jobId}"));
        QCOMPARE(AiSpriteService::Timing{}.pollMs, 3000);
        QVERIFY(service.configure({{"generationUrl", server.url()}, {"statusUrl", server.url("/generations/{jobId}")}}));
        service.generate("a crystal sword", "tibia-style-items-1", "test-secret");
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(server.requests.size(), 3);
        for (const auto &request : server.requests)
            QCOMPARE(headerValue(request, "Authorization"), QByteArray("Bearer test-secret"));
        const auto body = QJsonDocument::fromJson(server.requests.first().body).object();
        QCOMPARE(body.size(), 2);
        QCOMPARE(body.value("modelId").toString(), QString("tibia-style-items-1"));
    }
    void promptAndBodyLimits() {
        Server server;
        server.handler = [](const Request &) { return Response{402, "{}"}; };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server));
        service.generate(QString(2001, 'x'), "tibia-style-items-1", "test-secret");
        QCOMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("2000"));
        QCOMPARE(server.requests.size(), 0);
        service.generate("sword", QString(17000, 'x'), "test-secret");
        QVERIFY(service.errorString().contains("16 KiB"));
        QCOMPARE(server.requests.size(), 0);
        service.generate(QString(2000, 'x'), "tibia-style-items-1", "test-secret");
        QTRY_COMPARE(server.requests.size(), 1);
    }
    void retrySubmissionPreservesIdempotency() {
        Server server;
        int posts = 0;
        server.handler = [&posts](const Request &req) {
            if (req.method == "POST") {
                if (++posts == 1) return Response{200, {}, {}, true};
                return Response{202, job("queued")};
            }
            return Response{200, req.path == "/image" ? png() : job("completed")};
        };
        AiSpriteService service({10, 3000, 60}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.canRetrySubmission());
        const QByteArray originalKey = headerValue(server.requests.first(), "Idempotency-Key");
        QVERIFY(!originalKey.isEmpty());
        QVERIFY(QRegularExpression("^[A-Za-z0-9_-]{1,128}$").match(QString::fromLatin1(originalKey)).hasMatch());
        service.retrySubmission("rotated-key");
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(server.requests.at(1).body, server.requests.first().body);
        QCOMPARE(headerValue(server.requests.at(1), "Idempotency-Key"), originalKey);
        QCOMPARE(headerValue(server.requests.at(1), "X-API-Key"), QByteArray("rotated-key"));
        QVERIFY(!service.canRetrySubmission());
        generate(service);
        QTRY_COMPARE(posts, 3);
        const auto it = std::find_if(server.requests.crbegin(), server.requests.crend(), [](const Request &req) { return req.method == "POST"; });
        QVERIFY(headerValue(*it, "Idempotency-Key") != originalKey);
    }
    void rateLimitRespectsRetryAfter() {
        Server server;
        int checks = 0;
        server.handler = [&checks](const Request &req) {
            if (req.method == "POST") return Response{202, job("queued")};
            if (req.path == "/image") return Response{200, png()};
            if (++checks == 1) return Response{429, "{\"error\":\"RATE_LIMIT\",\"message\":\"Slow down\"}", "Retry-After: 1\r\n"};
            return Response{200, job("completed")};
        };
        AiSpriteService service({10, 5000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(checks, 1);
        QTest::qWait(400);
        QCOMPARE(checks, 1);
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QString("completed"), 3000);
        QCOMPARE(checks, 2);
        QCOMPARE(std::count_if(server.requests.begin(), server.requests.end(), [](const Request &req) { return req.method == "POST"; }), 1);
    }
    void submissionRateLimitAndApiMessage() {
        Server server;
        int posts = 0;
        server.handler = [&posts](const Request &req) {
            if (req.method == "POST" && ++posts == 1)
                return Response{429, "{\"error\":\"RATE_LIMIT\",\"message\":\"Account limit reached\"}", "Retry-After: 1\r\n"};
            return Response{200, req.method == "POST" ? job("completed") : png()};
        };
        AiSpriteService service({10, 5000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("Account limit reached"));
        QVERIFY(!service.canRetrySubmission());
        service.retrySubmission("test-secret");
        QCOMPARE(posts, 1);
        QTRY_VERIFY(service.canRetrySubmission());
        service.retrySubmission("test-secret");
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(headerValue(server.requests.at(0), "Idempotency-Key"), headerValue(server.requests.at(1), "Idempotency-Key"));
    }
    void immediateResultAndSlicer() {
        Server server;
        server.handler = [](const Request &req) { return Response{200, req.method == "POST" ? job("completed") : png()}; };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server));
        generate(service);
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(server.requests.first().headers.toLower().contains("x-api-key: test-secret"));
        const auto body = QJsonDocument::fromJson(server.requests.first().body).object();
        QCOMPARE(body.value("modelId").toString(), QString("qwen21-midhem-256"));
        QCOMPARE(body.value("prompt").toString(), QString("emerald dragon"));
        QCOMPARE(server.requests.last().path, QByteArray("/image"));
        const QImage image(service.imageUrl().toLocalFile());
        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(image.pixelColor(1, 1).alpha(), 180);
        QTemporaryDir dir;
        QVERIFY(service.savePng(QUrl::fromLocalFile(dir.filePath("saved.png"))));
        QCOMPARE(QImage(dir.filePath("saved.png")), image);
        EditorBackend backend;
        QVERIFY(!backend.loaded());
        QVERIFY(backend.slicerOpen(service.imageUrl().toString()));
        QCOMPARE(backend.slicerWidth(), 2);
        QCOMPARE(backend.slicerHeight(), 2);
    }
    void pollUntilCompleted() {
        Server server;
        int polls = 0;
        server.handler = [&polls](const Request &req) {
            if (req.method == "POST") return Response{200, job("queued")};
            if (req.path == "/image") return Response{200, png()};
            return Response{200, job(++polls == 1 ? "running" : "completed")};
        };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(polls, 2);
        QCOMPARE(server.requests.first().method, QByteArray("POST"));
        QCOMPARE(server.requests.at(1).path, QByteArray("/jobs/test-job"));
        QCOMPARE(std::count_if(server.requests.begin(), server.requests.end(), [](const Request &r) { return r.method == "POST"; }), 1);
    }
    void missingStatusCanResume() {
        Server server;
        server.handler = [](const Request &req) {
            return Response{200, req.method == "POST" ? job("queued") : req.path == "/image" ? png() : job("completed")};
        };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server, false)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("status URL"));
        QCOMPARE(service.jobId(), QString("test-job"));
        QVERIFY(configure(service, server)); service.checkResult("test-secret");
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(server.requests.size(), 3);
    }
    void apiErrors_data() {
        QTest::addColumn<int>("code"); QTest::addColumn<QByteArray>("body"); QTest::addColumn<QString>("message");
        QTest::newRow("unauthorized") << 401 << QByteArray("denied") << QString("access denied");
        QTest::newRow("no-credits") << 402 << QByteArray("credits") << QString("credits");
        QTest::newRow("malformed-json") << 200 << QByteArray("not-json") << QString("invalid JSON");
        QTest::newRow("failed-job") << 200 << job("failed") << QString("could not complete");
        QTest::newRow("incomplete") << 200 << QByteArray("{}") << QString("missing a job");
    }
    void apiErrors() {
        QFETCH(int, code); QFETCH(QByteArray, body); QFETCH(QString, message);
        Server server; server.handler = [code, body](const Request &) { return Response{code, body}; };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY2(service.errorString().contains(message), qPrintable(service.errorString()));
        QCOMPARE(server.requests.size(), 1);
    }
    void timeoutAndCancel() {
        Server server; server.handler = [](const Request &) { return Response{200, {}, {}, true}; };
        AiSpriteService service({10, 100, 3000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("timed out"));
        generate(service);
        QTRY_COMPARE(server.requests.size(), 2);
        service.cancel();
        QCOMPARE(service.state(), QString("cancelled"));
        QVERIFY(!service.busy());
        QTest::qWait(130);
        QCOMPARE(service.state(), QString("cancelled"));
    }
    void requestTimeout() {
        Server server; server.handler = [](const Request &) { return Response{200, {}, {}, true}; };
        AiSpriteService service({10, 5000, 50}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QString("error"), 3000);
        QCOMPARE(server.requests.size(), 1);
    }
    void redirectsDoNotLeakKeyOrReplayPost() {
        Server external, server;
        external.handler = [](const Request &) { return Response{200, png()}; };
        server.handler = [&external](const Request &req) {
            if (req.method == "POST") return Response{200, job("completed")};
            return Response{302, {}, "Location: " + external.url("/image").toUtf8() + "\r\n"};
        };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("completed"));
        QCOMPARE(external.requests.size(), 1);
        QVERIFY(!external.requests.first().headers.contains("test-secret"));
        server.handler = [&external](const Request &) { return Response{307, {}, "Location: " + external.url().toUtf8() + "\r\n"}; };
        generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QCOMPARE(external.requests.size(), 1);
    }
    void invalidImageAndDownloadLimit() {
        Server server;
        QByteArray image = "broken-png";
        QByteArray headers;
        server.handler = [&image, &headers](const Request &req) {
            return req.method == "POST" ? Response{200, job("completed")} : Response{200, image, headers};
        };
        AiSpriteService service({10, 5000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.imageUrl().isEmpty());
        headers = "Content-Length: 33554433\r\n";
        generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("size limit"));
        headers.clear(); image = QByteArray(33554433, 'a');
        generate(service);
        QTRY_COMPARE_WITH_TIMEOUT(service.state(), QString("error"), 10000);
        QVERIFY(service.errorString().contains("size limit"));
    }
    void oversizedImage() {
        // Valid IHDR CRC declares a huge image without allocating its pixels.
        QByteArray image = png();
        image.replace(16, 8, QByteArray::fromHex("0000138800001388")); // 5000 x 5000
        quint32 crc = 0xffffffffu;
        for (unsigned char value : image.mid(12, 17)) {
            crc ^= value;
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
        }
        crc ^= 0xffffffffu;
        QByteArray encoded;
        for (int shift = 24; shift >= 0; shift -= 8) encoded.append(char(crc >> shift));
        image.replace(29, 4, encoded);
        Server server; server.handler = [image](const Request &req) { return Response{200, req.method == "POST" ? job("completed") : image}; };
        AiSpriteService service({10, 3000, 1000}, false);
        QVERIFY(configure(service, server)); generate(service);
        QTRY_COMPARE(service.state(), QString("error"));
        QVERIFY(service.errorString().contains("16 million"));
    }
    void settingsValidationAndPersistence() {
        QTemporaryDir dir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
        QCoreApplication::setOrganizationName("OTEditorTests");
        QCoreApplication::setApplicationName("AiSpriteTests");
        Server server; server.handler = [](const Request &) { return Response{401, {}}; };
        {
            QSettings legacy;
            legacy.beginGroup("aiSpriteGenerator");
            legacy.setValue("authHeader", "Authorization"); legacy.setValue("authPrefix", ""); legacy.setValue("statusUrl", "");
        }
        {
            AiSpriteService service({10, 3000, 1000}, true);
            QCOMPARE(service.settings().value("authHeader").toString(), QString("Authorization"));
            QCOMPARE(service.settings().value("authPrefix").toString(), QString("Bearer "));
            QVERIFY(!service.settings().value("statusUrl").toString().isEmpty());
            QVERIFY(service.configure({{"authHeader", ""}, {"authPrefix", ""}}));
            QCOMPARE(service.settings().value("authHeader").toString(), QString("Authorization"));
            QCOMPARE(service.settings().value("authPrefix").toString(), QString("Bearer "));
            QVERIFY(!service.configure({{"generationUrl", server.url()}, {"statusUrl", server.url() + "/{jobId}"}, {"authHeader", "X-Key\r\nInjected"}}));
            QVERIFY(!service.configure({{"generationUrl", "http://example.com"}}));
            QVERIFY(!service.configure({{"statusUrl", "https://example.com/{jobId}"}}));
            QVERIFY(configure(service, server)); generate(service);
            QTRY_COMPARE(service.state(), QString("error"));
        }
        QSettings stored; stored.sync();
        for (const auto &key : stored.allKeys()) QVERIFY(!stored.value(key).toString().contains("test-secret"));
        AiSpriteService restored({10, 3000, 1000}, true);
        QCOMPARE(restored.settings().value("authHeader").toString(), QString("X-API-Key"));
        QCOMPARE(restored.settings().value("generationUrl").toString(), server.url());
    }
};

QTEST_GUILESS_MAIN(AiSpriteTests)
#include "ai_sprite_tests.moc"
