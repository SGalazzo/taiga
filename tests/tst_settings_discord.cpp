#include <QCheckBox>
#include <QFile>
#include <QGroupBox>
#include <QTest>

#include "compat/settings.hpp"
#include "gui/settings/settings_dialog.hpp"
#include "taiga/accounts.hpp"
#include "test_env.hpp"

using namespace Qt::Literals::StringLiterals;

// Ported from our feat/discord-presence tests to erengy/taiga#1281.
class TestSettingsDiscord : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { test::initCore("myanimelist"); }

  void disabledByDefault() { QVERIFY(!taiga::settings.discordEnabled()); }

  void checkboxReflectsSettings() {
    // Applying would contact the real Discord app, so only loading is tested here.
    {
      gui::SettingsDialog dialog(nullptr);
      QVERIFY(!dialog.findChild<QCheckBox*>("discordEnabledCheckBox")->isChecked());
      QVERIFY(dialog.findChild<QGroupBox*>("discordGroupBox")->isEnabled());
    }
    taiga::settings.setDiscordEnabled(true);
    taiga::settings.setSharingEnabled(false);
    {
      gui::SettingsDialog dialog(nullptr);
      QVERIFY(dialog.findChild<QCheckBox*>("discordEnabledCheckBox")->isChecked());
      auto* group = dialog.findChild<QGroupBox*>("discordGroupBox");
      QVERIFY(!group->isEnabled());
      dialog.findChild<QCheckBox*>("sharingEnabledCheckBox")->setChecked(true);
      QVERIFY(group->isEnabled());
    }
    taiga::settings.setDiscordEnabled(false);
    taiga::settings.setSharingEnabled(true);
  }

  void importedFromV1() {
    const auto path = QString::fromStdString(taiga::get_data_path()) + u"/v1-settings.xml"_s;
    QFile file{path};
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<settings>
  <announce>
    <discord applicationid="379871385176244224" enabled="true" groupenabled="true" />
    <http enabled="false" url="" />
  </announce>
  <program>
    <general />
  </program>
</settings>
)");
    file.close();

    taiga::settings.setDiscordEnabled(false);
    compat::v1::readSettings(path.toStdString(), taiga::settings, taiga::accounts);
    QVERIFY(taiga::settings.discordEnabled());
    taiga::settings.setDiscordEnabled(false);
  }
};

QTEST_MAIN(TestSettingsDiscord)
#include "tst_settings_discord.moc"
