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

#include "discord.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QTimer>

#include "base/string.hpp"
#include "link/discord_ipc.hpp"
#include "media/anime_db.hpp"
#include "media/anime_utils.hpp"
#include "sync/service.hpp"
#include "taiga/settings.hpp"
#include "track/media.hpp"

namespace discord {

namespace {

constexpr auto kApplicationId = "379871385176244224";
constexpr int kActivityTypeWatching = 3;
constexpr int kStatusDisplayDetails = 2;  // show `details` instead of the application name
constexpr qsizetype kMinTextLength = 2;
constexpr qsizetype kMaxTextLength = 128;

std::optional<QString> fitText(const QString& text) {
  const auto trimmed = text.trimmed();
  if (trimmed.size() < kMinTextLength) return std::nullopt;
  if (trimmed.size() <= kMaxTextLength) return trimmed;
  return trimmed.left(kMaxTextLength - 1) + u"…"_s;
}

QString formatEpisode(const track::Episode& episode, const int episodeCount) {
  const auto range = episode.episodeNumberRange();
  if (!range) return {};

  const auto number = range->first == range->second
                          ? QString::number(range->first)
                          : u"%1-%2"_s.arg(range->first).arg(range->second);
  return episodeCount > 0 ? u"Episode %1/%2"_s.arg(number).arg(episodeCount)
                          : u"Episode %1"_s.arg(number);
}

}  // namespace

std::optional<QJsonObject> buildActivity(const track::Episode& episode) {
  const auto* item = anime::db.item(episode.animeId());
  if (!item) return std::nullopt;

  if (const auto* entry = anime::db.entry(item->id); entry && entry->is_private) {
    return std::nullopt;
  }

  const auto serviceId = sync::currentServiceId();
  const auto imageUrl = QString::fromStdString(item->image_url);

  QJsonObject assets{
      {"large_image", imageUrl.startsWith(u"https://") ? imageUrl : u"default"_s},
      {"small_image", sync::serviceSlug(serviceId)},
      {"small_text", sync::serviceName(serviceId)},
  };

  QJsonObject activity{
      {"type", kActivityTypeWatching},
      {"buttons",
       QJsonArray{QJsonObject{{"label", "View Anime"}, {"url", sync::animePageUrl(item->id)}}}},
  };

  if (const auto title = fitText(QString::fromStdString(anime::preferredTitle(*item)))) {
    activity["details"] = *title;
    assets["large_text"] = *title;
    activity["status_display_type"] = kStatusDisplayDetails;
  }
  if (const auto state = fitText(formatEpisode(episode, item->episode_count))) {
    activity["state"] = *state;
  }

  activity["assets"] = assets;
  return activity;
}

Presence::Presence(QObject* parent)
    : QObject{parent},
      client_{new IpcClient{QString::fromLatin1(kApplicationId), this}},
      updateTimer_{new QTimer{this}} {
  // Discord rate-limits activity updates, and players can report changes in quick succession.
  updateTimer_->setSingleShot(true);
  updateTimer_->setInterval(std::chrono::milliseconds{1500});
  connect(updateTimer_, &QTimer::timeout, this, &Presence::update);
}

void Presence::init() {
  connect(track::media::detection(), &track::media::Detection::currentEpisodeChanged, this,
          &Presence::onEpisodeChanged);

  const auto refreshIfCurrent = [this](const int id) {
    if (episode_ && episode_->animeId() == id) updateTimer_->start();
  };
  connect(&anime::db, &anime::Database::itemUpdated, this, refreshIfCurrent);
  connect(&anime::db, &anime::Database::entryUpdated, this, refreshIfCurrent);

  setEnabled(taiga::settings.discordEnabled());
}

void Presence::setEnabled(bool enabled) {
  client_->setEnabled(enabled);
  if (enabled) update();
}

void Presence::onEpisodeChanged(const std::optional<track::Episode>& episode) {
  episode_ = episode;
  updateTimer_->start();
}

void Presence::update() {
  if (!client_->isEnabled()) return;

  const auto activity = episode_ ? buildActivity(*episode_) : std::nullopt;
  client_->setActivity(activity ? QJsonValue{*activity} : QJsonValue{});
}

Presence* presence() {
  static auto instance = new Presence{qApp};
  return instance;
}

}  // namespace discord
