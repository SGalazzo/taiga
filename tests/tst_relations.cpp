#include <QFile>
#include <QTest>

#include "track/recognition_relations.hpp"

using namespace Qt::Literals::StringLiterals;
using track::recognition::Relations;

namespace {

// Column order matches the anime-relations file: 0 = MyAnimeList, 1 = Kitsu, 2 = AniList.
constexpr int kMal = 0;
constexpr int kAniList = 2;

Relations load(const QString& rules, const int column) {
  Relations relations;
  relations.load(u"::meta\nversion: 1.3.0\n::rules\n"_s + rules, column);
  return relations;
}

}  // namespace

class TestRelations : public QObject {
  Q_OBJECT

private slots:
  void rangeRedirectsWithOffset() {
    const auto r = load(u"- 10001|10002|10003:14-26 -> 20001|20002|20003:1-13\n"_s, kMal);

    const auto hit = r.find(10001, 14);
    QVERIFY(hit.has_value());
    QCOMPARE(hit->id, 20001);
    QCOMPARE(hit->episode_range.first, 1);

    const auto last = r.find(10001, 26);
    QVERIFY(last.has_value());
    QCOMPARE(last->episode_range.first, 13);

    QVERIFY(!r.find(10001, 13).has_value());
    QVERIFY(!r.find(10001, 27).has_value());
  }

  void columnSelectsService() {
    const auto r = load(u"- 10001|10002|10003:14-26 -> 20001|20002|20003:1-13\n"_s, kAniList);

    QVERIFY(!r.find(10001, 14).has_value());
    const auto hit = r.find(10003, 14);
    QVERIFY(hit.has_value());
    QCOMPARE(hit->id, 20003);
  }

  void tildeRepeatsSourceId() {
    const auto r = load(u"- 100|?|?:13 -> ~|~|~:0\n"_s, kMal);
    const auto hit = r.find(100, 13);
    QVERIFY(hit.has_value());
    QCOMPARE(hit->id, 100);
    QCOMPARE(hit->episode_range.first, 0);
  }

  void unknownSourceIdIsSkipped() {
    const auto r = load(u"- ?|?|300:2 -> ?|?|301:1\n"_s, kMal);
    QVERIFY(!r.find(0, 2).has_value());
  }

  void bangAlsoRedirectsDestinationToItself() {
    const auto r = load(u"- 1|1|1:14-26 -> 2|2|2:1-13!\n"_s, kMal);
    const auto self = r.find(2, 14);
    QVERIFY(self.has_value());
    QCOMPARE(self->id, 2);
    QCOMPARE(self->episode_range.first, 1);
  }

  void openEndedRange() {
    const auto r = load(u"- 5|5|5:13-? -> 6|6|6:1-?\n"_s, kMal);
    const auto hit = r.find(5, 40);
    QVERIFY(hit.has_value());
    QCOMPARE(hit->id, 6);
    QCOMPARE(hit->episode_range.first, 28);
  }

  void realFileParsesAndRedirects() {
    QFile file(QStringLiteral(TAIGA_RELATIONS_FILE));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    Relations r;
    r.load(QString::fromUtf8(file.readAll()), kMal);
    // Sanity: at least one rule in the real data must resolve.
    // Gintama' (9969) continues Gintama numbering from 202 → 1.
    const auto hit = r.find(918, 202);
    QVERIFY2(hit.has_value(), "expected Gintama 918:202 to redirect");
  }
};

QTEST_GUILESS_MAIN(TestRelations)
#include "tst_relations.moc"
