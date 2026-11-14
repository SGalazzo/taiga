#include <QTest>

#include "track/episode.hpp"
#include "track/update_decision.hpp"

using EpisodeRange = std::optional<std::pair<int, int>>;

namespace {

track::Episode makeEpisode(std::initializer_list<std::string> numbers) {
  track::Episode episode;
  for (const auto& number : numbers) episode.addElement(anitomy::ElementKind::Episode, number);
  return episode;
}

}  // namespace

class TestEpisodeNumber : public QObject {
  Q_OBJECT

private slots:
  void range_data() {
    QTest::addColumn<QStringList>("numbers");
    QTest::addColumn<EpisodeRange>("expected");

    QTest::newRow("none") << QStringList{} << EpisodeRange{};
    QTest::newRow("plain") << QStringList{"4"} << EpisodeRange{{4, 4}};
    QTest::newRow("leading zero") << QStringList{"04"} << EpisodeRange{{4, 4}};
    QTest::newRow("suffix") << QStringList{"4a"} << EpisodeRange{{4, 4}};
    QTest::newRow("uppercase suffix") << QStringList{"111C"} << EpisodeRange{{111, 111}};
    QTest::newRow("range") << QStringList{"01", "12"} << EpisodeRange{{1, 12}};
    QTest::newRow("fractional") << QStringList{"07.5"} << EpisodeRange{};
    QTest::newRow("fractional in range") << QStringList{"7", "7.5"} << EpisodeRange{};
    QTest::newRow("non-numeric") << QStringList{"abc"} << EpisodeRange{};
    QTest::newRow("overflow") << QStringList{"99999999999"} << EpisodeRange{};
  }

  void range() {
    QFETCH(QStringList, numbers);
    QFETCH(EpisodeRange, expected);

    track::Episode episode;
    for (const auto& number : numbers) {
      episode.addElement(anitomy::ElementKind::Episode, number.toStdString());
    }
    QCOMPARE(episode.episodeNumberRange(), expected);
  }

  void fractionalEpisodeIsNotWatched() {
    anime::Details item;
    item.episode_count = 12;

    const auto decision = track::decideUpdate(makeEpisode({"7.5"}), item, nullptr, false);
    QCOMPARE(decision.action, track::UpdateDecision::Action::Deny);
    QCOMPARE(decision.reason, track::UpdateDecision::Reason::InvalidEpisode);
  }

  void suffixedEpisodeIsWatched() {
    anime::Details item;
    item.episode_count = 12;

    QCOMPARE(track::watchedEpisodeNumber(makeEpisode({"4a"}), item), 4);
  }
};

QTEST_GUILESS_MAIN(TestEpisodeNumber)
#include "tst_episode_number.moc"
