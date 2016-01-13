// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

// Include views_test_base.h first because the definition of None in X.h
// conflicts with the definition of None in gtest-type-util.h
#include "ui/views/test/views_test_base.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_aurax11.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

#include <X11/Xlib.h>

namespace views {

namespace {

const char* kAtomsToCache[] = {
  "XdndActionCopy",
  "XdndDrop",
  "XdndEnter",
  "XdndFinished",
  "XdndLeave",
  "XdndPosition",
  "XdndStatus",
  "XdndTypeList",
  NULL
};

class TestDragDropClient;

// Collects messages which would otherwise be sent to |xid_| via
// SendXClientEvent().
class ClientMessageEventCollector {
 public:
  ClientMessageEventCollector(::Window xid, TestDragDropClient* client);
  virtual ~ClientMessageEventCollector();

  // Returns true if |events_| is non-empty.
  bool HasEvents() const {
    return !events_.empty();
  }

  // Pops all of |events_| and returns the popped events in the order that they
  // were on the stack
  std::vector<XClientMessageEvent> PopAllEvents();

  // Adds |event| to the stack.
  void RecordEvent(const XClientMessageEvent& event);

 private:
  ::Window xid_;

  // Not owned.
  TestDragDropClient* client_;

  std::vector<XClientMessageEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(ClientMessageEventCollector);
};

// Implementation of DesktopDragDropClientAuraX11 which works with a fake
// |DesktopDragDropClientAuraX11::source_current_window_|.
class TestDragDropClient : public DesktopDragDropClientAuraX11 {
 public:
  // The location in screen coordinates used for the synthetic mouse moves
  // generated in SetTopmostXWindowAndMoveMouse().
  static const int kMouseMoveX;
  static const int kMouseMoveY;

  TestDragDropClient(aura::Window* window,
                     DesktopNativeCursorManager* cursor_manager);
  virtual ~TestDragDropClient();

  // Returns the XID of the window which initiated the drag.
  ::Window source_xwindow() {
    return source_xid_;
  }

  // Returns the Atom with |name|.
  Atom GetAtom(const char* name);

  // Returns true if the event's message has |type|.
  bool MessageHasType(const XClientMessageEvent& event,
                      const char* type);

  // Sets |collector| to collect XClientMessageEvents which would otherwise
  // have been sent to the drop target window.
  void SetEventCollectorFor(::Window xid,
                            ClientMessageEventCollector* collector);

  // Builds an XdndStatus message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnStatus(XID target_window,
                bool will_accept_drop,
                ::Atom accepted_action);

  // Builds an XdndFinished message and sends it to
  // DesktopDragDropClientAuraX11.
  void OnFinished(XID target_window,
                  bool accepted_drop,
                  ::Atom performed_action);

  // Sets |xid| as the topmost window at the current mouse position and
  // generates a synthetic mouse move.
  void SetTopmostXWindowAndMoveMouse(::Window xid);

  // Returns true if the move loop is running.
  bool IsMoveLoopRunning();

  // DesktopDragDropClientAuraX11:
  virtual int StartDragAndDrop(
      const ui::OSExchangeData& data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& root_location,
      int operation,
      ui::DragDropTypes::DragEventSource source) OVERRIDE;
  virtual void OnMoveLoopEnded() OVERRIDE;

 private:
  // DesktopDragDropClientAuraX11:
  virtual ::Window FindWindowFor(const gfx::Point& screen_point) OVERRIDE;
  virtual void SendXClientEvent(::Window xid, XEvent* event) OVERRIDE;

  // The XID of the window which initiated the drag.
  ::Window source_xid_;

  // The XID of the window which is simulated to be the topmost window at the
  // current mouse position.
  ::Window target_xid_;

  // Whether the move loop is running.
  bool move_loop_running_;

  // Map of ::Windows to the collector which intercepts XClientMessageEvents
  // for that window.
  std::map< ::Window, ClientMessageEventCollector*> collectors_;

  ui::X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(TestDragDropClient);
};

///////////////////////////////////////////////////////////////////////////////
// ClientMessageEventCollector

ClientMessageEventCollector::ClientMessageEventCollector(
    ::Window xid,
    TestDragDropClient* client)
    : xid_(xid),
      client_(client) {
  client->SetEventCollectorFor(xid, this);
}

ClientMessageEventCollector::~ClientMessageEventCollector() {
  client_->SetEventCollectorFor(xid_, NULL);
}

std::vector<XClientMessageEvent> ClientMessageEventCollector::PopAllEvents() {
  std::vector<XClientMessageEvent> to_return;
  to_return.swap(events_);
  return to_return;
}

void ClientMessageEventCollector::RecordEvent(
    const XClientMessageEvent& event) {
  events_.push_back(event);
}

///////////////////////////////////////////////////////////////////////////////
// TestDragDropClient

// static
const int TestDragDropClient::kMouseMoveX = 100;

// static
const int TestDragDropClient::kMouseMoveY = 200;

TestDragDropClient::TestDragDropClient(
    aura::Window* window,
    DesktopNativeCursorManager* cursor_manager)
    : DesktopDragDropClientAuraX11(window,
                                   cursor_manager,
                                   gfx::GetXDisplay(),
                                   window->GetHost()->GetAcceleratedWidget()),
      source_xid_(window->GetHost()->GetAcceleratedWidget()),
      target_xid_(None),
      move_loop_running_(false),
      atom_cache_(gfx::GetXDisplay(), kAtomsToCache) {
}

TestDragDropClient::~TestDragDropClient() {
}

Atom TestDragDropClient::GetAtom(const char* name) {
  return atom_cache_.GetAtom(name);
}

bool TestDragDropClient::MessageHasType(const XClientMessageEvent& event,
                                        const char* type) {
  return event.message_type == atom_cache_.GetAtom(type);
}

void TestDragDropClient::SetEventCollectorFor(
    ::Window xid,
    ClientMessageEventCollector* collector) {
  if (collector)
    collectors_[xid] = collector;
  else
    collectors_.erase(xid);
}

void TestDragDropClient::OnStatus(XID target_window,
                                  bool will_accept_drop,
                                  ::Atom accepted_action) {
  XClientMessageEvent event;
  event.message_type = atom_cache_.GetAtom("XdndStatus");
  event.format = 32;
  event.window = source_xid_;
  event.data.l[0] = target_window;
  event.data.l[1] = will_accept_drop ? 1 : 0;
  event.data.l[2] = 0;
  event.data.l[3] = 0;
  event.data.l[4] = accepted_action;
  OnXdndStatus(event);
}

void TestDragDropClient::OnFinished(XID target_window,
                                    bool accepted_drop,
                                    ::Atom performed_action) {
  XClientMessageEvent event;
  event.message_type = atom_cache_.GetAtom("XdndFinished");
  event.format = 32;
  event.window = source_xid_;
  event.data.l[0] = target_window;
  event.data.l[1] = accepted_drop ? 1 : 0;
  event.data.l[2] = performed_action;
  event.data.l[3] = 0;
  event.data.l[4] = 0;
  OnXdndFinished(event);
}

void TestDragDropClient::SetTopmostXWindowAndMoveMouse(::Window xid) {
  target_xid_ = xid;

  XMotionEvent event;
  event.time = CurrentTime;
  event.x_root = kMouseMoveX;
  event.y_root = kMouseMoveY;
  OnMouseMovement(&event);
}

bool TestDragDropClient::IsMoveLoopRunning() {
  return move_loop_running_;
}

int TestDragDropClient::StartDragAndDrop(
    const ui::OSExchangeData& data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& root_location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  move_loop_running_ = true;
  return DesktopDragDropClientAuraX11::StartDragAndDrop(data, root_window,
      source_window, root_location, operation, source);
}

void TestDragDropClient::OnMoveLoopEnded() {
  DesktopDragDropClientAuraX11::OnMoveLoopEnded();
  move_loop_running_ = false;
}

::Window TestDragDropClient::FindWindowFor(const gfx::Point& screen_point) {
  return target_xid_;
}

void TestDragDropClient::SendXClientEvent(::Window xid, XEvent* event) {
  std::map< ::Window, ClientMessageEventCollector*>::iterator it =
      collectors_.find(xid);
  if (it != collectors_.end())
    it->second->RecordEvent(event->xclient);
}

}  // namespace

class DesktopDragDropClientAuraX11Test : public ViewsTestBase {
 public:
  DesktopDragDropClientAuraX11Test() {
  }

  virtual ~DesktopDragDropClientAuraX11Test() {
  }

  int StartDragAndDrop() {
    ui::OSExchangeData data;
    data.SetString(base::ASCIIToUTF16("Test"));

    return client_->StartDragAndDrop(
        data,
        widget_->GetNativeWindow()->GetRootWindow(),
        widget_->GetNativeWindow(),
        gfx::Point(),
        ui::DragDropTypes::DRAG_COPY,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
  }

  // ViewsTestBase:
  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();

    // Create widget to initiate the drags.
    widget_.reset(new Widget);
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new DesktopNativeWidgetAura(widget_.get());
    params.bounds = gfx::Rect(100, 100);
    widget_->Init(params);
    widget_->Show();

    cursor_manager_.reset(new DesktopNativeCursorManager(
        DesktopCursorLoaderUpdater::Create()));

    client_.reset(new TestDragDropClient(widget_->GetNativeWindow(),
                                         cursor_manager_.get()));
  }

  virtual void TearDown() OVERRIDE {
    client_.reset();
    cursor_manager_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  TestDragDropClient* client() {
    return client_.get();
  }

 private:
  scoped_ptr<TestDragDropClient> client_;
  scoped_ptr<DesktopNativeCursorManager> cursor_manager_;

  // The widget used to initiate drags.
  scoped_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11Test);
};

namespace {

void BasicStep2(TestDragDropClient* client, XID toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  // XdndEnter should have been sent to |toplevel| before the XdndPosition
  // message.
  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());

  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));
  EXPECT_EQ(1, events[0].data.l[1] & 1);
  std::vector<Atom> targets;
  ui::GetAtomArrayProperty(client->source_xwindow(), "XdndTypeList", &targets);
  EXPECT_FALSE(targets.empty());

  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));
  const long kCoords =
      TestDragDropClient::kMouseMoveX << 16 | TestDragDropClient::kMouseMoveY;
  EXPECT_EQ(kCoords, events[1].data.l[2]);
  EXPECT_EQ(client->GetAtom("XdndActionCopy"),
            static_cast<Atom>(events[1].data.l[4]));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));

  // Because there is no unprocessed XdndPosition, the drag drop client should
  // send XdndDrop immediately after the mouse is released.
  client->OnMouseReleased();

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));
  EXPECT_EQ(client->source_xwindow(),
            static_cast<XID>(events[0].data.l[0]));

  // Send XdndFinished to indicate that the drag drop client can cleanup any
  // data related to this drag. The move loop should end only after the
  // XdndFinished message was received.
  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void BasicStep3(TestDragDropClient* client, XID toplevel) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  // We have not received an XdndStatus ack for the second XdndPosition message.
  // Test that sending XdndDrop is delayed till the XdndStatus ack is received.
  client->OnMouseReleased();
  EXPECT_FALSE(collector.HasEvents());

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

TEST_F(DesktopDragDropClientAuraX11Test, Basic) {
  XID toplevel = 1;

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(&BasicStep2,
                                         client(),
                                         toplevel));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);

  // Do another drag and drop to test that the data is properly cleaned up as a
  // result of the XdndFinished message.
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(&BasicStep3,
                                         client(),
                                         toplevel));
  result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void TargetDoesNotRespondStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndLeave"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that we do not wait for the target to send XdndStatus if we have not
// received any XdndStatus messages at all from the target. The Unity
// DNDCollectionWindow is an example of an XdndAware target which does not
// respond to XdndPosition messages at all.
TEST_F(DesktopDragDropClientAuraX11Test, TargetDoesNotRespond) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TargetDoesNotRespondStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

namespace {

void QueuePositionStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->OnMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that XdndPosition messages are queued till the pending XdndPosition
// message is acked via an XdndStatus message.
TEST_F(DesktopDragDropClientAuraX11Test, QueuePosition) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&QueuePositionStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void TargetChangesStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel1 = 1;
  ClientMessageEventCollector collector1(toplevel1, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel1);

  std::vector<XClientMessageEvent> events1 = collector1.PopAllEvents();
  ASSERT_EQ(2u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events1[1], "XdndPosition"));

  XID toplevel2 = 2;
  ClientMessageEventCollector collector2(toplevel2, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel2);

  // It is possible for |toplevel1| to send XdndStatus after the source has sent
  // XdndLeave but before |toplevel1| has received the XdndLeave message. The
  // XdndStatus message should be ignored.
  client->OnStatus(toplevel1, true, client->GetAtom("XdndActionCopy"));
  events1 = collector1.PopAllEvents();
  ASSERT_EQ(1u, events1.size());
  EXPECT_TRUE(client->MessageHasType(events1[0], "XdndLeave"));

  std::vector<XClientMessageEvent> events2 = collector2.PopAllEvents();
  ASSERT_EQ(2u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events2[1], "XdndPosition"));

  client->OnStatus(toplevel2, true, client->GetAtom("XdndActionCopy"));
  client->OnMouseReleased();
  events2 = collector2.PopAllEvents();
  ASSERT_EQ(1u, events2.size());
  EXPECT_TRUE(client->MessageHasType(events2[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel2, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test the behavior when the target changes during a drag.
TEST_F(DesktopDragDropClientAuraX11Test, TargetChanges) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&TargetChangesStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, result);
}

namespace {

void RejectAfterMouseReleaseStep2(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 1;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  // Send another mouse move such that there is a pending XdndPosition.
  client->SetTopmostXWindowAndMoveMouse(toplevel);
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndPosition"));

  client->OnMouseReleased();
  // Reject the drop.
  client->OnStatus(toplevel, false, None);

  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndLeave"));
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

void RejectAfterMouseReleaseStep3(TestDragDropClient* client) {
  EXPECT_TRUE(client->IsMoveLoopRunning());

  XID toplevel = 2;
  ClientMessageEventCollector collector(toplevel, client);
  client->SetTopmostXWindowAndMoveMouse(toplevel);

  std::vector<XClientMessageEvent> events = collector.PopAllEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndEnter"));
  EXPECT_TRUE(client->MessageHasType(events[1], "XdndPosition"));

  client->OnStatus(toplevel, true, client->GetAtom("XdndActionCopy"));
  EXPECT_FALSE(collector.HasEvents());

  client->OnMouseReleased();
  events = collector.PopAllEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(client->MessageHasType(events[0], "XdndDrop"));

  EXPECT_TRUE(client->IsMoveLoopRunning());
  client->OnFinished(toplevel, false, None);
  EXPECT_FALSE(client->IsMoveLoopRunning());
}

}  // namespace

// Test that the source sends XdndLeave instead of XdndDrop if the drag
// operation is rejected after the mouse is released.
TEST_F(DesktopDragDropClientAuraX11Test, RejectAfterMouseRelease) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RejectAfterMouseReleaseStep2, client()));
  int result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);

  // Repeat the test but reject the drop in the XdndFinished message instead.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RejectAfterMouseReleaseStep3, client()));
  result = StartDragAndDrop();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE, result);
}

}  // namespace views
