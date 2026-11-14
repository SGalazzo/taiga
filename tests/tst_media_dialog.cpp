#include <QCheckBox>
#include <QSpinBox>
#include <QTest>

#include "gui/media/media_dialog.hpp"
#include "media/anime_list.hpp"
#include "test_env.hpp"

// Regression: opening the dialog for a MyAnimeList entry that is Completed and being rewatched
// must not reset the episode progress. Before the fix, initList() triggered the rewatching
// checkbox handler, which set progress to 0, and OK then saved 0.
class TestMediaDialog : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { test::initCore("myanimelist"); }

  void rewatchingCompletedEntryKeepsProgress() {
    constexpr int kId = 1;
    anime::db.updateItem(test::makeAnime(kId, 24));

    ListEntry entry;
    entry.anime_id = kId;
    entry.status = anime::list::Status::Completed;
    entry.rewatching = true;
    entry.watched_episodes = 12;
    anime::db.updateEntry(entry);

    gui::MediaDialog dialog(nullptr);
    dialog.setAnime(*anime::db.item(kId));

    const auto* check = dialog.findChild<QCheckBox*>("checkRewatching");
    const auto* progress = dialog.findChild<QSpinBox*>("spinProgress");
    QVERIFY(check && progress);
    QVERIFY(check->isChecked());
    QCOMPARE(progress->value(), 12);
  }

  void userTogglingRewatchingStillResetsProgress() {
    constexpr int kId = 2;
    anime::db.updateItem(test::makeAnime(kId, 24));

    ListEntry entry;
    entry.anime_id = kId;
    entry.status = anime::list::Status::Completed;
    entry.rewatching = false;
    entry.watched_episodes = 24;
    anime::db.updateEntry(entry);

    gui::MediaDialog dialog(nullptr);
    dialog.setAnime(*anime::db.item(kId));

    auto* check = dialog.findChild<QCheckBox*>("checkRewatching");
    const auto* progress = dialog.findChild<QSpinBox*>("spinProgress");
    QVERIFY(check && progress);
    QCOMPARE(progress->value(), 24);

    check->setChecked(true);  // the intended behavior for a user click is unchanged
    QCOMPARE(progress->value(), 0);
  }
};

QTEST_MAIN(TestMediaDialog)
#include "tst_media_dialog.moc"
