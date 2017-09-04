// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunk.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Optional.h"

namespace blink {

TEST(PaintChunkTest, matchesSame) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.updateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_TRUE(PaintChunk(0, 1, &id, properties)
                  .matches(PaintChunk(0, 1, &id, properties)));
}

TEST(PaintChunkTest, matchesEqual) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.updateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  DisplayItem::Id idEqual = id;
  EXPECT_TRUE(PaintChunk(0, 1, &id, properties)
                  .matches(PaintChunk(0, 1, &idEqual, properties)));
  EXPECT_TRUE(PaintChunk(0, 1, &idEqual, properties)
                  .matches(PaintChunk(0, 1, &id, properties)));
}

TEST(PaintChunkTest, IdNotMatches) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client1;
  client1.updateCacheGeneration();
  DisplayItem::Id id1a(client1, DisplayItem::kDrawingFirst);
  DisplayItem::Id id1b(client1, DisplayItem::kSubsequence);
  EXPECT_FALSE(PaintChunk(0, 1, &id1a, properties)
                   .matches(PaintChunk(0, 1, &id1b, properties)));
  EXPECT_FALSE(PaintChunk(0, 1, &id1b, properties)
                   .matches(PaintChunk(0, 1, &id1a, properties)));

  FakeDisplayItemClient client2;
  client2.updateCacheGeneration();
  DisplayItem::Id id2(client2, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, &id1a, properties)
                   .matches(PaintChunk(0, 1, &id2, properties)));
  EXPECT_FALSE(PaintChunk(0, 1, &id2, properties)
                   .matches(PaintChunk(0, 1, &id1a, properties)));
}

TEST(PaintChunkTest, IdNotMatchesNull) {
  PaintChunkProperties properties;
  FakeDisplayItemClient client;
  client.updateCacheGeneration();
  DisplayItem::Id id(client, DisplayItem::kDrawingFirst);
  EXPECT_FALSE(PaintChunk(0, 1, nullptr, properties)
                   .matches(PaintChunk(0, 1, &id, properties)));
  EXPECT_FALSE(PaintChunk(0, 1, &id, properties)
                   .matches(PaintChunk(0, 1, nullptr, properties)));
  EXPECT_FALSE(PaintChunk(0, 1, nullptr, properties)
                   .matches(PaintChunk(0, 1, nullptr, properties)));
}

TEST(PaintChunkTest, IdNotMatchesJustCreated) {
  PaintChunkProperties properties;
  Optional<FakeDisplayItemClient> client;
  client.emplace();
  EXPECT_TRUE(client->isJustCreated());
  // Invalidation won't change the "just created" status.
  client->setDisplayItemsUncached();
  EXPECT_TRUE(client->isJustCreated());

  DisplayItem::Id id(*client, DisplayItem::kDrawingFirst);
  // A chunk of a newly created client doesn't match any chunk because it's
  // never cached.
  EXPECT_FALSE(PaintChunk(0, 1, &id, properties)
                   .matches(PaintChunk(0, 1, &id, properties)));

  client->updateCacheGeneration();
  EXPECT_TRUE(PaintChunk(0, 1, &id, properties)
                  .matches(PaintChunk(0, 1, &id, properties)));

  // Delete the current object and create a new object at the same address.
  client = WTF::nullopt;
  client.emplace();
  EXPECT_TRUE(client->isJustCreated());
  EXPECT_FALSE(PaintChunk(0, 1, &id, properties)
                   .matches(PaintChunk(0, 1, &id, properties)));
}

}  // namespace blink
