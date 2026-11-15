#include <QTest>

#include "track/media_stream.hpp"

using track::media::isPrivateBrowserWindow;
using track::media::isStreamingSiteUrl;

class TestMediaStream : public QObject {
  Q_OBJECT

private slots:
  void streamingSiteUrl_data() {
    QTest::addColumn<QString>("url");
    QTest::addColumn<bool>("expected");

    QTest::newRow("plex app") << "app.plex.tv/desktop/#!/server/abc/details?key=1" << true;
    QTest::newRow("with scheme") << "https://app.plex.tv/desktop" << true;
    QTest::newRow("case") << "APP.Plex.TV/desktop" << true;
    QTest::newRow("host only") << "app.plex.tv" << true;
    QTest::newRow("local plex") << "10.0.0.2:32400/web/index.html#!/" << true;
    QTest::newRow("local jellyfin") << "http://192.168.1.5:8096/web/#/video" << true;
    QTest::newRow("gmail") << "mail.google.com/mail/u/0/#inbox" << false;
    QTest::newRow("lan admin page") << "10.0.0.2:8006/" << false;
    QTest::newRow("longer host") << "app.plex.tv.example.com/desktop" << false;
    QTest::newRow("in path") << "example.com/app.plex.tv/desktop" << false;
    QTest::newRow("wildcard across path") << "example.com/x:32400/web" << false;
    QTest::newRow("empty") << "" << false;
  }

  void streamingSiteUrl() {
    QFETCH(QString, url);
    QFETCH(bool, expected);

    const std::vector<std::string> sites{"app.plex.tv", "*:32400/web", " *:8096/web ", ""};
    QCOMPARE(isStreamingSiteUrl(url.toStdString(), sites), expected);
  }

  void noSitesMatchNothing() { QVERIFY(!isStreamingSiteUrl("app.plex.tv/desktop", {})); }

  void privateWindow_data() {
    QTest::addColumn<QString>("title");
    QTest::addColumn<bool>("expected");

    QTest::newRow("chrome") << "Plex - Google Chrome" << false;
    QTest::newRow("chrome incognito") << "Plex (Incognito) - Google Chrome" << true;
    QTest::newRow("edge inprivate") << "Plex - [InPrivate] - Microsoft Edge" << true;
    QTest::newRow("firefox private") << "Plex — Mozilla Firefox Private Browsing" << true;
    QTest::newRow("brave private") << "Plex (Private) - Brave" << true;
  }

  void privateWindow() {
    QFETCH(QString, title);
    QFETCH(bool, expected);
    QCOMPARE(isPrivateBrowserWindow(title.toStdString()), expected);
  }
};

QTEST_GUILESS_MAIN(TestMediaStream)
#include "tst_media_stream.moc"
