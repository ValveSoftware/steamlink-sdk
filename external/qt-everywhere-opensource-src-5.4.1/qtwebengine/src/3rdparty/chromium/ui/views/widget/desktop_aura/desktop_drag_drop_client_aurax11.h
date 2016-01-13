// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_

#include <set>
#include <X11/Xlib.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/point.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop.h"
#include "ui/views/widget/desktop_aura/x11_whole_screen_move_loop_delegate.h"
#include "ui/wm/public/drag_drop_client.h"

namespace aura {
namespace client {
class DragDropDelegate;
}
}

namespace gfx {
class Point;
}

namespace ui {
class DragSource;
class DropTargetEvent;
class OSExchangeData;
class OSExchangeDataProviderAuraX11;
class SelectionFormatMap;
}

namespace views {
class DesktopNativeCursorManager;

// Implements drag and drop on X11 for aura. On one side, this class takes raw
// X11 events forwarded from DesktopWindowTreeHostLinux, while on the other, it
// handles the views drag events.
class VIEWS_EXPORT DesktopDragDropClientAuraX11
    : public aura::client::DragDropClient,
      public aura::WindowObserver,
      public X11WholeScreenMoveLoopDelegate {
 public:
  DesktopDragDropClientAuraX11(
      aura::Window* root_window,
      views::DesktopNativeCursorManager* cursor_manager,
      Display* xdisplay,
      ::Window xwindow);
  virtual ~DesktopDragDropClientAuraX11();

  // We maintain a mapping of live DesktopDragDropClientAuraX11 objects to
  // their ::Windows. We do this so that we're able to short circuit sending
  // X11 messages to windows in our process.
  static DesktopDragDropClientAuraX11* GetForWindow(::Window window);

  // These methods handle the various X11 client messages from the platform.
  void OnXdndEnter(const XClientMessageEvent& event);
  void OnXdndLeave(const XClientMessageEvent& event);
  void OnXdndPosition(const XClientMessageEvent& event);
  void OnXdndStatus(const XClientMessageEvent& event);
  void OnXdndFinished(const XClientMessageEvent& event);
  void OnXdndDrop(const XClientMessageEvent& event);

  // Called when XSelection data has been copied to our process.
  void OnSelectionNotify(const XSelectionEvent& xselection);

  // Overridden from aura::client::DragDropClient:
  virtual int StartDragAndDrop(
      const ui::OSExchangeData& data,
      aura::Window* root_window,
      aura::Window* source_window,
      const gfx::Point& root_location,
      int operation,
      ui::DragDropTypes::DragEventSource source) OVERRIDE;
  virtual void DragUpdate(aura::Window* target,
                          const ui::LocatedEvent& event) OVERRIDE;
  virtual void Drop(aura::Window* target,
                    const ui::LocatedEvent& event) OVERRIDE;
  virtual void DragCancel() OVERRIDE;
  virtual bool IsDragDropInProgress() OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Overridden from X11WholeScreenMoveLoopDelegate:
  virtual void OnMouseMovement(XMotionEvent* event) OVERRIDE;
  virtual void OnMouseReleased() OVERRIDE;
  virtual void OnMoveLoopEnded() OVERRIDE;

 protected:
  // The following methods are virtual for the sake of testing.

  // Finds the topmost X11 window at |screen_point| and returns it if it is
  // Xdnd aware. Returns NULL otherwise.
  virtual ::Window FindWindowFor(const gfx::Point& screen_point);

  // Sends |xev| to |xid|, optionally short circuiting the round trip to the X
  // server.
  virtual void SendXClientEvent(::Window xid, XEvent* xev);

 private:
  enum SourceState {
    // |source_current_window_| will receive a drop once we receive an
    // XdndStatus from it.
    SOURCE_STATE_PENDING_DROP,

    // The move looped will be ended once we receive XdndFinished from
    // |source_current_window_|. We should not send XdndPosition to
    // |source_current_window_| while in this state.
    SOURCE_STATE_DROPPED,

    // There is no drag in progress or there is a drag in progress and the
    // user has not yet released the mouse.
    SOURCE_STATE_OTHER,
  };

  // Processes a mouse move at |screen_point|.
  void ProcessMouseMove(const gfx::Point& screen_point,
                        unsigned long event_time);

  // Start timer to end the move loop if the target is too slow to respond after
  // the mouse is released.
  void StartEndMoveLoopTimer();

  // Ends the move loop.
  void EndMoveLoop();

  // When we receive an position x11 message, we need to translate that into
  // the underlying aura::Window representation, as moves internal to the X11
  // window can cause internal drag leave and enter messages.
  void DragTranslate(const gfx::Point& root_window_location,
                     scoped_ptr<ui::OSExchangeData>* data,
                     scoped_ptr<ui::DropTargetEvent>* event,
                     aura::client::DragDropDelegate** delegate);

  // Called when we need to notify the current aura::Window that we're no
  // longer dragging over it.
  void NotifyDragLeave();

  // Converts our bitfield of actions into an Atom that represents what action
  // we're most likely to take on drop.
  ::Atom DragOperationToAtom(int drag_operation);

  // Converts a single action atom to a drag operation.
  ui::DragDropTypes::DragOperation AtomToDragOperation(::Atom atom);

  // During the blocking StartDragAndDrop() call, this converts the views-style
  // |drag_operation_| bitfield into a vector of Atoms to offer to other
  // processes.
  std::vector< ::Atom> GetOfferedDragOperations();

  // This returns a representation of the data we're offering in this
  // drag. This is done to bypass an asynchronous roundtrip with the X11
  // server.
  ui::SelectionFormatMap GetFormatMap() const;

  // Handling XdndPosition can be paused while waiting for more data; this is
  // called either synchronously from OnXdndPosition, or asynchronously after
  // we've received data requested from the other window.
  void CompleteXdndPosition(::Window source_window,
                            const gfx::Point& screen_point);

  void SendXdndEnter(::Window dest_window);
  void SendXdndLeave(::Window dest_window);
  void SendXdndPosition(::Window dest_window,
                        const gfx::Point& screen_point,
                        unsigned long event_time);
  void SendXdndDrop(::Window dest_window);

  // A nested message loop that notifies this object of events through the
  // X11WholeScreenMoveLoopDelegate interface.
  X11WholeScreenMoveLoop move_loop_;

  aura::Window* root_window_;

  Display* xdisplay_;
  ::Window xwindow_;

  ui::X11AtomCache atom_cache_;

  // Target side information.
  class X11DragContext;
  scoped_ptr<X11DragContext> target_current_context_;

  // The Aura window that is currently under the cursor. We need to manually
  // keep track of this because Windows will only call our drag enter method
  // once when the user enters the associated X Window. But inside that X
  // Window there could be multiple aura windows, so we need to generate drag
  // enter events for them.
  aura::Window* target_window_;

  // Because Xdnd messages don't contain the position in messages other than
  // the XdndPosition message, we must manually keep track of the last position
  // change.
  gfx::Point target_window_location_;
  gfx::Point target_window_root_location_;

  // In the Xdnd protocol, we aren't supposed to send another XdndPosition
  // message until we have received a confirming XdndStatus message.
  bool waiting_on_status_;

  // If we would send an XdndPosition message while we're waiting for an
  // XdndStatus response, we need to cache the latest details we'd send.
  scoped_ptr<std::pair<gfx::Point, unsigned long> > next_position_message_;

  // Reprocesses the most recent mouse move event if the mouse has not moved
  // in a while in case the window stacking order has changed and
  // |source_current_window_| needs to be updated.
  base::OneShotTimer<DesktopDragDropClientAuraX11> repeat_mouse_move_timer_;

  // When the mouse is released, we need to wait for the last XdndStatus message
  // only if we have previously received a status message from
  // |source_current_window_|.
  bool status_received_since_enter_;

  // Source side information.
  ui::OSExchangeDataProviderAuraX11 const* source_provider_;
  ::Window source_current_window_;
  SourceState source_state_;

  // The current drag-drop client that has an active operation. Since we have
  // multiple root windows and multiple DesktopDragDropClientAuraX11 instances
  // it is important to maintain only one drag and drop operation at any time.
  static DesktopDragDropClientAuraX11* g_current_drag_drop_client;

  // The operation bitfield as requested by StartDragAndDrop.
  int drag_operation_;

  // We offer the other window a list of possible operations,
  // XdndActionsList. This is the requested action from the other window. This
  // is DRAG_NONE if we haven't sent out an XdndPosition message yet, haven't
  // yet received an XdndStatus or if the other window has told us that there's
  // no action that we can agree on.
  ui::DragDropTypes::DragOperation negotiated_operation_;

  // Ends the move loop if the target is too slow to respond after the mouse is
  // released.
  base::OneShotTimer<DesktopDragDropClientAuraX11> end_move_loop_timer_;

  // We use these cursors while dragging.
  gfx::NativeCursor grab_cursor_;
  gfx::NativeCursor copy_grab_cursor_;
  gfx::NativeCursor move_grab_cursor_;

  base::WeakPtrFactory<DesktopDragDropClientAuraX11> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopDragDropClientAuraX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DRAG_DROP_CLIENT_AURAX11_H_
