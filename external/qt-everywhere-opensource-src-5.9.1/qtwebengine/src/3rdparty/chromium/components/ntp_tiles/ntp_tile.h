// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_NTP_TILE_H_
#define COMPONENTS_NTP_TILES_NTP_TILE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace ntp_tiles {

// The source of an NTP tile.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
enum class NTPTileSource {
  // Tile comes from the personal top sites list, based on local history.
  TOP_SITES,
  // Tile comes from the suggestions service, based on synced history.
  SUGGESTIONS_SERVICE,
  // Tile is regionally popular.
  POPULAR,
  // Tile is on an custodian-managed whitelist.
  WHITELIST
};

// A suggested site shown on the New Tab Page.
struct NTPTile {
  base::string16 title;
  GURL url;
  NTPTileSource source;

  // Only valid for source == WHITELIST (empty otherwise).
  base::FilePath whitelist_icon_path;

  NTPTile();
  NTPTile(const NTPTile&);
  ~NTPTile();
};

using NTPTilesVector = std::vector<NTPTile>;

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_NTP_TILE_H_
