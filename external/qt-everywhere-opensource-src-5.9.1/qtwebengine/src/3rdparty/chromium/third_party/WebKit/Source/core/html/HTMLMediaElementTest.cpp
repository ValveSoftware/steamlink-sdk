// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMediaElement.h"

#include "core/html/HTMLAudioElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

enum class TestParam { Audio, Video };

class HTMLMediaElementTest : public ::testing::TestWithParam<TestParam> {
 protected:
  void SetUp() {
    m_dummyPageHolder = DummyPageHolder::create();

    if (GetParam() == TestParam::Audio)
      m_media = HTMLAudioElement::create(m_dummyPageHolder->document());
    else
      m_media = HTMLVideoElement::create(m_dummyPageHolder->document());
  }

  HTMLMediaElement* media() { return m_media.get(); }

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<HTMLMediaElement> m_media;
};

INSTANTIATE_TEST_CASE_P(Audio,
                        HTMLMediaElementTest,
                        ::testing::Values(TestParam::Audio));
INSTANTIATE_TEST_CASE_P(Video,
                        HTMLMediaElementTest,
                        ::testing::Values(TestParam::Video));

TEST_P(HTMLMediaElementTest, effectiveMediaVolume) {
  struct TestData {
    double volume;
    bool muted;
    double effectiveVolume;
  } test_data[] = {
      {0.0, false, 0.0}, {0.5, false, 0.5}, {1.0, false, 1.0},
      {0.0, true, 0.0},  {0.5, true, 0.0},  {1.0, true, 0.0},
  };

  for (const auto& data : test_data) {
    media()->setVolume(data.volume);
    media()->setMuted(data.muted);
    EXPECT_EQ(data.effectiveVolume, media()->effectiveMediaVolume());
  }
}

}  // namespace blink
