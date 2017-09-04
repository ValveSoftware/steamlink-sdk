// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/ProgressTracker.h"

#include "core/frame/Settings.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ProgressClient : public EmptyFrameLoaderClient {
 public:
  ProgressClient() : m_lastProgress(0.0) {}

  void progressEstimateChanged(double progressEstimate) override {
    m_lastProgress = progressEstimate;
  }

  double lastProgress() const { return m_lastProgress; }

 private:
  double m_lastProgress;
};

class ProgressTrackerTest : public ::testing::Test {
 public:
  ProgressTrackerTest()
      : m_response(KURL(ParsedURLString, "http://example.com"),
                   "text/html",
                   1024,
                   nullAtom,
                   String()) {}

  void SetUp() override {
    m_client = new ProgressClient;
    m_dummyPageHolder =
        DummyPageHolder::create(IntSize(800, 600), nullptr, m_client.get());
    frame().settings()->setProgressBarCompletion(
        ProgressBarCompletion::ResourcesBeforeDCL);
  }

  LocalFrame& frame() const { return m_dummyPageHolder->frame(); }
  ProgressTracker& progress() const { return frame().loader().progress(); }
  double lastProgress() const { return m_client->lastProgress(); }
  const ResourceResponse& responseHeaders() const { return m_response; }

  // Reports a 1024-byte "main resource" (VeryHigh priority) request/response
  // to ProgressTracker with identifier 1, but tests are responsible for
  // emulating payload and load completion.
  void emulateMainResourceRequestAndResponse() const {
    progress().progressStarted();
    progress().willStartLoading(1ul, ResourceLoadPriorityVeryHigh);
    EXPECT_EQ(0.0, lastProgress());
    progress().incrementProgress(1ul, responseHeaders());
    EXPECT_EQ(0.0, lastProgress());
  }

 private:
  Persistent<ProgressClient> m_client;
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  ResourceResponse m_response;
};

TEST_F(ProgressTrackerTest, Static) {
  progress().progressStarted();
  EXPECT_EQ(0.0, lastProgress());
  progress().finishedParsing();
  EXPECT_EQ(1.0, lastProgress());
}

TEST_F(ProgressTrackerTest, MainResourceOnly) {
  emulateMainResourceRequestAndResponse();

  // .2 for committing, .25 out of .5 possible for bytes received.
  progress().incrementProgress(1ul, 512);
  EXPECT_EQ(0.45, lastProgress());

  // .2 for finishing parsing, .5 for all bytes received.
  progress().completeProgress(1ul);
  EXPECT_EQ(0.7, lastProgress());

  progress().finishedParsing();
  EXPECT_EQ(1.0, lastProgress());
}

TEST_F(ProgressTrackerTest, WithHighPriorirySubresource) {
  emulateMainResourceRequestAndResponse();

  progress().willStartLoading(2ul, ResourceLoadPriorityHigh);
  progress().incrementProgress(2ul, responseHeaders());
  EXPECT_EQ(0.0, lastProgress());

  // .2 for committing, .25 out of .5 possible for bytes received.
  progress().incrementProgress(1ul, 1024);
  progress().completeProgress(1ul);
  EXPECT_EQ(0.45, lastProgress());

  // .4 for finishing parsing, .25 out of .5 possible for bytes received.
  progress().finishedParsing();
  EXPECT_EQ(0.65, lastProgress());

  progress().completeProgress(2ul);
  EXPECT_EQ(1.0, lastProgress());
}

TEST_F(ProgressTrackerTest, WithMediumPrioritySubresource) {
  emulateMainResourceRequestAndResponse();

  progress().willStartLoading(2ul, ResourceLoadPriorityMedium);
  progress().incrementProgress(2ul, responseHeaders());
  EXPECT_EQ(0.0, lastProgress());

  // .2 for finishing parsing, .5 for all bytes received.
  // Medium priority resource is ignored.
  progress().completeProgress(1ul);
  EXPECT_EQ(0.7, lastProgress());

  progress().finishedParsing();
  EXPECT_EQ(1.0, lastProgress());
}

}  // namespace blink
