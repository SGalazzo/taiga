#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTest>
#include <QtEndian>

#include "share/discord_ipc.hpp"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;
using share::discord::ipc::Client;

// Ported from our feat/discord-presence tests to the API of erengy/taiga#1281.
namespace {

struct Frame {
  qint32 opcode;
  QJsonObject json;
  QByteArray raw;
};

QByteArray encode(qint32 opcode, const QByteArray& payload) {
  QByteArray frame(8, Qt::Uninitialized);
  qToLittleEndian<qint32>(opcode, frame.data());
  qToLittleEndian<qint32>(static_cast<qint32>(payload.size()), frame.data() + 4);
  return frame + payload;
}

QByteArray encode(qint32 opcode, const QJsonObject& json) {
  return encode(opcode, QJsonDocument{json}.toJson(QJsonDocument::Compact));
}

const QJsonObject kReady{{"cmd", "DISPATCH"}, {"evt", "READY"}, {"data", QJsonObject{{"v", 1}}}};

class FakeDiscord : public QObject {
public:
  explicit FakeDiscord(const QString& name) : name_{name} {
    connect(&server_, &QLocalServer::newConnection, this, [this]() {
      client_ = server_.nextPendingConnection();
      ++connections;
      connect(client_, &QLocalSocket::readyRead, this, [this]() { read(); });
    });
  }

  bool listen() {
    QLocalServer::removeServer(name_);
    return server_.listen(name_);
  }

  void send(const QByteArray& bytes) {
    client_->write(bytes);
    client_->flush();
  }
  void dropClient() { client_->disconnectFromServer(); }

  QList<Frame> frames;
  int connections = 0;

private:
  void read() {
    buffer_ += client_->readAll();
    while (buffer_.size() >= 8) {
      const auto opcode = qFromLittleEndian<qint32>(buffer_.constData());
      const auto length = qFromLittleEndian<qint32>(buffer_.constData() + 4);
      if (buffer_.size() < 8 + length) break;
      const auto raw = buffer_.mid(8, length);
      buffer_.remove(0, 8 + length);
      frames.push_back({opcode, QJsonDocument::fromJson(raw).object(), raw});
    }
  }

  QString name_;
  QLocalServer server_;
  QLocalSocket* client_ = nullptr;
  QByteArray buffer_;
};

QString uniquePrefix(const char* test) {
  return u"taiga-test-%1-%2-"_s.arg(QCoreApplication::applicationPid()).arg(test);
}

}  // namespace

class TestDiscordIpc : public QObject {
  Q_OBJECT

private slots:
  void handshakeThenCommandAfterReady() {
    const auto prefix = uniquePrefix("handshake");
    FakeDiscord discord{prefix + "2"};  // client must scan past 0 and 1
    QVERIFY(discord.listen());

    Client client{"1234", nullptr, prefix, 500ms};
    client.open();

    QTRY_COMPARE(discord.frames.size(), 1);
    QCOMPARE(discord.frames[0].opcode, 0);
    QCOMPARE(discord.frames[0].json["v"].toInt(), 1);
    QCOMPARE(discord.frames[0].json["client_id"].toString(), u"1234"_s);

    client.sendCommand(u"SET_ACTIVITY"_s, {});  // dropped: not ready
    QTest::qWait(50);
    QCOMPARE(discord.frames.size(), 1);

    QSignalSpy ready(&client, &Client::ready);
    discord.send(encode(1, kReady));
    QTRY_COMPARE(ready.count(), 1);
    QVERIFY(client.isReady());

    const QJsonObject args{{"pid", 42}};
    client.sendCommand(u"SET_ACTIVITY"_s, args);
    client.sendCommand(u"SET_ACTIVITY"_s, args);
    QTRY_COMPARE(discord.frames.size(), 3);
    const auto& command = discord.frames[1].json;
    QCOMPARE(discord.frames[1].opcode, 1);
    QCOMPARE(command["cmd"].toString(), u"SET_ACTIVITY"_s);
    QCOMPARE(command["args"].toObject(), args);
    QVERIFY(!command["nonce"].toString().isEmpty());
    QVERIFY(discord.frames[1].json["nonce"] != discord.frames[2].json["nonce"]);
  }

  void pingIsAnsweredWithPong() {
    const auto prefix = uniquePrefix("ping");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 500ms};
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);

    discord.send(encode(3, QByteArray{"{\"x\":42}"}));
    QTRY_COMPARE(discord.frames.size(), 2);
    QCOMPARE(discord.frames[1].opcode, 4);
    QCOMPARE(discord.frames[1].json, (QJsonObject{{"x", 42}}));
  }

  void splitAndCoalescedFrames() {
    const auto prefix = uniquePrefix("split");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 500ms};
    QSignalSpy ready(&client, &Client::ready);
    QSignalSpy errors(&client, &Client::errorReceived);
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);

    const auto readyFrame = encode(1, kReady);
    discord.send(readyFrame.left(5));
    QTest::qWait(30);
    QCOMPARE(ready.count(), 0);
    discord.send(readyFrame.mid(5));
    QTRY_COMPARE(ready.count(), 1);

    const QJsonObject error{
        {"cmd", "SET_ACTIVITY"},
        {"evt", "ERROR"},
        {"data", QJsonObject{{"code", 4000}, {"message", "bad activity"}}}};
    discord.send(encode(1, error) + encode(1, error));
    QTRY_COMPARE(errors.count(), 2);
    QCOMPARE(errors[0][0].toInt(), 4000);
    QCOMPARE(errors[0][1].toString(), u"bad activity"_s);
    QVERIFY(client.isReady());  // an error response doesn't end the connection
  }

  void connectsWhenDiscordStartsLater() {
    const auto prefix = uniquePrefix("later");
    Client client{"1234", nullptr, prefix, 50ms};
    client.open();
    QTest::qWait(120);  // a few failed attempts

    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    QTRY_VERIFY(discord.frames.size() >= 1);
    QCOMPARE(discord.frames[0].opcode, 0);
  }

  void reconnectsAfterDisconnect() {
    const auto prefix = uniquePrefix("reconnect");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 200ms};
    QSignalSpy ready(&client, &Client::ready);
    QSignalSpy disconnected(&client, &Client::disconnected);
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);
    discord.send(encode(1, kReady));
    QTRY_COMPARE(ready.count(), 1);

    discord.dropClient();
    QTRY_COMPARE(disconnected.count(), 1);
    QVERIFY(!client.isReady());
    QTRY_COMPARE(discord.connections, 2);
    QTRY_COMPARE(discord.frames.size(), 2);  // new handshake
    QCOMPARE(discord.frames[1].opcode, 0);
    discord.send(encode(1, kReady));
    QTRY_COMPARE(ready.count(), 2);
  }

  void invalidClientIdStopsRetryingUntilReopened() {
    const auto prefix = uniquePrefix("close");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"bad", nullptr, prefix, 200ms};
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);

    discord.send(encode(2, QJsonObject{{"code", 4000}, {"message", "Invalid Client ID"}}));
    QTest::qWait(600);
    QCOMPARE(discord.connections, 1);
    QVERIFY(!client.isReady());

    client.close();
    client.open();
    QTRY_COMPARE(discord.connections, 2);
  }

  void closeDisconnectsAndStopsRetrying() {
    const auto prefix = uniquePrefix("disable");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 200ms};
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);
    discord.send(encode(1, kReady));
    QTRY_VERIFY(client.isReady());

    client.close();
    QVERIFY(!client.isReady());
    QTest::qWait(500);
    QCOMPARE(discord.connections, 1);
  }

  void unansweredHandshakeIsRetried() {
    const auto prefix = uniquePrefix("timeout");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 50ms, 100ms};
    client.open();

    QTRY_VERIFY(discord.frames.size() >= 1);  // handshake, never answered
    QTRY_VERIFY(discord.connections >= 2);    // gave up and connected again
    QTRY_VERIFY(discord.frames.size() >= 2);
    QCOMPARE(discord.frames[1].opcode, 0);
  }

  void reopeningReconnectsImmediately() {
    const auto prefix = uniquePrefix("reopen");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 10s};  // long interval: no help from retries
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);
    discord.send(encode(1, kReady));
    QTRY_VERIFY(client.isReady());

    client.close();
    client.open();
    QTRY_COMPARE_WITH_TIMEOUT(discord.connections, 2, 2000);
    QTRY_COMPARE(discord.frames.size(), 2);
  }

  void reopeningWhileConnectingReconnects() {
    // Discord not running yet: close and reopen in quick succession (e.g. toggling sharing twice).
    const auto prefix = uniquePrefix("toggle");
    Client client{"1234", nullptr, prefix, 10s};
    client.open();
    client.close();
    client.open();
    QTest::qWait(50);

    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    client.close();
    client.open();
    QTRY_COMPARE_WITH_TIMEOUT(discord.frames.size(), 1, 2000);
  }

  void oversizedFrameDisconnects() {
    const auto prefix = uniquePrefix("oversized");
    FakeDiscord discord{prefix + "0"};
    QVERIFY(discord.listen());
    Client client{"1234", nullptr, prefix, 10s};
    QSignalSpy disconnected(&client, &Client::disconnected);
    client.open();
    QTRY_COMPARE(discord.frames.size(), 1);

    QByteArray header(8, Qt::Uninitialized);
    qToLittleEndian<qint32>(1, header.data());
    qToLittleEndian<qint32>(0x7FFFFFF0, header.data() + 4);
    discord.send(header);
    QTRY_COMPARE(disconnected.count(), 1);
    QVERIFY(!client.isReady());
  }
};

QTEST_GUILESS_MAIN(TestDiscordIpc)
#include "tst_discord_ipc.moc"
