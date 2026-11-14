#include <QNetworkProxy>
#include <QTcpServer>
#include <QTest>

#include "sync/myanimelist/myanimelist.hpp"
#include "taiga/accounts.hpp"
#include "taiga/network.hpp"
#include "test_env.hpp"

// Regression: authentication results were applied to the currently selected service rather than
// the service that reported them, and that service was then synchronized. Authorizing
// MyAnimeList while AniList is selected flagged AniList as authenticated.
class TestAuthServiceId : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    QVERIFY(m_blackhole.listen(QHostAddress::LocalHost));  // nothing leaves this machine
    test::initCore("anilist");
    taiga::settings.setSyncEnabled(false);
    taiga::settings.setProxyType(QNetworkProxy::HttpProxy);
    taiga::settings.setProxyHost("127.0.0.1");
    taiga::settings.setProxyPort(m_blackhole.serverPort());
    QObject::disconnect(taiga::network(), &QNetworkAccessManager::finished, nullptr, nullptr);

    taiga::accounts.setAnilistAuthenticated(false);
    taiga::accounts.setMyanimelistAuthenticated(false);
  }

  void resultAppliesToReportingService() {
    emit sync::myanimelist::Service::instance()->authenticationCompleted(true);

    QVERIFY(taiga::accounts.myanimelistAuthenticated());
    QVERIFY(!taiga::accounts.anilistAuthenticated());
  }

private:
  QTcpServer m_blackhole;
};

QTEST_GUILESS_MAIN(TestAuthServiceId)
#include "tst_auth_service_id.moc"
