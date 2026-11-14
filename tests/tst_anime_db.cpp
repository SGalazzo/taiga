#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

#include "media/anime_list.hpp"
#include "test_env.hpp"

// anime_list used to be keyed on the service's list-entry id, which is 0 for every
// MyAnimeList entry (and for any not-yet-synced add), so INSERT OR REPLACE collapsed
// them into one row. The in-memory map hid this until the database was read back.
namespace {

// A database created by an earlier build: anime_list keyed by the service's entry id.
bool createOldSchema(const QString& rows) {
  test::resetDataDir();
  const auto file = QString::fromStdString(taiga::get_data_path()) + "/media.sqlite";
  bool ok = false;
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", "old");
    db.setDatabaseName(file);
    if (db.open()) {
      QSqlQuery q{db};
      ok = q.exec("CREATE TABLE meta(id INTEGER PRIMARY KEY, name TEXT, value TEXT)") &&
           q.exec("CREATE TABLE anime_list(id INTEGER PRIMARY KEY, media_id INTEGER NOT NULL,"
                  " progress INTEGER, date_start TEXT, date_end TEXT, score INTEGER,"
                  " status INTEGER, private INTEGER, rewatched_times INTEGER,"
                  " rewatching INTEGER, rewatching_ep INTEGER, notes TEXT,"
                  " last_updated TEXT, pending_delete INTEGER NOT NULL DEFAULT 0)") &&
           q.exec("INSERT INTO anime_list(id, media_id, progress, status, last_updated) " + rows);
      db.close();
    }
  }
  QSqlDatabase::removeDatabase("old");
  return ok;
}

}  // namespace

class TestAnimeDb : public QObject {
  Q_OBJECT

private slots:
  void entriesWithoutServiceIdSurviveReload() {
    test::initCore("myanimelist");

    QList<ListEntry> entries;
    for (int id = 1; id <= 3; ++id) {
      anime::db.updateItem(test::makeAnime(id, 12));
      ListEntry entry;  // entry.id stays kUnknownId, as MyAnimeList's parser leaves it
      entry.anime_id = id;
      entry.status = anime::list::Status::Watching;
      entry.watched_episodes = id;
      entries.push_back(entry);
    }
    anime::db.updateEntries(entries);
    QCOMPARE(anime::db.entries().size(), 3);  // in memory

    anime::Database reloaded;  // same file, as after an application restart
    reloaded.init();
    QCOMPARE(reloaded.entries().size(), 3);
    QCOMPARE(reloaded.entry(2)->watched_episodes, 2);
  }

  void oldSchemaIsMigrated() {
    QVERIFY(createOldSchema("VALUES(101, 1, 5, 1, NULL), (102, 2, 7, 1, NULL)"));

    anime::Database migrated;
    migrated.init();
    QCOMPARE(migrated.entries().size(), 2);
    QCOMPARE(migrated.entry(2)->watched_episodes, 7);
    QCOMPARE(migrated.entry(2)->id, 102);

    // New key: one row per anime, even when the service entry id is unknown.
    QList<ListEntry> entries;
    for (int id = 3; id <= 4; ++id) {
      ListEntry entry;
      entry.anime_id = id;
      entries.push_back(entry);
    }
    migrated.updateEntries(entries);

    anime::Database reloaded;
    reloaded.init();
    QCOMPARE(reloaded.entries().size(), 4);
  }

  void duplicateRowsKeepMostRecentlyUpdated() {
    // Same anime twice; the row with the higher id is the older one.
    QVERIFY(createOldSchema("VALUES(201, 9, 3, 1, '1700000100'), (202, 9, 2, 1, '1700000000')"));

    anime::Database migrated;
    migrated.init();
    QCOMPARE(migrated.entries().size(), 1);
    QCOMPARE(migrated.entry(9)->id, 201);
    QCOMPARE(migrated.entry(9)->watched_episodes, 3);
  }
};

QTEST_GUILESS_MAIN(TestAnimeDb)
#include "tst_anime_db.moc"
