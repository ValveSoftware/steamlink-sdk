// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {

const int VideoCaptureBufferPool::kInvalidId = -1;

// Tracker specifics for SharedMemory.
class VideoCaptureBufferPool::SharedMemTracker final : public Tracker {
 public:
  SharedMemTracker() : Tracker() {}

  bool Init(const gfx::Size& dimensions,
            media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            base::Lock* lock) override {
    DVLOG(2) << "allocating ShMem of " << dimensions.ToString();
    set_dimensions(dimensions);
    // |dimensions| can be 0x0 for trackers that do not require memory backing.
    set_max_pixel_count(dimensions.GetArea());
    set_pixel_format(format);
    set_storage_type(storage_type);
    mapped_size_ =
        media::VideoCaptureFormat(dimensions, 0.0f, format, storage_type)
            .ImageAllocationSize();
    if (!mapped_size_)
      return true;
    return shared_memory_.CreateAndMapAnonymous(mapped_size_);
  }

  std::unique_ptr<BufferHandle> GetBufferHandle() override {
    return base::WrapUnique(new SharedMemBufferHandle(this));
  }
  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    return shared_memory_.ShareToProcess(process_handle, new_handle);
  }
  bool ShareToProcess2(int plane,
                       base::ProcessHandle process_handle,
                       gfx::GpuMemoryBufferHandle* new_handle) override {
    NOTREACHED();
    return false;
  }

 private:
  // A simple proxy that implements the BufferHandle interface, providing
  // accessors to SharedMemTracker's memory and properties.
  class SharedMemBufferHandle : public VideoCaptureBufferPool::BufferHandle {
   public:
    // |tracker| must outlive SimpleBufferHandle. This is ensured since a
    // tracker is pinned until ownership of this SimpleBufferHandle is returned
    // to VideoCaptureBufferPool.
    explicit SharedMemBufferHandle(SharedMemTracker* tracker)
        : tracker_(tracker) {}
    ~SharedMemBufferHandle() override {}

    gfx::Size dimensions() const override { return tracker_->dimensions(); }
    size_t mapped_size() const override { return tracker_->mapped_size_; }
    void* data(int plane) override {
      DCHECK_EQ(plane, 0);
      return tracker_->shared_memory_.memory();
    }
    ClientBuffer AsClientBuffer(int plane) override {
      NOTREACHED();
      return nullptr;
    }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    base::FileDescriptor AsPlatformFile() override {
      return tracker_->shared_memory_.handle();
    }
#endif

   private:
    SharedMemTracker* const tracker_;
  };

  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
  size_t mapped_size_;
};

// Tracker specifics for GpuMemoryBuffer. Owns GpuMemoryBuffers and its
// associated pixel dimensions.
class VideoCaptureBufferPool::GpuMemoryBufferTracker final : public Tracker {
 public:
  GpuMemoryBufferTracker() : Tracker() {}

  ~GpuMemoryBufferTracker() override {
    for (const auto& gmb : gpu_memory_buffers_)
      gmb->Unmap();
  }

  bool Init(const gfx::Size& dimensions,
            media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            base::Lock* lock) override {
    DVLOG(2) << "allocating GMB for " << dimensions.ToString();
    // BrowserGpuMemoryBufferManager::current() may not be accessed on IO
    // Thread.
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(BrowserGpuMemoryBufferManager::current());
    // This class is only expected to be called with I420 buffer requests at
    // this point.
    DCHECK_EQ(format, media::PIXEL_FORMAT_I420);
    set_dimensions(dimensions);
    set_max_pixel_count(dimensions.GetArea());
    set_pixel_format(format);
    set_storage_type(storage_type);
    // |dimensions| can be 0x0 for trackers that do not require memory backing.
    if (dimensions.GetArea() == 0u)
      return true;

    base::AutoUnlock auto_unlock(*lock);
    const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format());
    for (size_t i = 0; i < num_planes; ++i) {
      const gfx::Size& size =
          media::VideoFrame::PlaneSize(pixel_format(), i, dimensions);
      gpu_memory_buffers_.push_back(
          BrowserGpuMemoryBufferManager::current()->AllocateGpuMemoryBuffer(
              size, gfx::BufferFormat::R_8,
              gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
              gpu::kNullSurfaceHandle));

      DLOG_IF(ERROR, !gpu_memory_buffers_[i]) << "Allocating GpuMemoryBuffer";
      if (!gpu_memory_buffers_[i] || !gpu_memory_buffers_[i]->Map())
        return false;
    }
    return true;
  }

  std::unique_ptr<BufferHandle> GetBufferHandle() override {
    DCHECK_EQ(gpu_memory_buffers_.size(),
              media::VideoFrame::NumPlanes(pixel_format()));
    return base::WrapUnique(new GpuMemoryBufferBufferHandle(this));
  }

  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    NOTREACHED();
    return false;
  }

  bool ShareToProcess2(int plane,
                       base::ProcessHandle process_handle,
                       gfx::GpuMemoryBufferHandle* new_handle) override {
    DCHECK_LE(plane, static_cast<int>(gpu_memory_buffers_.size()));

    const auto& current_gmb_handle = gpu_memory_buffers_[plane]->GetHandle();
    switch (current_gmb_handle.type) {
      case gfx::EMPTY_BUFFER:
        NOTREACHED();
        return false;
      case gfx::SHARED_MEMORY_BUFFER: {
        DCHECK(base::SharedMemory::IsHandleValid(current_gmb_handle.handle));
        base::SharedMemory shared_memory(
            base::SharedMemory::DuplicateHandle(current_gmb_handle.handle),
            false);
        shared_memory.ShareToProcess(process_handle, &new_handle->handle);
        DCHECK(base::SharedMemory::IsHandleValid(new_handle->handle));
        new_handle->type = gfx::SHARED_MEMORY_BUFFER;
        return true;
      }
      case gfx::IO_SURFACE_BUFFER:
      case gfx::SURFACE_TEXTURE_BUFFER:
      case gfx::OZONE_NATIVE_PIXMAP:
        *new_handle = current_gmb_handle;
        return true;
    }
    NOTREACHED();
    return true;
  }

 private:
  // A simple proxy that implements the BufferHandle interface, providing
  // accessors to GpuMemoryBufferTracker's memory and properties.
  class GpuMemoryBufferBufferHandle
      : public VideoCaptureBufferPool::BufferHandle {
   public:
    // |tracker| must outlive GpuMemoryBufferBufferHandle. This is ensured since
    // a tracker is pinned until ownership of this GpuMemoryBufferBufferHandle
    // is returned to VideoCaptureBufferPool.
    explicit GpuMemoryBufferBufferHandle(GpuMemoryBufferTracker* tracker)
        : tracker_(tracker) {}
    ~GpuMemoryBufferBufferHandle() override {}

    gfx::Size dimensions() const override { return tracker_->dimensions(); }
    size_t mapped_size() const override {
      return tracker_->dimensions().GetArea();
    }
    void* data(int plane) override {
      DCHECK_GE(plane, 0);
      DCHECK_LT(plane, static_cast<int>(tracker_->gpu_memory_buffers_.size()));
      DCHECK(tracker_->gpu_memory_buffers_[plane]);
      return tracker_->gpu_memory_buffers_[plane]->memory(0);
    }
    ClientBuffer AsClientBuffer(int plane) override {
      DCHECK_GE(plane, 0);
      DCHECK_LT(plane, static_cast<int>(tracker_->gpu_memory_buffers_.size()));
      return tracker_->gpu_memory_buffers_[plane]->AsClientBuffer();
    }
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    base::FileDescriptor AsPlatformFile() override {
      NOTREACHED();
      return base::FileDescriptor();
    }
#endif

   private:
    GpuMemoryBufferTracker* const tracker_;
  };

  // Owned references to GpuMemoryBuffers.
  std::vector<std::unique_ptr<gfx::GpuMemoryBuffer>> gpu_memory_buffers_;
};

// static
std::unique_ptr<VideoCaptureBufferPool::Tracker>
VideoCaptureBufferPool::Tracker::CreateTracker(
    media::VideoPixelStorage storage) {
  switch (storage) {
    case media::PIXEL_STORAGE_GPUMEMORYBUFFER:
      return base::WrapUnique(new GpuMemoryBufferTracker());
    case media::PIXEL_STORAGE_CPU:
      return base::WrapUnique(new SharedMemTracker());
  }
  NOTREACHED();
  return std::unique_ptr<VideoCaptureBufferPool::Tracker>();
}

VideoCaptureBufferPool::Tracker::~Tracker() {}

VideoCaptureBufferPool::VideoCaptureBufferPool(int count)
    : count_(count),
      next_buffer_id_(0),
      last_relinquished_buffer_id_(kInvalidId) {
  DCHECK_GT(count, 0);
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
  STLDeleteValues(&trackers_);
}

bool VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle,
    base::SharedMemoryHandle* new_handle) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }
  if (tracker->ShareToProcess(process_handle, new_handle))
    return true;
  DPLOG(ERROR) << "Error mapping memory";
  return false;
}

bool VideoCaptureBufferPool::ShareToProcess2(
    int buffer_id,
    int plane,
    base::ProcessHandle process_handle,
    gfx::GpuMemoryBufferHandle* new_handle) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }
  if (tracker->ShareToProcess2(plane, process_handle, new_handle))
    return true;
  DPLOG(ERROR) << "Error mapping memory";
  return false;
}

std::unique_ptr<VideoCaptureBufferPool::BufferHandle>
VideoCaptureBufferPool::GetBufferHandle(int buffer_id) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return std::unique_ptr<BufferHandle>();
  }

  DCHECK(tracker->held_by_producer());
  return tracker->GetBufferHandle();
}

int VideoCaptureBufferPool::ReserveForProducer(const gfx::Size& dimensions,
                                               media::VideoPixelFormat format,
                                               media::VideoPixelStorage storage,
                                               int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(dimensions, format, storage,
                                    buffer_id_to_drop);
}

void VideoCaptureBufferPool::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  tracker->set_held_by_producer(false);
  last_relinquished_buffer_id_ = buffer_id;
}

void VideoCaptureBufferPool::HoldForConsumers(
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  DCHECK(!tracker->consumer_hold_count());

  tracker->set_consumer_hold_count(num_clients);
  // Note: |held_by_producer()| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this tracker, e.g. a media::VideoFrame).
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK_GE(tracker->consumer_hold_count(), num_clients);

  tracker->set_consumer_hold_count(tracker->consumer_hold_count() -
                                   num_clients);
}

int VideoCaptureBufferPool::ResurrectLastForProducer(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage) {
  base::AutoLock lock(lock_);

  // Return early if the last relinquished buffer has been re-used already.
  if (last_relinquished_buffer_id_ == kInvalidId)
    return kInvalidId;

  // If there are no consumers reading from this buffer, then it's safe to
  // provide this buffer back to the producer (because the producer may
  // potentially modify the content). Check that the expected dimensions,
  // format, and storage match.
  TrackerMap::iterator it = trackers_.find(last_relinquished_buffer_id_);
  DCHECK(it != trackers_.end());
  DCHECK(!it->second->held_by_producer());
  if (it->second->consumer_hold_count() == 0 &&
      it->second->dimensions() == dimensions &&
      it->second->pixel_format() == format &&
      it->second->storage_type() == storage) {
    it->second->set_held_by_producer(true);
    const int resurrected_buffer_id = last_relinquished_buffer_id_;
    last_relinquished_buffer_id_ = kInvalidId;
    return resurrected_buffer_id;
  }

  return kInvalidId;
}

double VideoCaptureBufferPool::GetBufferPoolUtilization() const {
  base::AutoLock lock(lock_);
  int num_buffers_held = 0;
  for (const auto& entry : trackers_) {
    Tracker* const tracker = entry.second;
    if (tracker->held_by_producer() || tracker->consumer_hold_count() > 0)
      ++num_buffers_held;
  }
  return static_cast<double>(num_buffers_held) / count_;
}

int VideoCaptureBufferPool::ReserveForProducerInternal(
    const gfx::Size& dimensions,
    media::VideoPixelFormat pixel_format,
    media::VideoPixelStorage storage_type,
    int* buffer_id_to_drop) {
  lock_.AssertAcquired();

  const size_t size_in_pixels = dimensions.GetArea();
  // Look for a tracker that's allocated, big enough, and not in use. Track the
  // largest one that's not big enough, in case we have to reallocate a tracker.
  *buffer_id_to_drop = kInvalidId;
  size_t largest_size_in_pixels = 0;
  TrackerMap::iterator tracker_of_last_resort = trackers_.end();
  TrackerMap::iterator tracker_to_drop = trackers_.end();
  for (TrackerMap::iterator it = trackers_.begin(); it != trackers_.end();
       ++it) {
    Tracker* const tracker = it->second;
    if (!tracker->consumer_hold_count() && !tracker->held_by_producer()) {
      if (tracker->max_pixel_count() >= size_in_pixels &&
          (tracker->pixel_format() == pixel_format) &&
          (tracker->storage_type() == storage_type)) {
        if (it->first == last_relinquished_buffer_id_) {
          // This buffer would do just fine, but avoid returning it because the
          // client may want to resurrect it. It will be returned perforce if
          // the pool has reached it's maximum limit (see code below).
          tracker_of_last_resort = it;
          continue;
        }
        // Existing tracker is big enough and has correct format. Reuse it.
        tracker->set_dimensions(dimensions);
        tracker->set_held_by_producer(true);
        return it->first;
      }
      if (tracker->max_pixel_count() > largest_size_in_pixels) {
        largest_size_in_pixels = tracker->max_pixel_count();
        tracker_to_drop = it;
      }
    }
  }

  // Preferably grow the pool by creating a new tracker. If we're at maximum
  // size, then try using |tracker_of_last_resort| or reallocate by deleting an
  // existing one instead.
  if (trackers_.size() == static_cast<size_t>(count_)) {
    if (tracker_of_last_resort != trackers_.end()) {
      last_relinquished_buffer_id_ = kInvalidId;
      tracker_of_last_resort->second->set_dimensions(dimensions);
      tracker_of_last_resort->second->set_held_by_producer(true);
      return tracker_of_last_resort->first;
    }
    if (tracker_to_drop == trackers_.end()) {
      // We're out of space, and can't find an unused tracker to reallocate.
      return kInvalidId;
    }
    if (tracker_to_drop->first == last_relinquished_buffer_id_)
      last_relinquished_buffer_id_ = kInvalidId;
    *buffer_id_to_drop = tracker_to_drop->first;
    delete tracker_to_drop->second;
    trackers_.erase(tracker_to_drop);
  }

  // Create the new tracker.
  const int buffer_id = next_buffer_id_++;

  std::unique_ptr<Tracker> tracker = Tracker::CreateTracker(storage_type);
  // TODO(emircan): We pass the lock here to solve GMB allocation issue, see
  // crbug.com/545238.
  if (!tracker->Init(dimensions, pixel_format, storage_type, &lock_)) {
    DLOG(ERROR) << "Error initializing Tracker";
    return kInvalidId;
  }

  tracker->set_held_by_producer(true);
  trackers_[buffer_id] = tracker.release();

  return buffer_id;
}

VideoCaptureBufferPool::Tracker* VideoCaptureBufferPool::GetTracker(
    int buffer_id) {
  TrackerMap::const_iterator it = trackers_.find(buffer_id);
  return (it == trackers_.end()) ? NULL : it->second;
}

}  // namespace content
