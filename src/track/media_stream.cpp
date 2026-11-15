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

#include "media_stream.hpp"

#include <QRegularExpression>
#include <QString>
#include <algorithm>

#include "base/string.hpp"

namespace track::media {

bool isStreamingSiteUrl(std::string_view url, const std::vector<std::string>& sites) {
  const auto address = QString::fromUtf8(url.data(), url.size()).trimmed();
  if (address.isEmpty()) return false;

  return std::ranges::any_of(sites, [&address](const std::string& site) {
    const auto prefix = QString::fromStdString(site).trimmed();
    if (prefix.isEmpty()) return false;

    // The site must end at a boundary, so that `example.com` doesn't match `example.com.evil`.
    auto pattern = QRegularExpression::escape(prefix);
    pattern.replace(u"\\*"_s, u"[^/]*"_s);
    const QRegularExpression regex{u"^(?:https?://)?%1(?:[/?#:]|$)"_s.arg(pattern),
                                   QRegularExpression::CaseInsensitiveOption};
    return regex.match(address).hasMatch();
  });
}

bool isPrivateBrowserWindow(std::string_view title) {
  static constexpr std::string_view markers[] = {
      "(Incognito)", "(Private)", "InPrivate", "Private Browsing",
  };
  return std::ranges::any_of(markers, [title](std::string_view marker) {
    return title.contains(marker);
  });
}

}  // namespace track::media
