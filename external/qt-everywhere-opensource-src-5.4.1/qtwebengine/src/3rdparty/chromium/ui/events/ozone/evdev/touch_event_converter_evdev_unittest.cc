// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/touch_event_converter_evdev.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"

namespace {

static int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

const char kTestDevicePath[] = "/dev/input/test-device";

}  // namespace

namespace ui {

class MockTouchEventConverterEvdev : public TouchEventConverterEvdev {
 public:
  MockTouchEventConverterEvdev(int fd, base::FilePath path);
  virtual ~MockTouchEventConverterEvdev() {};

  void ConfigureReadMock(struct input_event* queue,
                         long read_this_many,
                         long queue_index);

  unsigned size() { return dispatched_events_.size(); }
  TouchEvent* event(unsigned index) { return dispatched_events_[index]; }

  // Actually dispatch the event reader code.
  void ReadNow() {
    OnFileCanReadWithoutBlocking(read_pipe_);
    base::RunLoop().RunUntilIdle();
  }

  void DispatchCallback(Event* event) {
    dispatched_events_.push_back(
        new TouchEvent(*static_cast<TouchEvent*>(event)));
  }

  virtual bool Reinitialize() OVERRIDE { return true; }

 private:
  int read_pipe_;
  int write_pipe_;

  ScopedVector<TouchEvent> dispatched_events_;

  DISALLOW_COPY_AND_ASSIGN(MockTouchEventConverterEvdev);
};

MockTouchEventConverterEvdev::MockTouchEventConverterEvdev(int fd,
                                                           base::FilePath path)
    : TouchEventConverterEvdev(
          fd,
          path,
          EventDeviceInfo(),
          base::Bind(&MockTouchEventConverterEvdev::DispatchCallback,
                     base::Unretained(this))) {
  pressure_min_ = 30;
  pressure_max_ = 60;

  // TODO(rjkroege): Check test axes.
  x_min_pixels_ = x_min_tuxels_ = 0;
  x_num_pixels_ = x_num_tuxels_ = std::numeric_limits<int>::max();
  y_min_pixels_ = y_min_tuxels_ = 0;
  y_num_pixels_ = y_num_tuxels_ = std::numeric_limits<int>::max();

  int fds[2];

  if (pipe(fds))
    PLOG(FATAL) << "failed pipe";

  DCHECK(SetNonBlocking(fds[0]) == 0)
      << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
  DCHECK(SetNonBlocking(fds[1]) == 0)
      << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
  read_pipe_ = fds[0];
  write_pipe_ = fds[1];
}

void MockTouchEventConverterEvdev::ConfigureReadMock(struct input_event* queue,
                                                     long read_this_many,
                                                     long queue_index) {
  int nwrite = HANDLE_EINTR(write(write_pipe_,
                                  queue + queue_index,
                                  sizeof(struct input_event) * read_this_many));
  DCHECK(nwrite ==
         static_cast<int>(sizeof(struct input_event) * read_this_many))
      << "write() failed, errno: " << errno;
}

}  // namespace ui

// Test fixture.
class TouchEventConverterEvdevTest : public testing::Test {
 public:
  TouchEventConverterEvdevTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    // Set up pipe to satisfy message pump (unused).
    int evdev_io[2];
    if (pipe(evdev_io))
      PLOG(FATAL) << "failed pipe";
    events_in_ = evdev_io[0];
    events_out_ = evdev_io[1];

    loop_ = new base::MessageLoopForUI;
    device_ = new ui::MockTouchEventConverterEvdev(
        events_in_, base::FilePath(kTestDevicePath));
  }

  virtual void TearDown() OVERRIDE {
    delete device_;
    delete loop_;
  }

  ui::MockTouchEventConverterEvdev* device() { return device_; }

 private:
  base::MessageLoop* loop_;
  ui::MockTouchEventConverterEvdev* device_;

  int events_out_;
  int events_in_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventConverterEvdevTest);
};

// TODO(rjkroege): Test for valid handling of time stamps.
TEST_F(TouchEventConverterEvdevTest, TouchDown) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ConfigureReadMock(mock_kernel_queue, 1, 0);
  dev->ReadNow();
  EXPECT_EQ(0u, dev->size());

  dev->ConfigureReadMock(mock_kernel_queue, 2, 1);
  dev->ReadNow();
  EXPECT_EQ(0u, dev->size());

  dev->ConfigureReadMock(mock_kernel_queue, 3, 3);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());

  ui::TouchEvent* event = dev->event(0);
  EXPECT_FALSE(event == NULL);

  EXPECT_EQ(ui::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event->time_stamp());
  EXPECT_EQ(42, event->x());
  EXPECT_EQ(51, event->y());
  EXPECT_EQ(0, event->touch_id());
  EXPECT_FLOAT_EQ(.5f, event->force());
  EXPECT_FLOAT_EQ(0.f, event->rotation_angle());
}

TEST_F(TouchEventConverterEvdevTest, NoEvents) {
  ui::MockTouchEventConverterEvdev* dev = device();
  dev->ConfigureReadMock(NULL, 0, 0);
  EXPECT_EQ(0u, dev->size());
}

TEST_F(TouchEventConverterEvdevTest, TouchMove) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 50},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 43}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_move2[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 42}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());

  dev->ConfigureReadMock(mock_kernel_queue_move1, 4, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, dev->size());
  ui::TouchEvent* event = dev->event(1);
  EXPECT_FALSE(event == NULL);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, event->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event->time_stamp());
  EXPECT_EQ(42, event->x());
  EXPECT_EQ(43, event->y());
  EXPECT_EQ(0, event->touch_id());
  EXPECT_FLOAT_EQ(2.f / 3.f, event->force());
  EXPECT_FLOAT_EQ(0.f, event->rotation_angle());

  dev->ConfigureReadMock(mock_kernel_queue_move2, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(3u, dev->size());
  event = dev->event(2);
  EXPECT_FALSE(event == NULL);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, event->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event->time_stamp());
  EXPECT_EQ(42, event->x());
  EXPECT_EQ(42, event->y());
  EXPECT_EQ(0, event->touch_id());
  EXPECT_FLOAT_EQ(2.f / 3.f, event->force());
  EXPECT_FLOAT_EQ(0.f, event->rotation_angle());
}

TEST_F(TouchEventConverterEvdevTest, TouchRelease) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  struct input_event mock_kernel_queue_release[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());
  ui::TouchEvent* event = dev->event(0);
  EXPECT_FALSE(event == NULL);

  dev->ConfigureReadMock(mock_kernel_queue_release, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, dev->size());
  event = dev->event(1);
  EXPECT_FALSE(event == NULL);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), event->time_stamp());
  EXPECT_EQ(42, event->x());
  EXPECT_EQ(51, event->y());
  EXPECT_EQ(0, event->touch_id());
  EXPECT_FLOAT_EQ(.5f, event->force());
  EXPECT_FLOAT_EQ(0.f, event->rotation_angle());
}

TEST_F(TouchEventConverterEvdevTest, TwoFingerGesture) {
  ui::MockTouchEventConverterEvdev* dev = device();

  ui::TouchEvent* ev0;
  ui::TouchEvent* ev1;

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Setup and discard a press.
  dev->ConfigureReadMock(mock_kernel_queue_press0, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());

  struct input_event mock_kernel_queue_move0[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Setup and discard a move.
  dev->ConfigureReadMock(mock_kernel_queue_move0, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, dev->size());

  struct input_event mock_kernel_queue_move0press1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0},
    {{0, 0}, EV_ABS, ABS_MT_SLOT, 1}, {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 686},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 101},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 102}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  // Move on 0, press on 1.
  dev->ConfigureReadMock(mock_kernel_queue_move0press1, 9, 0);
  dev->ReadNow();
  EXPECT_EQ(4u, dev->size());
  ev0 = dev->event(2);
  ev1 = dev->event(3);

  // Move
  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev0->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0->time_stamp());
  EXPECT_EQ(40, ev0->x());
  EXPECT_EQ(51, ev0->y());
  EXPECT_EQ(0, ev0->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev0->force());
  EXPECT_FLOAT_EQ(0.f, ev0->rotation_angle());

  // Press
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, ev1->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1->time_stamp());
  EXPECT_EQ(101, ev1->x());
  EXPECT_EQ(102, ev1->y());
  EXPECT_EQ(1, ev1->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev1->force());
  EXPECT_FLOAT_EQ(0.f, ev1->rotation_angle());

  // Stationary 0, Moves 1.
  struct input_event mock_kernel_queue_stationary0_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_stationary0_move1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(5u, dev->size());
  ev1 = dev->event(4);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev1->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1->time_stamp());
  EXPECT_EQ(40, ev1->x());
  EXPECT_EQ(102, ev1->y());
  EXPECT_EQ(1, ev1->touch_id());

  EXPECT_FLOAT_EQ(.5f, ev1->force());
  EXPECT_FLOAT_EQ(0.f, ev1->rotation_angle());

  // Move 0, stationary 1.
  struct input_event mock_kernel_queue_move0_stationary1[] = {
    {{0, 0}, EV_ABS, ABS_MT_SLOT, 0}, {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 39},
    {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_move0_stationary1, 3, 0);
  dev->ReadNow();
  EXPECT_EQ(6u, dev->size());
  ev0 = dev->event(5);

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev0->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0->time_stamp());
  EXPECT_EQ(39, ev0->x());
  EXPECT_EQ(51, ev0->y());
  EXPECT_EQ(0, ev0->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev0->force());
  EXPECT_FLOAT_EQ(0.f, ev0->rotation_angle());

  // Release 0, move 1.
  struct input_event mock_kernel_queue_release0_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_ABS, ABS_MT_SLOT, 1},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 38}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };
  dev->ConfigureReadMock(mock_kernel_queue_release0_move1, 4, 0);
  dev->ReadNow();
  EXPECT_EQ(8u, dev->size());
  ev0 = dev->event(6);
  ev1 = dev->event(7);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ev0->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev0->time_stamp());
  EXPECT_EQ(39, ev0->x());
  EXPECT_EQ(51, ev0->y());
  EXPECT_EQ(0, ev0->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev0->force());
  EXPECT_FLOAT_EQ(0.f, ev0->rotation_angle());

  EXPECT_EQ(ui::ET_TOUCH_MOVED, ev1->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1->time_stamp());
  EXPECT_EQ(38, ev1->x());
  EXPECT_EQ(102, ev1->y());
  EXPECT_EQ(1, ev1->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev1->force());
  EXPECT_FLOAT_EQ(0.f, ev1->rotation_angle());

  // Release 1.
  struct input_event mock_kernel_queue_release1[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, -1}, {{0, 0}, EV_SYN, SYN_REPORT, 0},
  };
  dev->ConfigureReadMock(mock_kernel_queue_release1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(9u, dev->size());
  ev1 = dev->event(8);

  EXPECT_EQ(ui::ET_TOUCH_RELEASED, ev1->type());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(0), ev1->time_stamp());
  EXPECT_EQ(38, ev1->x());
  EXPECT_EQ(102, ev1->y());
  EXPECT_EQ(1, ev1->touch_id());
  EXPECT_FLOAT_EQ(.5f, ev1->force());
  EXPECT_FLOAT_EQ(0.f, ev1->rotation_angle());
}

TEST_F(TouchEventConverterEvdevTest, TypeA) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51},
    {{0, 0}, EV_SYN, SYN_MT_REPORT, 0},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 61},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 71},
    {{0, 0}, EV_SYN, SYN_MT_REPORT, 0},
    {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Check that two events are generated.
  dev->ConfigureReadMock(mock_kernel_queue_press0, 10, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, dev->size());
}

TEST_F(TouchEventConverterEvdevTest, Unsync) {
  ui::MockTouchEventConverterEvdev* dev = device();

  struct input_event mock_kernel_queue_press0[] = {
    {{0, 0}, EV_ABS, ABS_MT_TRACKING_ID, 684},
    {{0, 0}, EV_ABS, ABS_MT_TOUCH_MAJOR, 3},
    {{0, 0}, EV_ABS, ABS_MT_PRESSURE, 45},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 42},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_Y, 51}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  dev->ConfigureReadMock(mock_kernel_queue_press0, 6, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());

  // Prepare a move with a drop.
  struct input_event mock_kernel_queue_move0[] = {
    {{0, 0}, EV_SYN, SYN_DROPPED, 0},
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Verify that we didn't receive it/
  dev->ConfigureReadMock(mock_kernel_queue_move0, 3, 0);
  dev->ReadNow();
  EXPECT_EQ(1u, dev->size());

  struct input_event mock_kernel_queue_move1[] = {
    {{0, 0}, EV_ABS, ABS_MT_POSITION_X, 40}, {{0, 0}, EV_SYN, SYN_REPORT, 0}
  };

  // Verify that it re-syncs after a SYN_REPORT.
  dev->ConfigureReadMock(mock_kernel_queue_move1, 2, 0);
  dev->ReadNow();
  EXPECT_EQ(2u, dev->size());
}
