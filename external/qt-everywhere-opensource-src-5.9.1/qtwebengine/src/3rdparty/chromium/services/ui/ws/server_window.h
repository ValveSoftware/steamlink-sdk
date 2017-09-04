// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_H_
#define SERVICES_UI_WS_SERVER_WINDOW_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/surfaces/surfaces_context_provider.h"
#include "services/ui/ws/ids.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"
#include "ui/platform_window/text_input_state.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace ui {
namespace ws {

class ServerWindowDelegate;
class ServerWindowObserver;
class ServerWindowCompositorFrameSinkManager;

// Server side representation of a window. Delegate is informed of interesting
// events.
//
// It is assumed that all functions that mutate the tree have validated the
// mutation is possible before hand. For example, Reorder() assumes the supplied
// window is a child and not already in position.
//
// ServerWindows do not own their children. If you delete a window that has
// children the children are implicitly removed. Similarly if a window has a
// parent and the window is deleted the deleted window is implicitly removed
// from the parent.
class ServerWindow {
 public:
  using Properties = std::map<std::string, std::vector<uint8_t>>;
  using Windows = std::vector<ServerWindow*>;

  ServerWindow(ServerWindowDelegate* delegate, const WindowId& id);
  ServerWindow(ServerWindowDelegate* delegate,
               const WindowId& id,
               const Properties& properties);
  ~ServerWindow();

  void AddObserver(ServerWindowObserver* observer);
  void RemoveObserver(ServerWindowObserver* observer);
  bool HasObserver(ServerWindowObserver* observer);

  // Creates a new CompositorFrameSink of the specified type, replacing the
  // existing.
  // TODO(fsamuel): We should not be passing in |gpu_memory_buffer_manager| and
  // |context_provider|. The window server should not know anything about them.
  // Instead, they should be a CompositorFrameSink service-side implementation
  // detail.
  void CreateCompositorFrameSink(
      mojom::CompositorFrameSinkType compositor_frame_sink_type,
      gfx::AcceleratedWidget widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      scoped_refptr<SurfacesContextProvider> context_provider,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  const WindowId& id() const { return id_; }

  void Add(ServerWindow* child);
  void Remove(ServerWindow* child);
  void Reorder(ServerWindow* relative, mojom::OrderDirection diretion);
  void StackChildAtBottom(ServerWindow* child);
  void StackChildAtTop(ServerWindow* child);

  const gfx::Rect& bounds() const { return bounds_; }
  // Sets the bounds. If the size changes this implicitly resets the client
  // area to fill the whole bounds.
  void SetBounds(const gfx::Rect& bounds);

  const std::vector<gfx::Rect>& additional_client_areas() const {
    return additional_client_areas_;
  }
  const gfx::Insets& client_area() const { return client_area_; }
  void SetClientArea(const gfx::Insets& insets,
                     const std::vector<gfx::Rect>& additional_client_areas);

  const gfx::Rect* hit_test_mask() const { return hit_test_mask_.get(); }
  void SetHitTestMask(const gfx::Rect& mask);
  void ClearHitTestMask();

  bool can_accept_drops() const { return accepts_drops_; }
  void SetCanAcceptDrops(bool accepts_drags);

  ui::mojom::Cursor cursor() const { return cursor_id_; }
  ui::mojom::Cursor non_client_cursor() const {
    return non_client_cursor_id_;
  }

  const ServerWindow* parent() const { return parent_; }
  ServerWindow* parent() { return parent_; }

  const ServerWindow* GetRoot() const;
  ServerWindow* GetRoot() {
    return const_cast<ServerWindow*>(
        const_cast<const ServerWindow*>(this)->GetRoot());
  }

  const Windows& children() const { return children_; }

  // Returns the ServerWindow object with the provided |id| if it lies in a
  // subtree of |this|.
  ServerWindow* GetChildWindow(const WindowId& id);

  // Transient window management.
  // Adding transient child fails if the child window is modal to system.
  bool AddTransientWindow(ServerWindow* child);
  void RemoveTransientWindow(ServerWindow* child);

  ServerWindow* transient_parent() { return transient_parent_; }
  const ServerWindow* transient_parent() const { return transient_parent_; }

  const Windows& transient_children() const { return transient_children_; }

  bool is_modal() const { return is_modal_; }
  void SetModal();

  // Returns true if this contains |window| or is |window|.
  bool Contains(const ServerWindow* window) const;

  // Returns the visibility requested by this window. IsDrawn() returns whether
  // the window is actually visible on screen.
  bool visible() const { return visible_; }
  void SetVisible(bool value);

  float opacity() const { return opacity_; }
  void SetOpacity(float value);

  void SetPredefinedCursor(ui::mojom::Cursor cursor_id);
  void SetNonClientCursor(ui::mojom::Cursor cursor_id);

  const gfx::Transform& transform() const { return transform_; }
  void SetTransform(const gfx::Transform& transform);

  const std::map<std::string, std::vector<uint8_t>>& properties() const {
    return properties_;
  }
  void SetProperty(const std::string& name, const std::vector<uint8_t>* value);

  std::string GetName() const;

  void SetTextInputState(const ui::TextInputState& state);
  const ui::TextInputState& text_input_state() const {
    return text_input_state_;
  }

  void set_can_focus(bool can_focus) { can_focus_ = can_focus; }
  bool can_focus() const { return can_focus_; }

  void set_can_accept_events(bool value) { can_accept_events_ = value; }
  bool can_accept_events() const { return can_accept_events_; }

  // Returns true if this window is attached to a root and all ancestors are
  // visible.
  bool IsDrawn() const;

  const gfx::Insets& extended_hit_test_region() const {
    return extended_hit_test_region_;
  }
  void set_extended_hit_test_region(const gfx::Insets& insets) {
    extended_hit_test_region_ = insets;
  }

  ServerWindowCompositorFrameSinkManager*
  GetOrCreateCompositorFrameSinkManager();
  ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager() {
    return compositor_frame_sink_manager_.get();
  }
  const ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager()
      const {
    return compositor_frame_sink_manager_.get();
  }

  // Offset of the underlay from the the window bounds (used for shadows).
  const gfx::Vector2d& underlay_offset() const { return underlay_offset_; }
  void SetUnderlayOffset(const gfx::Vector2d& offset);

  ServerWindowDelegate* delegate() { return delegate_; }

  // Called when the window is no longer an embed root.
  void OnEmbeddedAppDisconnected();

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  std::string GetDebugWindowHierarchy() const;
  std::string GetDebugWindowInfo() const;
  void BuildDebugInfo(const std::string& depth, std::string* result) const;
#endif

 private:
  // Implementation of removing a window. Doesn't send any notification.
  void RemoveImpl(ServerWindow* window);

  // Called when this window's stacking order among its siblings is changed.
  void OnStackingChanged();

  static void ReorderImpl(ServerWindow* window,
                          ServerWindow* relative,
                          mojom::OrderDirection diretion);

  // Returns a pointer to the stacking target that can be used by
  // RestackTransientDescendants.
  static ServerWindow** GetStackingTarget(ServerWindow* window);

  ServerWindowDelegate* delegate_;
  const WindowId id_;
  ServerWindow* parent_;
  Windows children_;

  // Transient window management.
  // If non-null we're actively restacking transient as the result of a
  // transient ancestor changing.
  ServerWindow* stacking_target_;
  ServerWindow* transient_parent_;
  Windows transient_children_;

  bool is_modal_;
  bool visible_;
  gfx::Rect bounds_;
  gfx::Insets client_area_;
  std::vector<gfx::Rect> additional_client_areas_;
  std::unique_ptr<ServerWindowCompositorFrameSinkManager>
      compositor_frame_sink_manager_;
  mojom::Cursor cursor_id_;
  mojom::Cursor non_client_cursor_id_;
  float opacity_;
  bool can_focus_;
  bool can_accept_events_;
  gfx::Transform transform_;
  ui::TextInputState text_input_state_;

  Properties properties_;

  gfx::Vector2d underlay_offset_;

  // The hit test for windows extends outside the bounds of the window by this
  // amount.
  gfx::Insets extended_hit_test_region_;

  // Mouse events outside the hit test mask don't hit the window. An empty mask
  // means all events miss the window. If null there is no mask.
  std::unique_ptr<gfx::Rect> hit_test_mask_;

  // Whether this window can be the target in a drag and drop
  // operation. Clients must opt-in to this.
  bool accepts_drops_ = false;

  base::ObserverList<ServerWindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindow);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_H_
