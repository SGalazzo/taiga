/**
 * Taiga
 * Copyright (C) 2010-2026, Eren Okka
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "settings_page_discord.hpp"

#include <QCheckBox>

#include "link/discord.hpp"
#include "taiga/settings.hpp"
#include "ui_settings_dialog.h"

namespace gui {

SettingsPageDiscord::SettingsPageDiscord(Ui::SettingsDialog* ui, QDialog* dialog)
    : SettingsPage(ui, dialog) {}

void SettingsPageDiscord::load() {
  ui_->discordEnabledCheckBox->setChecked(taiga::settings.discordEnabled());
}

void SettingsPageDiscord::apply() const {
  const bool enabled = ui_->discordEnabledCheckBox->isChecked();
  taiga::settings.setDiscordEnabled(enabled);
  discord::presence()->setEnabled(enabled);  // settings have no change notifications
}

}  // namespace gui
