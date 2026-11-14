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

#include "episode.hpp"

#include <algorithm>
#include <charconv>
#include <ranges>

#include "media/anime.hpp"

namespace track {

Episode::Episode() : anime_id_{anime::kUnknownId} {}

auto Episode::find(const anitomy::ElementKind kind) const {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  return std::ranges::find_if(elements_, is_kind);
}

int Episode::animeId() const {
  return anime_id_;
}

void Episode::setAnimeId(int id) {
  anime_id_ = id;
}

// Truncates suffixed numbers (e.g. `4a` -> `4`). Fractional numbers (e.g. `7.5`)
// are rejected, because they would collide with integer episode numbers, and so
// are values that are not numbers at all.
static std::optional<int> parseEpisodeNumber(const std::string& value) {
  if (value.contains('.')) return std::nullopt;
  int number = 0;
  const auto [_, ec] = std::from_chars(value.data(), value.data() + value.size(), number);
  if (ec != std::errc{}) return std::nullopt;
  return number;
}

std::optional<std::pair<int, int>> Episode::episodeNumberRange() const {
  const auto numbers = elements(anitomy::ElementKind::Episode);
  if (numbers.empty()) return std::nullopt;
  const auto first = parseEpisodeNumber(numbers.front());
  const auto last = parseEpisodeNumber(numbers.back());
  if (!first || !last) return std::nullopt;
  return std::pair{*first, *last};
}

void Episode::setEpisodeNumberRange(const std::pair<int, int>& range) {
  removeElements(anitomy::ElementKind::Episode);
  addElement(anitomy::ElementKind::Episode, std::to_string(range.first));
  if (range.second > range.first) {
    addElement(anitomy::ElementKind::Episode, std::to_string(range.second));
  }
}

const std::vector<anitomy::Element>& Episode::elements() const noexcept {
  return elements_;
}

void Episode::setElements(std::vector<anitomy::Element>& elements) {
  elements_ = elements;
}

bool Episode::contains(const anitomy::ElementKind kind) const {
  return find(kind) != elements_.end();
}

std::string Episode::element(const anitomy::ElementKind kind, const std::string placeholder) const {
  const auto it = find(kind);
  if (it != elements_.end()) return it->value;
  return placeholder;
};

std::vector<std::string> Episode::elements(const anitomy::ElementKind kind) const {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  const auto to_value = [](const anitomy::Element& element) { return element.value; };

  return elements_ | std::views::filter(is_kind) | std::views::transform(to_value) |
         std::ranges::to<std::vector>();
}

void Episode::addElement(const anitomy::ElementKind kind, const std::string& value) {
  elements_.emplace_back(anitomy::Element{.kind = kind, .value = value});
}

void Episode::removeElements(const anitomy::ElementKind kind) {
  const auto is_kind = [kind](const anitomy::Element& element) { return element.kind == kind; };
  std::erase_if(elements_, is_kind);
}

}  // namespace track
