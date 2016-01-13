// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/simple_test_tick_clock.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/media/render_media_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class RenderMediaLogTest : public testing::Test {
 public:
  RenderMediaLogTest()
      : log_(new RenderMediaLog()),
        tick_clock_(new base::SimpleTestTickClock()) {
    log_->SetTickClockForTesting(scoped_ptr<base::TickClock>(tick_clock_));
  }

  virtual ~RenderMediaLogTest() {}

  void AddEvent(media::MediaLogEvent::Type type) {
    log_->AddEvent(log_->CreateEvent(type));
  }

  void Advance(base::TimeDelta delta) { tick_clock_->Advance(delta); }

  int message_count() { return render_thread_.sink().message_count(); }

  std::vector<media::MediaLogEvent> GetMediaLogEvents() {
    const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
        ViewHostMsg_MediaLogEvents::ID);
    if (!msg) {
      ADD_FAILURE() << "Did not find ViewHostMsg_MediaLogEvents IPC message";
      return std::vector<media::MediaLogEvent>();
    }

    Tuple1<std::vector<media::MediaLogEvent> > events;
    ViewHostMsg_MediaLogEvents::Read(msg, &events);
    return events.a;
  }

 private:
  MockRenderThread render_thread_;
  scoped_refptr<RenderMediaLog> log_;
  base::SimpleTestTickClock* tick_clock_;  // Owned by |log_|.

  DISALLOW_COPY_AND_ASSIGN(RenderMediaLogTest);
};

TEST_F(RenderMediaLogTest, ThrottleSendingEvents) {
  AddEvent(media::MediaLogEvent::LOAD);
  EXPECT_EQ(0, message_count());

  // Still shouldn't send anything.
  Advance(base::TimeDelta::FromMilliseconds(500));
  AddEvent(media::MediaLogEvent::SEEK);
  EXPECT_EQ(0, message_count());

  // Now we should expect an IPC.
  Advance(base::TimeDelta::FromMilliseconds(500));
  AddEvent(media::MediaLogEvent::PLAY);
  EXPECT_EQ(1, message_count());

  // Verify contents.
  std::vector<media::MediaLogEvent> events = GetMediaLogEvents();
  ASSERT_EQ(3u, events.size());
  EXPECT_EQ(media::MediaLogEvent::LOAD, events[0].type);
  EXPECT_EQ(media::MediaLogEvent::SEEK, events[1].type);
  EXPECT_EQ(media::MediaLogEvent::PLAY, events[2].type);

  // Adding another event shouldn't send anything.
  AddEvent(media::MediaLogEvent::PIPELINE_ERROR);
  EXPECT_EQ(1, message_count());
}

TEST_F(RenderMediaLogTest, BufferedExtents) {
  AddEvent(media::MediaLogEvent::LOAD);
  AddEvent(media::MediaLogEvent::SEEK);

  // This event is handled separately and should always appear last regardless
  // of how many times we see it.
  AddEvent(media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED);
  AddEvent(media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED);
  AddEvent(media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED);

  // Trigger IPC message.
  EXPECT_EQ(0, message_count());
  Advance(base::TimeDelta::FromMilliseconds(1000));
  AddEvent(media::MediaLogEvent::PLAY);
  EXPECT_EQ(1, message_count());

  // Verify contents. There should only be a single buffered extents changed
  // event.
  std::vector<media::MediaLogEvent> events = GetMediaLogEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_EQ(media::MediaLogEvent::LOAD, events[0].type);
  EXPECT_EQ(media::MediaLogEvent::SEEK, events[1].type);
  EXPECT_EQ(media::MediaLogEvent::PLAY, events[2].type);
  EXPECT_EQ(media::MediaLogEvent::BUFFERED_EXTENTS_CHANGED, events[3].type);
}

}  // namespace content
