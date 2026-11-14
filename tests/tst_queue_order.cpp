#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>

#include "media/anime_list.hpp"
#include "sync/queue.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"
#include "test_env.hpp"

// Regression: the queue always sent the oldest item first, and a failure did not change that.
// One change the service kept rejecting held up every change queued after it, indefinitely.
class TestQueueOrder : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // Requests stay in flight against a local proxy that never answers; nothing leaves the
    // machine and the test decides each outcome with complete().
    QVERIFY(m_blackhole.listen(QHostAddress::LocalHost));
    test::initCore("myanimelist");
    taiga::settings.setSyncEnabled(false);
    taiga::settings.setProxyType(QNetworkProxy::HttpProxy);
    taiga::settings.setProxyHost("127.0.0.1");
    taiga::settings.setProxyPort(m_blackhole.serverPort());
    QObject::disconnect(taiga::network(), &QNetworkAccessManager::finished, nullptr, nullptr);

    taiga::accounts.setMyanimelistUsername("test");
    taiga::accounts.setMyanimelistAccessToken("not-a-real-token");
    taiga::accounts.setMyanimelistAuthenticated(true);

    sync::queue.init();
  }

  void failedItemDoesNotBlockOthers() {
    for (const int id : {1, 2}) {
      anime::db.updateItem(test::makeAnime(id, 12));
      ListEntry entry;
      entry.anime_id = id;
      entry.status = anime::list::Status::Watching;
      entry.watched_episodes = id;
      anime::db.updateEntry(entry);
      sync::queue.push(id, anime::list::Field::Episode);
    }

    QSignalSpy processing(&sync::queue, &sync::Queue::processing);

    sync::queue.process();
    QCOMPARE(processing.last().first().toInt(), 1);
    sync::queue.complete(false, "Rejected.");  // the service keeps rejecting this one

    sync::queue.process();
    QCOMPARE(processing.last().first().toInt(), 2);  // not stuck behind anime 1

    sync::queue.complete(true);  // anime 2 goes through, then anime 1 is retried
    QCOMPARE(processing.last().first().toInt(), 1);
    QCOMPARE(sync::queue.count(), 1);
  }

private:
  QTcpServer m_blackhole;
};

QTEST_GUILESS_MAIN(TestQueueOrder)
#include "tst_queue_order.moc"
