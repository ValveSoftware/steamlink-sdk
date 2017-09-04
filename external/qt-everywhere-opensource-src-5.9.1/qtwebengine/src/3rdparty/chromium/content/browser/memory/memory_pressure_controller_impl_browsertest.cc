// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/browser/memory/memory_pressure_controller_impl.h"
#include "content/common/memory_messages.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

MATCHER_P(IsSetSuppressedMessage, suppressed, "") {
  // Ensure that the message is deleted upon return.
  std::unique_ptr<IPC::Message> message(arg);
  if (message == nullptr)
    return false;
  MemoryMsg_SetPressureNotificationsSuppressed::Param param;
  if (!MemoryMsg_SetPressureNotificationsSuppressed::Read(message.get(),
                                                          &param))
    return false;
  return suppressed == std::get<0>(param);
}

MATCHER_P(IsSimulateMessage, level, "") {
  // Ensure that the message is deleted upon return.
  std::unique_ptr<IPC::Message> message(arg);
  if (message == nullptr)
    return false;
  MemoryMsg_SimulatePressureNotification::Param param;
  if (!MemoryMsg_SimulatePressureNotification::Read(message.get(), &param))
    return false;
  return level == std::get<0>(param);
}

MATCHER_P(IsPressureMessage, level, "") {
  // Ensure that the message is deleted upon return.
  std::unique_ptr<IPC::Message> message(arg);
  if (message == nullptr)
    return false;
  MemoryMsg_PressureNotification::Param param;
  if (!MemoryMsg_PressureNotification::Read(message.get(), &param))
    return false;
  return level == std::get<0>(param);
}

class MemoryMessageFilterForTesting : public MemoryMessageFilter {
 public:
  // Use this object itself as a fake RenderProcessHost pointer. The address is
  // only used for looking up the message filter in the controller and is never
  // actually dereferenced, so this is safe.
  MemoryMessageFilterForTesting()
      : MemoryMessageFilter(reinterpret_cast<RenderProcessHost*>(this)) {}

  MOCK_METHOD1(Send, bool(IPC::Message* message));

  void Add() {
    // The filter must be added on the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MemoryMessageFilterForTesting::Add,
                                         base::Unretained(this)));
      RunAllPendingInMessageLoop(BrowserThread::IO);
      return;
    }
    OnChannelConnected(0);
    OnFilterAdded(nullptr);
  }

  void Remove() {
    // The filter must be removed on the IO thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MemoryMessageFilterForTesting::Remove,
                                         base::Unretained(this)));
      RunAllPendingInMessageLoop(BrowserThread::IO);
      return;
    }
    OnChannelClosing();
    OnFilterRemoved();
  }

 protected:
  ~MemoryMessageFilterForTesting() override {}
};

class MemoryPressureControllerImplBrowserTest : public ContentBrowserTest {
 public:
  MOCK_METHOD1(OnMemoryPressure,
               void(base::MemoryPressureListener::MemoryPressureLevel level));

 protected:
  void SetPressureNotificationsSuppressedInAllProcessesAndWait(
      bool suppressed) {
    MemoryPressureControllerImpl::GetInstance()
        ->SetPressureNotificationsSuppressedInAllProcesses(suppressed);
    RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void SimulatePressureNotificationInAllProcessesAndWait(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    MemoryPressureControllerImpl::GetInstance()
        ->SimulatePressureNotificationInAllProcesses(level);
    RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void SendPressureNotificationAndWait(
      const void* fake_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level) {
    MemoryPressureControllerImpl::GetInstance()->SendPressureNotification(
        reinterpret_cast<const RenderProcessHost*>(fake_process_host), level);
    RunAllPendingInMessageLoop(BrowserThread::IO);
  }
};

const auto MEMORY_PRESSURE_LEVEL_MODERATE =
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
const auto MEMORY_PRESSURE_LEVEL_CRITICAL =
    base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

IN_PROC_BROWSER_TEST_F(MemoryPressureControllerImplBrowserTest,
                       SetPressureNotificationsSuppressedInAllProcesses) {
  scoped_refptr<MemoryMessageFilterForTesting> filter1(
      new MemoryMessageFilterForTesting);
  scoped_refptr<MemoryMessageFilterForTesting> filter2(
      new MemoryMessageFilterForTesting);

  NavigateToURL(shell(), GetTestUrl("", "title.html"));

  // Add the first filter. No messages should be sent because notifications are
  // not suppressed.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter1->Add();
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
  testing::Mock::VerifyAndClearExpectations(this);

  // Enable suppressing memory pressure notifications in all processes. The
  // first filter should send a message.
  EXPECT_CALL(*filter1, Send(IsSetSuppressedMessage(true))).Times(1);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  testing::Mock::VerifyAndClearExpectations(this);

  // Add the second filter. It should send a message because notifications are
  // suppressed.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(true))).Times(1);
  filter2->Add();
  testing::Mock::VerifyAndClearExpectations(this);

  // Send a memory pressure event to the first child process. No messages should
  // be sent as the notifications are suppressed.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  SendPressureNotificationAndWait(filter1.get(),
                                  MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(this);

  // Disable suppressing memory pressure notifications in all processes. Both
  // filters should send a message.
  EXPECT_CALL(*filter1, Send(IsSetSuppressedMessage(false))).Times(1);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(false))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
  testing::Mock::VerifyAndClearExpectations(this);

  // Send a memory pressure event to the first child process. A message should
  // be received as messages are not being suppressed.
  EXPECT_CALL(*filter1, Send(IsPressureMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  SendPressureNotificationAndWait(filter1.get(),
                                  MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(this);

  // Send a memory pressure event to a non-existing child process. No message
  // should be sent.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  SendPressureNotificationAndWait(reinterpret_cast<const void*>(0xF005BA11),
                                  MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(this);

  // Remove the first filter. No messages should be sent.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter1->Remove();
  testing::Mock::VerifyAndClearExpectations(this);

  // Enable suppressing memory pressure notifications in all processes. The
  // second filter should send a message.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(IsSetSuppressedMessage(true))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);
  EXPECT_TRUE(base::MemoryPressureListener::AreNotificationsSuppressed());
  testing::Mock::VerifyAndClearExpectations(this);

  // Remove the second filter and disable suppressing memory pressure
  // notifications in all processes. No messages should be sent.
  EXPECT_CALL(*filter1, Send(testing::_)).Times(0);
  EXPECT_CALL(*filter2, Send(testing::_)).Times(0);
  filter2->Remove();
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);
  EXPECT_FALSE(base::MemoryPressureListener::AreNotificationsSuppressed());
  testing::Mock::VerifyAndClearExpectations(this);
}

IN_PROC_BROWSER_TEST_F(MemoryPressureControllerImplBrowserTest,
                       SimulatePressureNotificationInAllProcesses) {
  scoped_refptr<MemoryMessageFilterForTesting> filter(
      new MemoryMessageFilterForTesting);
  std::unique_ptr<base::MemoryPressureListener> listener(
      new base::MemoryPressureListener(
          base::Bind(&MemoryPressureControllerImplBrowserTest::OnMemoryPressure,
                     base::Unretained(this))));

  NavigateToURL(shell(), GetTestUrl("", "title.html"));

  filter->Add();

  // Send a memory pressure event to the first child process. It should send a
  // pressure notification message.
  EXPECT_CALL(*filter, Send(IsPressureMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  SendPressureNotificationAndWait(filter.get(), MEMORY_PRESSURE_LEVEL_MODERATE);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_CRITICAL)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_CRITICAL)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_CRITICAL);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.
  testing::Mock::VerifyAndClearExpectations(this);

  // Enable suppressing memory pressure notifications in all processes. This
  // should have no impact on simulating memory pressure notifications.
  EXPECT_CALL(*filter, Send(IsSetSuppressedMessage(true))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(true);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.
  testing::Mock::VerifyAndClearExpectations(this);

  // Disable suppressing memory pressure notifications in all processes. This
  // should have no impact on simulating memory pressure notifications.
  EXPECT_CALL(*filter, Send(IsSetSuppressedMessage(false))).Times(1);
  SetPressureNotificationsSuppressedInAllProcessesAndWait(false);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*filter, Send(IsSimulateMessage(MEMORY_PRESSURE_LEVEL_MODERATE)))
      .Times(1);
  EXPECT_CALL(*this, OnMemoryPressure(MEMORY_PRESSURE_LEVEL_MODERATE)).Times(1);
  SimulatePressureNotificationInAllProcessesAndWait(
      MEMORY_PRESSURE_LEVEL_MODERATE);
  RunAllPendingInMessageLoop();  // Wait for the listener to run.
  testing::Mock::VerifyAndClearExpectations(this);

  filter->Remove();
}

}  // namespace content
