#include <QTest>

#include "track/media.hpp"

using track::media::Candidate;
using track::media::chooseCandidate;

// Regression: detection looked at a single window per poll (the previous one, else the first),
// so a window showing something else could hide another window that was playing an anime, e.g.
// with two browser windows open. Discord presence and list updates then silently stopped.
class TestDetectionChoice : public QObject {
  Q_OBJECT

private slots:
  void switchesFromUnidentifiedCurrentToIdentifiedOther() {
    // The user's case: Taiga locked onto a non-anime browser window at startup.
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = true, .identified = false},
        {.handle = &b, .hasEpisode = true, .identified = true},
    };
    QCOMPARE(chooseCandidate(candidates, &a), std::optional<std::size_t>{1});
  }

  void keepsIdentifiedCurrentEvenIfAnotherComesFirst() {
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = true, .identified = true},
        {.handle = &b, .hasEpisode = true, .identified = true},
    };
    QCOMPARE(chooseCandidate(candidates, &b), std::optional<std::size_t>{1});
  }

  void firstWindowWithoutEpisodeDoesNotHideOthers() {
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = false, .identified = false},
        {.handle = &b, .hasEpisode = true, .identified = true},
    };
    QCOMPARE(chooseCandidate(candidates, nullptr), std::optional<std::size_t>{1});
  }

  void keepsUnidentifiedCurrentWhenNothingIsIdentified() {
    // So that "Set as now playing..." still has something to act on.
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = true, .identified = false},
        {.handle = &b, .hasEpisode = true, .identified = false},
    };
    QCOMPARE(chooseCandidate(candidates, &b), std::optional<std::size_t>{1});
    QCOMPARE(chooseCandidate(candidates, nullptr), std::optional<std::size_t>{0});
  }

  void currentGoneFallsBackToFirstWithEpisode() {
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = false, .identified = false},
        {.handle = &b, .hasEpisode = true, .identified = false},
    };
    QCOMPARE(chooseCandidate(candidates, &c), std::optional<std::size_t>{1});
  }

  void nothingWithEpisode() {
    const Candidate candidates[] = {
        {.handle = &a, .hasEpisode = false, .identified = false},
    };
    QCOMPARE(chooseCandidate(candidates, &a), std::nullopt);
    QCOMPARE(chooseCandidate({}, nullptr), std::nullopt);
  }

private:
  int a = 0, b = 0, c = 0;  // stand-ins for window handles
};

QTEST_GUILESS_MAIN(TestDetectionChoice)
#include "tst_detection_choice.moc"
