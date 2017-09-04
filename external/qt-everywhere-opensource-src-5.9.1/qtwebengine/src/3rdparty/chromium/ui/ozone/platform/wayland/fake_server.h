// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_
#define UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_

#include <wayland-server-core.h>

#include "base/bind.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_global;
struct wl_resource;

namespace wl {

// Base class for managing the life cycle of server objects.
class ServerObject {
 public:
  ServerObject(wl_resource* resource);
  virtual ~ServerObject();

  wl_resource* resource() { return resource_; }

  static void OnResourceDestroyed(wl_resource* resource);

 private:
  wl_resource* resource_;

  DISALLOW_COPY_AND_ASSIGN(ServerObject);
};

// Manage xdg_surface for providing desktop UI.
class MockXdgSurface : public ServerObject {
 public:
  MockXdgSurface(wl_resource* resource);
  ~MockXdgSurface() override;

  MOCK_METHOD1(SetParent, void(wl_resource* parent));
  MOCK_METHOD1(SetTitle, void(const char* title));
  MOCK_METHOD1(SetAppId, void(const char* app_id));
  MOCK_METHOD1(AckConfigure, void(uint32_t serial));
  MOCK_METHOD0(SetMaximized, void());
  MOCK_METHOD0(UnsetMaximized, void());
  MOCK_METHOD0(SetMinimized, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgSurface);
};

// Manage client surface
class MockSurface : public ServerObject {
 public:
  MockSurface(wl_resource* resource);
  ~MockSurface() override;

  static MockSurface* FromResource(wl_resource* resource);

  MOCK_METHOD3(Attach, void(wl_resource* buffer, int32_t x, int32_t y));
  MOCK_METHOD4(Damage,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD0(Commit, void());

  std::unique_ptr<MockXdgSurface> xdg_surface;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSurface);
};

class MockPointer : public ServerObject {
 public:
  MockPointer(wl_resource* resource);
  ~MockPointer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPointer);
};

struct GlobalDeleter {
  void operator()(wl_global* global);
};

// Base class for managing the life cycle of global objects:
// It presents a global object used to emit global events to all clients.
class Global {
 public:
  Global(const wl_interface* interface,
         const void* implementation,
         uint32_t version);
  virtual ~Global();

  // Create a global object.
  bool Initialize(wl_display* display);
  // Called from Bind() to send additional information to clients.
  virtual void OnBind() {}

  // The first bound resource to this global, which is usually all that is
  // useful when testing a simple client.
  wl_resource* resource() { return resource_; }

  // Send the global event to clients.
  static void Bind(wl_client* client,
                   void* data,
                   uint32_t version,
                   uint32_t id);
  static void OnResourceDestroyed(wl_resource* resource);

 private:
  std::unique_ptr<wl_global, GlobalDeleter> global_;

  const wl_interface* interface_;
  const void* implementation_;
  const uint32_t version_;
  wl_resource* resource_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Global);
};

// Manage wl_compositor object.
class MockCompositor : public Global {
 public:
  MockCompositor();
  ~MockCompositor() override;

  void AddSurface(std::unique_ptr<MockSurface> surface);

 private:
  std::vector<std::unique_ptr<MockSurface>> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockCompositor);
};

// Handle wl_output object.
class MockOutput : public Global {
 public:
  MockOutput();
  ~MockOutput() override;
  void SetRect(const gfx::Rect rect) { rect_ = rect; }
  const gfx::Rect GetRect() { return rect_; }
  void OnBind() override;

 private:
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(MockOutput);
};

// Manage wl_seat object: group of input devices.
class MockSeat : public Global {
 public:
  MockSeat();
  ~MockSeat() override;

  std::unique_ptr<MockPointer> pointer;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSeat);
};

// Manage xdg_shell object.
class MockXdgShell : public Global {
 public:
  MockXdgShell();
  ~MockXdgShell() override;

  MOCK_METHOD1(UseUnstableVersion, void(int32_t version));
  MOCK_METHOD1(Pong, void(uint32_t serial));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgShell);
};

struct DisplayDeleter {
  void operator()(wl_display* display);
};

class FakeServer : public base::Thread, base::MessagePumpLibevent::Watcher {
 public:
  FakeServer();
  ~FakeServer() override;

  // Start the fake Wayland server. If this succeeds, the WAYLAND_SOCKET
  // environment variable will be set to the string representation of a file
  // descriptor that a client can connect to. The caller is responsible for
  // ensuring that this file descriptor gets closed (for example, by calling
  // wl_display_connect).
  bool Start();

  // Pause the server when it becomes idle.
  void Pause();

  // Resume the server after flushing client connections.
  void Resume();

  template <typename T>
  T* GetObject(uint32_t id) {
    wl_resource* resource = wl_client_get_object(client_, id);
    return resource ? T::FromResource(resource) : nullptr;
  }

  MockSeat* seat() { return &seat_; }
  MockXdgShell* xdg_shell() { return &xdg_shell_; }
  MockOutput* output() { return &output_; }

 private:
  void DoPause();

  std::unique_ptr<base::MessagePump> CreateMessagePump();

  // base::MessagePumpLibevent::Watcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  std::unique_ptr<wl_display, DisplayDeleter> display_;
  wl_client* client_ = nullptr;
  wl_event_loop* event_loop_ = nullptr;

  base::WaitableEvent pause_event_;
  base::WaitableEvent resume_event_;

  // Represent Wayland global objects
  MockCompositor compositor_;
  MockOutput output_;
  MockSeat seat_;
  MockXdgShell xdg_shell_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeServer);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_
