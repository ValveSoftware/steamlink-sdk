// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptValueSerializer.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/layout/LayoutTestHelper.h"
#include "wtf/Alignment.h"

namespace blink {

class ScriptValueSerializerTest : public RenderingTest {
 public:
  FrameSettingOverrideFunction settingOverrider() const override {
    return &overrideSettings;
  }

 private:
  static void overrideSettings(Settings& settings) {
    settings.setScriptEnabled(true);
  }
};

TEST_F(ScriptValueSerializerTest, Uint64Decode) {
  // Even large 64-bit integers should decode properly.
  const uint64_t expected = 0x123456789abcdef0;
  WTF_ALIGNED(const uint8_t, buffer[], sizeof(UChar)) = {
      0xf0, 0xbd, 0xf3, 0xd5, 0x89, 0xcf, 0x95, 0x9a, 0x92, 0x00};
  BlobDataHandleMap blobDataHandleMap;
  SerializedScriptValueReader reader(
      buffer, sizeof(buffer), nullptr, blobDataHandleMap,
      ScriptState::forMainWorld(document().frame()));
  uint64_t actual;
  ASSERT_TRUE(reader.doReadUint64(&actual));
  EXPECT_EQ(expected, actual);
}

}  // namespace blink
