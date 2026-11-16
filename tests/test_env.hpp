#pragma once

#include <QDateTime>
#include <QDir>
#include <QString>

#include "media/anime.hpp"
#include "media/anime_db.hpp"
#include "taiga/path.hpp"
#include "taiga/settings.hpp"

namespace test {

// Portable build: data lives in <test exe dir>/data, which is separate from bin/.
inline void resetDataDir() {
  QDir dir{QString::fromStdString(taiga::getDataPath())};
  dir.removeRecursively();
  QDir{}.mkpath(dir.path());
}

inline void initCore(const std::string& service) {
  resetDataDir();
  taiga::settings()->init();
  taiga::settings()->setSyncService(service);
  anime::db()->init();
}

// A fully populated item, so anime::isStale() is false and nothing hits the network.
inline Anime makeAnime(const int id, const int episodes) {
  Anime item;
  item.id = id;
  item.titles.romaji = "Test Anime " + std::to_string(id);
  item.episodeCount = episodes;
  item.lastModified = QDateTime::currentSecsSinceEpoch();
  item.synopsis = "Synopsis";
  item.genres = {"Action"};
  item.score = 8.0;
  return item;
}

}  // namespace test
