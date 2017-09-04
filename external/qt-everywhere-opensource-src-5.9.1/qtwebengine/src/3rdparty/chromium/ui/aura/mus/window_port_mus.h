// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_PORT_MUS_H_
#define UI_AURA_MUS_WINDOW_PORT_MUS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/window_compositor_frame_sink.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/window_port.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"

namespace aura {

class PropertyConverter;
class SurfaceIdHandler;
class Window;
class WindowPortMusTestApi;
class WindowTreeClient;
class WindowTreeClientPrivate;
class WindowTreeHostMus;

// WindowPortMus is a WindowPort that forwards calls to WindowTreeClient
// so that changes are propagated to the server. All changes from
// WindowTreeClient to the underlying Window route through this class (by
// way of WindowMus) and are done in such a way that they don't result in
// calling back to WindowTreeClient.
class AURA_EXPORT WindowPortMus : public WindowPort, public WindowMus {
 public:
  // See WindowMus's constructor for details on |window_mus_type|.
  WindowPortMus(WindowTreeClient* client, WindowMusType window_mus_type);
  ~WindowPortMus() override;

  static WindowPortMus* Get(Window* window);

  Window* window() { return window_; }
  const Window* window() const { return window_; }

  void SetTextInputState(mojo::TextInputStatePtr state);
  void SetImeVisibility(bool visible, mojo::TextInputStatePtr state);

  ui::mojom::Cursor predefined_cursor() const { return predefined_cursor_; }
  void SetPredefinedCursor(ui::mojom::Cursor cursor_id);

  std::unique_ptr<WindowCompositorFrameSink> RequestCompositorFrameSink(
      ui::mojom::CompositorFrameSinkType type,
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);

  void AttachCompositorFrameSink(
      ui::mojom::CompositorFrameSinkType type,
      std::unique_ptr<WindowCompositorFrameSinkBinding>
          compositor_frame_sink_binding);

  void set_surface_id_handler(SurfaceIdHandler* surface_id_handler) {
    surface_id_handler_ = surface_id_handler;
  }

 private:
  friend class WindowPortMusTestApi;
  friend class WindowTreeClient;
  friend class WindowTreeClientPrivate;
  friend class WindowTreeHostMus;

  using ServerChangeIdType = uint8_t;

  // Changes to the underlying Window originating from the server must be done
  // in such a way that the same change is not applied back to the server. To
  // accomplish this every changes from the server is associated with at least
  // one ServerChange. If the underlying Window ends up calling back to this
  // class and the change is expected then the change is ignored and not sent to
  // the server. For example, here's the flow when the server changes the
  // bounds:
  // . WindowTreeClient calls SetBoundsFromServer().
  // . A ServerChange is added of type BOUNDS and the matching bounds.
  // . Window::SetBounds() is called.
  // . Window::SetBounds() calls WindowPortMus::OnDidChangeBounds().
  // . A ServerChange of type BOUNDS is found, and the request is ignored.
  //   Additionally the ServerChange is removed at this point so that if another
  //   bounds change is made it will be propagated. This is important as changes
  //   to underyling window may generate more changes.
  //
  // The typical pattern in implementing a call from the server looks like:
  //   // Create and configure the data as appropriate to the change:
  //   ServerChangeData data;
  //   data.foo = window->bar();
  //   ScopedServerChange change(this, ServerChangeType::FOO, data);
  //   window_->SetFoo(...);
  //
  // And the call from the Window (by way of WindowPort interface) looks like:
  //   ServerChangeData change_data;
  //   change_data.foo = ...;
  //   if (!RemoveChangeByTypeAndData(ServerChangeType::FOO, change_data))
  //     window_tree_client_->OnFooChanged(this, ...);
  enum ServerChangeType {
    ADD,
    ADD_TRANSIENT,
    BOUNDS,
    PROPERTY,
    REMOVE,
    REMOVE_TRANSIENT,
    REORDER,
    VISIBLE,
  };

  // Contains data needed to identify a change from the server.
  struct ServerChangeData {
    // Applies to ADD, ADD_TRANSIENT, REMOVE, REMOVE_TRANSIENT and REORDER.
    Id child_id;
    // Applies to BOUNDS.
    gfx::Rect bounds;
    // Applies to VISIBLE.
    bool visible;
    // Applies to PROPERTY.
    std::string property_name;
  };

  // Used to identify a change the server.
  struct ServerChange {
    ServerChangeType type;
    // A unique id assigned to the change and used later on to identify it for
    // removal.
    ServerChangeIdType server_change_id;
    ServerChangeData data;
  };

  // Convenience for adding/removing a ScopedChange.
  class ScopedServerChange {
   public:
    ScopedServerChange(WindowPortMus* window_impl,
                       const ServerChangeType type,
                       const ServerChangeData& data)
        : window_impl_(window_impl),
          server_change_id_(window_impl->ScheduleChange(type, data)) {}

    ~ScopedServerChange() { window_impl_->RemoveChangeById(server_change_id_); }

   private:
    WindowPortMus* window_impl_;
    const ServerChangeIdType server_change_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedServerChange);
  };

  struct WindowMusChangeDataImpl : public WindowMusChangeData {
    WindowMusChangeDataImpl();
    ~WindowMusChangeDataImpl() override;

    std::unique_ptr<ScopedServerChange> change;
  };

  // Creates and adds a ServerChange to |server_changes_|. Returns the id
  // assigned to the ServerChange.
  ServerChangeIdType ScheduleChange(const ServerChangeType type,
                                    const ServerChangeData& data);

  // Removes a ServerChange by id.
  void RemoveChangeById(ServerChangeIdType change_id);

  // If there is a schedule change matching |type| and |data| it is removed and
  // true is returned. If no matching change is scheduled returns false.
  bool RemoveChangeByTypeAndData(const ServerChangeType type,
                                 const ServerChangeData& data);

  PropertyConverter* GetPropertyConverter();

  // WindowMus:
  Window* GetWindow() override;
  void AddChildFromServer(WindowMus* window) override;
  void RemoveChildFromServer(WindowMus* child) override;
  void ReorderFromServer(WindowMus* child,
                         WindowMus* relative,
                         ui::mojom::OrderDirection) override;
  void SetBoundsFromServer(const gfx::Rect& bounds) override;
  void SetVisibleFromServer(bool visible) override;
  void SetOpacityFromServer(float opacity) override;
  void SetPredefinedCursorFromServer(ui::mojom::Cursor cursor) override;
  void SetPropertyFromServer(
      const std::string& property_name,
      const std::vector<uint8_t>* property_data) override;
  void SetSurfaceIdFromServer(
      std::unique_ptr<SurfaceInfo> surface_info) override;
  void AddTransientChildFromServer(WindowMus* child) override;
  void RemoveTransientChildFromServer(WindowMus* child) override;
  ChangeSource OnTransientChildAdded(WindowMus* child) override;
  ChangeSource OnTransientChildRemoved(WindowMus* child) override;
  std::unique_ptr<WindowMusChangeData> PrepareForServerBoundsChange(
      const gfx::Rect& bounds) override;
  std::unique_ptr<WindowMusChangeData> PrepareForServerVisibilityChange(
      bool value) override;
  void NotifyEmbeddedAppDisconnected() override;

  // WindowPort:
  void OnPreInit(Window* window) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWillAddChild(Window* child) override;
  void OnWillRemoveChild(Window* child) override;
  void OnWillMoveChild(size_t current_index, size_t dest_index) override;
  void OnVisibilityChanged(bool visible) override;
  void OnDidChangeBounds(const gfx::Rect& old_bounds,
                         const gfx::Rect& new_bounds) override;
  std::unique_ptr<WindowPortPropertyData> OnWillChangeProperty(
      const void* key) override;
  void OnPropertyChanged(const void* key,
                         std::unique_ptr<WindowPortPropertyData> data) override;

  WindowTreeClient* window_tree_client_;

  Window* window_ = nullptr;

  ServerChangeIdType next_server_change_id_ = 0;
  std::vector<ServerChange> server_changes_;

  SurfaceIdHandler* surface_id_handler_;
  std::unique_ptr<SurfaceInfo> surface_info_;

  ui::mojom::Cursor predefined_cursor_ = ui::mojom::Cursor::CURSOR_NULL;

  DISALLOW_COPY_AND_ASSIGN(WindowPortMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_PORT_MUS_H_
