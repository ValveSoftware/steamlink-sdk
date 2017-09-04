// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/track/TextTrackList.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextTrackListTest, InvalidateTrackIndexes) {
  // Create and fill the list
  TextTrackList* list = TextTrackList::create(nullptr);
  const size_t numTextTracks = 4;
  TextTrack* textTracks[numTextTracks];
  for (size_t i = 0; i < numTextTracks; ++i) {
    textTracks[i] = TextTrack::create("subtitles", "", "");
    list->append(textTracks[i]);
  }

  EXPECT_EQ(4u, list->length());
  EXPECT_EQ(0, textTracks[0]->trackIndex());
  EXPECT_EQ(1, textTracks[1]->trackIndex());
  EXPECT_EQ(2, textTracks[2]->trackIndex());
  EXPECT_EQ(3, textTracks[3]->trackIndex());

  // Remove element from the middle of the list
  list->remove(textTracks[1]);

  EXPECT_EQ(3u, list->length());
  EXPECT_EQ(nullptr, textTracks[1]->trackList());
  EXPECT_EQ(0, textTracks[0]->trackIndex());
  EXPECT_EQ(1, textTracks[2]->trackIndex());
  EXPECT_EQ(2, textTracks[3]->trackIndex());
}

}  // namespace blink
