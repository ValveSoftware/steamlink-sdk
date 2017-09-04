// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/metrics.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"

namespace ntp_tiles {
namespace metrics {

namespace {

// Maximum number of tiles to record in histograms.
const int kMaxNumTiles = 12;

// Identifiers for the various tile sources.
const char kHistogramClientName[] = "client";
const char kHistogramServerName[] = "server";
const char kHistogramPopularName[] = "popular";
const char kHistogramWhitelistName[] = "whitelist";

// Log an event for a given |histogram| at a given element |position|. This
// routine exists because regular histogram macros are cached thus can't be used
// if the name of the histogram will change at a given call site.
void LogHistogramEvent(const std::string& histogram,
                       int position,
                       int num_sites) {
  base::HistogramBase* counter = base::LinearHistogram::FactoryGet(
      histogram, 1, num_sites, num_sites + 1,
      base::Histogram::kUmaTargetedHistogramFlag);
  if (counter)
    counter->Add(position);
}

std::string GetSourceHistogramName(NTPTileSource source) {
  switch (source) {
    case NTPTileSource::TOP_SITES:
      return kHistogramClientName;
    case NTPTileSource::POPULAR:
      return kHistogramPopularName;
    case NTPTileSource::WHITELIST:
      return kHistogramWhitelistName;
    case NTPTileSource::SUGGESTIONS_SERVICE:
      return kHistogramServerName;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace

void RecordTileImpression(int index, NTPTileSource source) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SuggestionsImpression",
                            static_cast<int>(index), kMaxNumTiles);

  std::string histogram =
      base::StringPrintf("NewTabPage.SuggestionsImpression.%s",
                         GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, static_cast<int>(index), kMaxNumTiles);
}

void RecordPageImpression(int number_of_tiles) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.NumberOfTiles", number_of_tiles);
}

void RecordImpressionTileTypes(
    const std::vector<MostVisitedTileType>& tile_types,
    const std::vector<NTPTileSource>& sources) {
  int counts_per_type[NUM_RECORDED_TILE_TYPES] = {0};
  for (size_t i = 0; i < tile_types.size(); ++i) {
    MostVisitedTileType tile_type = tile_types[i];
    DCHECK_LT(tile_type, NUM_RECORDED_TILE_TYPES);
    ++counts_per_type[tile_type];

    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileType", tile_type,
                              NUM_RECORDED_TILE_TYPES);

    std::string histogram = base::StringPrintf(
        "NewTabPage.TileType.%s", GetSourceHistogramName(sources[i]).c_str());
    LogHistogramEvent(histogram, tile_type, NUM_RECORDED_TILE_TYPES);
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsReal",
                              counts_per_type[ICON_REAL]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsColor",
                              counts_per_type[ICON_COLOR]);
  UMA_HISTOGRAM_SPARSE_SLOWLY("NewTabPage.IconsGray",
                              counts_per_type[ICON_DEFAULT]);
}

void RecordTileClick(int index,
                     NTPTileSource source,
                     MostVisitedTileType tile_type) {
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.MostVisited", index, kMaxNumTiles);

  std::string histogram = base::StringPrintf(
      "NewTabPage.MostVisited.%s", GetSourceHistogramName(source).c_str());
  LogHistogramEvent(histogram, index, kMaxNumTiles);

  if (tile_type < NUM_RECORDED_TILE_TYPES) {
    UMA_HISTOGRAM_ENUMERATION("NewTabPage.TileTypeClicked", tile_type,
                              NUM_RECORDED_TILE_TYPES);

    std::string histogram =
        base::StringPrintf("NewTabPage.TileTypeClicked.%s",
                           GetSourceHistogramName(source).c_str());
    LogHistogramEvent(histogram, tile_type, NUM_RECORDED_TILE_TYPES);
  }
}

}  // namespace metrics
}  // namespace ntp_tiles
