#include <QNetworkProxy>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTest>

#include "media/anime_list.hpp"
#include "sync/anilist/anilist.hpp"
#include "sync/myanimelist/myanimelist.hpp"
#include "sync/queue.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"
#include "test_env.hpp"

using namespace Qt::Literals::StringLiterals;

// Regression: when a queued request ends in a failed token refresh, the service emits
// authenticationCompleted(false) but nothing completed the queue item, so the queue stayed
// "processing" and never sent anything again until restart.
class TestQueueAuth : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    // A proxy that accepts connections and never answers keeps requests in flight and
    // guarantees nothing leaves this machine. Must be set before the network manager exists.
    QVERIFY(m_blackhole.listen(QHostAddress::LocalHost));
    test::initCore("myanimelist");
    taiga::settings.setSyncEnabled(false);  // no timers; the test drives the queue
    taiga::settings.setProxyType(QNetworkProxy::HttpProxy);
    taiga::settings.setProxyHost("127.0.0.1");
    taiga::settings.setProxyPort(m_blackhole.serverPort());

    // The debug logger dereferences taiga::app(), which does not exist in tests.
    QObject::disconnect(taiga::network(), &QNetworkAccessManager::finished, nullptr, nullptr);
    QCOMPARE(taiga::network()->proxy().port(), m_blackhole.serverPort());

    taiga::accounts.setMyanimelistUsername("test");
    taiga::accounts.setMyanimelistAccessToken("not-a-real-token");
    taiga::accounts.setMyanimelistAuthenticated(true);

    sync::queue.init();
  }

  void failedAuthenticationReleasesQueue() {
    constexpr int kId = 1;
    anime::db.updateItem(test::makeAnime(kId, 12));
    ListEntry entry;
    entry.anime_id = kId;
    entry.status = anime::list::Status::Watching;
    entry.watched_episodes = 3;
    anime::db.updateEntry(entry);
    sync::queue.push(kId, anime::list::Field::Episode);

    QSignalSpy processing(&sync::queue, &sync::Queue::processing);
    sync::queue.process();
    QCOMPARE(processing.count(), 1);  // request is now in flight

    // What refreshAccessToken() emits when the refresh fails.
    emit sync::myanimelist::Service::instance()->authenticationCompleted(false);

    const auto* item = sync::queue.currentItem();
    QVERIFY(item);
    QCOMPARE(QString::fromStdString(item->last_error), u"Authentication failed."_s);

    // Not stuck: once authenticated again, the queue sends the item again.
    taiga::accounts.setMyanimelistAuthenticated(true);
    sync::queue.process();
    QCOMPARE(processing.count(), 2);
  }

  void otherServiceFailureKeepsItemInFlight() {
    // The item from the previous test is in flight again (MyAnimeList is the current service).
    const auto retries = sync::queue.currentItem()->retry_count;

    emit sync::anilist::Service::instance()->authenticationCompleted(false);

    QCOMPARE(sync::queue.currentItem()->retry_count, retries);
  }

private:
  QTcpServer m_blackhole;
};

QTEST_GUILESS_MAIN(TestQueueAuth)
#include "tst_queue_auth.moc"
