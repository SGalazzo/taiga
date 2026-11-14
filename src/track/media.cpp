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

#include "media.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "taiga/settings.hpp"
#include "track/episode.hpp"
#include "track/media_player.hpp"
#include "track/recognition.hpp"

namespace track::media {

namespace {

struct MediaFields {
  std::vector<std::string> files;
  std::string title;
  std::string url;
};

struct ResolvedMedia {
  Episode episode;
  std::optional<std::string> file;
};

#ifdef Q_OS_WINDOWS
anisthesia::Media flattenMedia(const anisthesia::win::Result& result) {
  anisthesia::Media media;

  for (const auto& item : result.media) {
    media.information.append_range(item.information);
  }

  return media;
}
#endif

MediaFields extractMediaFields(const anisthesia::Media& media) {
  MediaFields fields;

  for (const auto& information : media.information) {
    switch (information.type) {
      case anisthesia::MediaInfoType::File:
        fields.files.push_back(information.value);
        break;
      case anisthesia::MediaInfoType::Url:
        fields.url = information.value;
        break;
      case anisthesia::MediaInfoType::Title:
        if (fields.title.empty()) fields.title = information.value;
        break;
      case anisthesia::MediaInfoType::Unknown:
        // Always prefer window title over page title reported via UI Automation.
        fields.title = information.value;
        break;
      case anisthesia::MediaInfoType::Tab:
        break;
    }
  }

  return fields;
}

std::optional<ResolvedMedia> resolveMedia(const MediaFields& fields) {
  if (!fields.files.empty()) {
    // Players may have other files open (e.g. subtitles, external audio tracks,
    // resource files), so pick the first one that is a video file.
    for (const auto& file : fields.files) {
      const QFileInfo fileInfo{QString::fromStdString(file)};
      auto episode = track::recognition::parseFileInfo(fileInfo);
      if (track::recognition::isVideoFile(episode)) {
        return ResolvedMedia{.episode = std::move(episode), .file = file};
      }
    }

    // Files are open, but none of them is a video (e.g. audio files). Don't
    // fall back to window title, which would describe the same file.
    return std::nullopt;
  }

  if (fields.title.empty()) return std::nullopt;

  return ResolvedMedia{.episode = track::recognition::parse(fields.title)};
}

}  // namespace

Detection::Detection(QObject* parent) : QObject(parent) {
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, &Detection::poll);
}

const std::optional<Episode> Detection::getCurrentEpisode() const {
  return currentEpisode_;
}

const std::optional<std::string> Detection::getCurrentFile() const {
  return currentFile_;
}

const std::optional<Detection::media_t> Detection::getCurrentMedia() const {
  return currentMedia_;
}

const std::optional<Detection::player_t> Detection::getCurrentPlayer() const {
  return currentPlayer_;
}

bool Detection::init() {
  if (!parsePlayersData(players_)) {
    return false;
  }

  setPollingEnabled(isEnabled());

  return true;
}

bool Detection::isEnabled() const {
  return taiga::settings.detectionEnabled();
}

void Detection::setEnabled(bool enabled) {
  taiga::settings.setDetectionEnabled(enabled);
  setPollingEnabled(enabled);
}

void Detection::setPollingEnabled(bool enabled) {
#ifdef Q_OS_WINDOWS
  if (enabled) {
    const auto interval = taiga::settings.mediaDetectionInterval();
    pollTimer_->start(interval);
  } else {
    pollTimer_->stop();
    reset();
  }
#endif
}

void Detection::poll() {
#ifdef Q_OS_WINDOWS
  const auto players = getEnabledPlayers(players_);

  static const auto media_proc = [](const anisthesia::MediaInfo&) {
    return true;  // Accept all media
  };

  std::vector<anisthesia::win::Result> results;
  if (!anisthesia::win::GetResults(players, media_proc, results)) {
    reset();
    return;
  }

  // Every window is checked, so that a window showing something else (e.g. another browser
  // window) cannot hide the one that is playing an anime.
  std::vector<media_t> media;
  std::vector<std::optional<ResolvedMedia>> resolved;
  std::vector<Candidate> candidates;
  for (const auto& result : results) {
    auto& item = media.emplace_back(flattenMedia(result));
    auto& entry = resolved.emplace_back(resolveMedia(extractMediaFields(item)));
    if (entry) entry->episode.setAnimeId(track::recognition::identify(entry->episode));
    candidates.push_back({
        .handle = result.window.handle,
        .hasEpisode = entry.has_value(),
        .identified = entry && entry->episode.animeId() != anime::kUnknownId,
    });
  }

  const auto index = chooseCandidate(candidates, currentWindowHandle_);
  if (!index) {
    reset();
    return;
  }

  currentPlayer_ = results[*index].player;
  currentMedia_ = std::move(media[*index]);
  currentWindowHandle_ = results[*index].window.handle;
  currentFile_ = std::move(resolved[*index]->file);
  const auto& episode = resolved[*index]->episode;

  if (hasEpisodeChanged(episode)) {
    currentEpisode_ = episode;
    emit currentEpisodeChanged(episode);
  }
#endif
}

std::optional<std::size_t> chooseCandidate(std::span<const Candidate> candidates,
                                           const void* currentHandle) {
  const auto find = [&candidates](auto predicate) -> std::optional<std::size_t> {
    const auto it = std::ranges::find_if(candidates, predicate);
    if (it == candidates.end()) return std::nullopt;
    return static_cast<std::size_t>(it - candidates.begin());
  };
  const auto isCurrent = [currentHandle](const Candidate& candidate) {
    return currentHandle && candidate.handle == currentHandle;
  };

  if (const auto index = find([&](const Candidate& c) { return isCurrent(c) && c.identified; })) {
    return index;
  }
  if (const auto index = find(&Candidate::identified)) return index;
  if (const auto index = find([&](const Candidate& c) { return isCurrent(c) && c.hasEpisode; })) {
    return index;
  }
  return find(&Candidate::hasEpisode);
}

bool Detection::isMediaIdentified() const {
  return currentEpisode_.has_value() && currentEpisode_->animeId() != anime::kUnknownId;
}

bool Detection::isPlayerFocused() const {
#ifdef Q_OS_WINDOWS
  return currentWindowHandle_ && currentWindowHandle_ == static_cast<void*>(GetForegroundWindow());
#else
  return true;  // no way to tell yet
#endif
}

void Detection::setCurrentEpisodeAnimeId(int animeId) {
  if (!currentEpisode_) return;

  currentEpisode_->setAnimeId(animeId);
  emit currentEpisodeChanged(currentEpisode_);
}

void Detection::reset() {
  currentPlayer_.reset();
  currentMedia_.reset();
  currentFile_.reset();
  currentWindowHandle_ = nullptr;

  if (currentEpisode_) {
    currentEpisode_.reset();
    emit currentEpisodeChanged(std::nullopt);
  }
}

bool Detection::hasEpisodeChanged(const Episode& episode) const {
  if (!currentEpisode_) return true;
  if (currentEpisode_->animeId() != episode.animeId()) return true;

  return currentEpisode_->elements(anitomy::ElementKind::Episode) !=
         episode.elements(anitomy::ElementKind::Episode);
}

}  // namespace track::media
