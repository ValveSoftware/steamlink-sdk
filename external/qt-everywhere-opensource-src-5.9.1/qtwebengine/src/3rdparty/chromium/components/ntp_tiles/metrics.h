// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_METRICS_H_
#define COMPONENTS_NTP_TILES_METRICS_H_

#include <vector>

#include "components/ntp_tiles/ntp_tile.h"

namespace ntp_tiles {
namespace metrics {

// The visual type of a most visited tile.
//
// These values must stay in sync with the MostVisitedTileType enum
// in histograms.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp
enum MostVisitedTileType {
  // The icon or thumbnail hasn't loaded yet.
  NONE,
  // The item displays a site's actual favicon or touch icon.
  ICON_REAL,
  // The item displays a color derived from the site's favicon or touch icon.
  ICON_COLOR,
  // The item displays a default gray box in place of an icon.
  ICON_DEFAULT,
  // The number of different tile types that get recorded. Entries below this
  // are not recorded in UMA.
  NUM_RECORDED_TILE_TYPES,
  // The item displays a thumbnail of the page. Used on desktop.
  THUMBNAIL,
};

// Records an impression of an individual tile.
void RecordTileImpression(int index, NTPTileSource source);

// Records an impression of the page itself, after all tiles have loaded.
void RecordPageImpression(int number_of_tiles);

// Records the visual types (see above) of all visible tiles.
// TODO(treib): Merge this with RecordPageImpression.
void RecordImpressionTileTypes(
    const std::vector<MostVisitedTileType>& tile_types,
    const std::vector<NTPTileSource>& sources);

// Records a click on a tile.
void RecordTileClick(int index,
                     NTPTileSource source,
                     MostVisitedTileType tile_type);

}  // namespace metrics
}  // namespace ntp_tiles

#endif
