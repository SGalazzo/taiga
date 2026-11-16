#include <QTest>

#include "media/anime.hpp"
#include "media/anime_list.hpp"
#include "media/anime_list_utils.hpp"

using anime::list::getProgressRatio;

class TestProgressRatio : public QObject {
  Q_OBJECT

private slots:
  void ratio_data() {
    QTest::addColumn<int>("episodes");
    QTest::addColumn<int>("watched");
    QTest::addColumn<float>("expected");

    QTest::newRow("none watched") << 12 << 0 << 0.0f;
    QTest::newRow("half") << 12 << 6 << 0.5f;
    QTest::newRow("all") << 12 << 12 << 1.0f;
    QTest::newRow("more than known") << 12 << 13 << 1.0f;
    QTest::newRow("unknown length, not started") << 0 << 0 << 0.0f;
    QTest::newRow("unknown length, started") << 0 << 3 << 0.8f;
  }

  void ratio() {
    QFETCH(int, episodes);
    QFETCH(int, watched);
    QFETCH(float, expected);

    anime::Details item;
    item.episodeCount = episodes;
    anime::list::Entry entry;
    entry.watchedEpisodes = watched;

    QCOMPARE(getProgressRatio(&item, &entry), expected);
  }

  void notInList() {
    anime::Details item;
    item.episodeCount = 0;
    QCOMPARE(getProgressRatio(&item, nullptr), 0.0f);
  }
};

QTEST_GUILESS_MAIN(TestProgressRatio)
#include "tst_progress_ratio.moc"
