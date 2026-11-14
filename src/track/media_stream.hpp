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

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace track::media {

// Whether an address, as shown in a browser's address bar, is on one of the given sites. Sites
// are address prefixes without a scheme (e.g. `app.plex.tv`, `*:32400/web`), where `*` matches
// any part of a host name or path segment.
bool isStreamingSiteUrl(std::string_view url, const std::vector<std::string>& sites);

// Whether a browser window title belongs to a private (incognito) window.
bool isPrivateBrowserWindow(std::string_view title);

}  // namespace track::media
