#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>

#include "gui/settings/settings_dialog.hpp"
#include "taiga/accounts.hpp"
#include "test_env.hpp"

// The MyAnimeList group gets an "Authorize..." button that runs the OAuth flow. The flow itself
// needs a browser and a real login, so this only checks the dialog wiring offscreen.
class TestSettingsAccounts : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    test::initCore("myanimelist");
    taiga::accounts.setMyanimelistUsername("Vash");
  }

  void authorizeButtonInMyAnimeListGroup() {
    gui::SettingsDialog dialog(nullptr);
    dialog.QWidget::show();

    auto* group = dialog.findChild<QGroupBox*>("myanimelistGroupBox");
    auto* button = dialog.findChild<QPushButton*>("myanimelistAuthorizeButton");
    QVERIFY(group && button);
    QVERIFY(group->isAncestorOf(button));
    QVERIFY(group->isVisible());
    QVERIFY(button->isVisible());
    QVERIFY(button->isEnabled());

    const auto* username = dialog.findChild<QLineEdit*>("myanimelistUsernameLineEdit");
    QVERIFY(username);
    QCOMPARE(username->text(), QStringLiteral("Vash"));
  }
};

QTEST_MAIN(TestSettingsAccounts)
#include "tst_settings_accounts.moc"
