#include <QJsonArray>
#include <QTest>

#include "media/anime_list.hpp"
#include "share/discord.hpp"
#include "sync/service.hpp"
#include "test_env.hpp"

using namespace Qt::Literals::StringLiterals;

// Ported from our feat/discord-presence tests to the API of erengy/taiga#1281.
namespace {

track::Episode makeEpisode(const int animeId, std::optional<std::pair<int, int>> range) {
  track::Episode episode;
  episode.setAnimeId(animeId);
  if (range) episode.setEpisodeNumberRange(*range);
  return episode;
}

void addAnime(const int id, const std::string& title, const int episodes,
              const std::string& imageUrl = "https://cdn.example/poster.jpg") {
  auto item = test::makeAnime(id, episodes);
  item.titles.romaji = title;
  item.image_url = imageUrl;
  anime::db.updateItem(item);
}

std::optional<QJsonObject> build(const int id, std::optional<std::pair<int, int>> range,
                                 const qint64 startTime = 0) {
  const auto activity = share::discord::buildActivity(makeEpisode(id, range), startTime);
  if (!activity) return std::nullopt;
  return share::discord::toJson(*activity);
}

}  // namespace

class TestDiscordPresence : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { test::initCore("myanimelist"); }

  void watchingActivity() {
    addAnime(1, "Sousou no Frieren", 28);
    const auto activity = build(1, std::pair{5, 5}, 1'700'000'000);
    QVERIFY(activity);
    const auto& a = *activity;

    QCOMPARE(a["type"].toInt(), 3);                 // Watching
    QCOMPARE(a["status_display_type"].toInt(), 2);  // status line shows the title
    QCOMPARE(a["details"].toString(), u"Sousou no Frieren"_s);
    QCOMPARE(a["details_url"].toString(), sync::animePageUrl(1));
    QCOMPARE(a["state"].toString(), u"Episode 5/28"_s);
    QCOMPARE(a["timestamps"]["start"].toInteger(), 1'700'000'000);

    const auto assets = a["assets"].toObject();
    QCOMPARE(assets["large_image"].toString(), u"https://cdn.example/poster.jpg"_s);
    QCOMPARE(assets["large_text"].toString(), u"Sousou no Frieren"_s);
    QCOMPARE(assets["small_image"].toString(), u"myanimelist"_s);
    QCOMPARE(assets["small_text"].toString(), u"MyAnimeList"_s);

    const auto buttons = a["buttons"].toArray();
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.first()["label"].toString(), u"View Anime"_s);
    QCOMPARE(buttons.first()["url"].toString(), sync::animePageUrl(1));

    QVERIFY(!build(1, std::pair{5, 5}, 0)->contains("timestamps"));
  }

  void episodeFormats() {
    addAnime(2, "Unknown Length", anime::kUnknownEpisodeCount);
    QCOMPARE((*build(2, std::pair{7, 7}))["state"].toString(), u"Episode 7"_s);

    addAnime(3, "Double Feature", 12);
    QCOMPARE((*build(3, std::pair{1, 2}))["state"].toString(), u"Episode 1-2/12"_s);

    // No episode number (e.g. a movie): omit the field instead of sending an empty string.
    QVERIFY(!build(3, std::nullopt)->contains("state"));
  }

  void privateEntryIsNotShared() {
    addAnime(4, "Secret Show", 12);
    ListEntry entry;
    entry.anime_id = 4;
    entry.is_private = true;
    anime::db.updateEntry(entry);
    QVERIFY(!build(4, std::pair{1, 1}));
  }

  void unknownAnimeIsNotShared() { QVERIFY(!build(999, std::pair{1, 1})); }

  void textLimits() {
    addAnime(5, std::string(200, 'x'), 12);
    const auto details = (*build(5, std::pair{1, 1}))["details"].toString();
    QCOMPARE(details.size(), 128);
    QVERIFY(details.endsWith(u"…"_s));

    // Discord rejects texts shorter than 2 characters, so leave them out (the anime "K").
    addAnime(6, "K", 13);
    const auto activity = *build(6, std::pair{1, 1});
    QVERIFY(!activity.contains("details"));
    QVERIFY(!activity.contains("status_display_type"));
  }

  void nonHttpsImageFallsBackToDefault() {
    addAnime(8, "No Poster", 12, "");
    const auto assets = (*build(8, std::pair{1, 1}))["assets"].toObject();
    QCOMPARE(assets["large_image"].toString(), u"default"_s);
  }
};

QTEST_GUILESS_MAIN(TestDiscordPresence)
#include "tst_discord_presence.moc"
