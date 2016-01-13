// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/context_factory_mojo.h"

#include "base/bind.h"
#include "cc/output/output_surface.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/reflector.h"

namespace mojo {
namespace {

void FreeSharedBitmap(cc::SharedBitmap* shared_bitmap) {
  delete shared_bitmap->memory();
}

void IgnoreSharedBitmap(cc::SharedBitmap* shared_bitmap) {}

class SoftwareOutputDeviceViewManager : public cc::SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceViewManager(ui::Compositor* compositor)
      : compositor_(compositor) {
  }
  virtual ~SoftwareOutputDeviceViewManager() {}

  // cc::SoftwareOutputDevice:
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE {
    WindowTreeHostMojo* window_tree_host =
        WindowTreeHostMojo::ForCompositor(compositor_);
    DCHECK(window_tree_host);
    window_tree_host->SetContents(
        skia::GetTopDevice(*canvas_)->accessBitmap(true));

    SoftwareOutputDevice::EndPaint(frame_data);
  }

 private:
  ui::Compositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceViewManager);
};

// TODO(sky): this is a copy from cc/test. Copy to a common place.
class TestSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  TestSharedBitmapManager() {}
  virtual ~TestSharedBitmapManager() {}

  virtual scoped_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) OVERRIDE {
    base::AutoLock lock(lock_);
    scoped_ptr<base::SharedMemory> memory(new base::SharedMemory);
    memory->CreateAndMapAnonymous(size.GetArea() * 4);
    cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
    bitmap_map_[id] = memory.get();
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(memory.release(), id,
                             base::Bind(&FreeSharedBitmap)));
  }

  virtual scoped_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const cc::SharedBitmapId& id) OVERRIDE {
    base::AutoLock lock(lock_);
    if (bitmap_map_.find(id) == bitmap_map_.end())
      return scoped_ptr<cc::SharedBitmap>();
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(bitmap_map_[id], id,
                             base::Bind(&IgnoreSharedBitmap)));
  }

  virtual scoped_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* memory) OVERRIDE {
    base::AutoLock lock(lock_);
    cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
    bitmap_map_[id] = memory;
    return scoped_ptr<cc::SharedBitmap>(
        new cc::SharedBitmap(memory, id, base::Bind(&IgnoreSharedBitmap)));
  }

 private:
  base::Lock lock_;
  std::map<cc::SharedBitmapId, base::SharedMemory*> bitmap_map_;

  DISALLOW_COPY_AND_ASSIGN(TestSharedBitmapManager);
};

}  // namespace

ContextFactoryMojo::ContextFactoryMojo()
    : shared_bitmap_manager_(new TestSharedBitmapManager()) {
}

ContextFactoryMojo::~ContextFactoryMojo() {}

scoped_ptr<cc::OutputSurface> ContextFactoryMojo::CreateOutputSurface(
    ui::Compositor* compositor,
    bool software_fallback) {
  scoped_ptr<cc::SoftwareOutputDevice> output_device(
      new SoftwareOutputDeviceViewManager(compositor));
  return make_scoped_ptr(new cc::OutputSurface(output_device.Pass()));
}

scoped_refptr<ui::Reflector> ContextFactoryMojo::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  return new ui::Reflector();
}

void ContextFactoryMojo::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
ContextFactoryMojo::SharedMainThreadContextProvider() {
  return scoped_refptr<cc::ContextProvider>(NULL);
}

void ContextFactoryMojo::RemoveCompositor(ui::Compositor* compositor) {}

bool ContextFactoryMojo::DoesCreateTestContexts() { return false; }

cc::SharedBitmapManager* ContextFactoryMojo::GetSharedBitmapManager() {
  return shared_bitmap_manager_.get();
}

base::MessageLoopProxy* ContextFactoryMojo::GetCompositorMessageLoop() {
  return NULL;
}

}  // namespace mojo
