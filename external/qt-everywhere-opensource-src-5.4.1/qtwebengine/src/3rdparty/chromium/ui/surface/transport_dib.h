// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SURFACE_TRANSPORT_DIB_H_
#define UI_SURFACE_TRANSPORT_DIB_H_

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "ui/surface/surface_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

class SkCanvas;

// -----------------------------------------------------------------------------
// A TransportDIB is a block of memory that is used to transport pixels
// between processes: from the renderer process to the browser, and
// between renderer and plugin processes.
// -----------------------------------------------------------------------------
class SURFACE_EXPORT TransportDIB {
 public:
  ~TransportDIB();

  // Two typedefs are defined. A Handle is the type which can be sent over
  // the wire so that the remote side can map the transport DIB. The Id typedef
  // is sufficient to identify the transport DIB when you know that the remote
  // side already may have it mapped.
#if defined(OS_WIN)
  typedef HANDLE Handle;
  // On Windows, the Id type includes a sequence number (epoch) to solve an ABA
  // issue:
  //   1) Process A creates a transport DIB with HANDLE=1 and sends to B.
  //   2) Process B maps the transport DIB and caches 1 -> DIB.
  //   3) Process A closes the transport DIB and creates a new one. The new DIB
  //      is also assigned HANDLE=1.
  //   4) Process A sends the Handle to B, but B incorrectly believes that it
  //      already has it cached.
  struct HandleAndSequenceNum {
    HandleAndSequenceNum()
        : handle(NULL),
          sequence_num(0) {
    }

    HandleAndSequenceNum(HANDLE h, uint32 seq_num)
        : handle(h),
          sequence_num(seq_num) {
    }

    bool operator==(const HandleAndSequenceNum& other) const {
      return other.handle == handle && other.sequence_num == sequence_num;
    }

    bool operator<(const HandleAndSequenceNum& other) const {
      // Use the lexicographic order on the tuple <handle, sequence_num>.
      if (other.handle != handle)
        return other.handle < handle;
      return other.sequence_num < sequence_num;
    }

    HANDLE handle;
    uint32 sequence_num;
  };
  typedef HandleAndSequenceNum Id;

  // Returns a default, invalid handle, that is meant to indicate a missing
  // Transport DIB.
  static Handle DefaultHandleValue() { return NULL; }
#else  // OS_POSIX
  typedef base::SharedMemoryHandle Handle;
  // On POSIX, the inode number of the backing file is used as an id.
#if defined(OS_ANDROID)
  typedef base::SharedMemoryHandle Id;
#else
  typedef base::SharedMemoryId Id;
#endif

  // Returns a default, invalid handle, that is meant to indicate a missing
  // Transport DIB.
  static Handle DefaultHandleValue() { return Handle(); }
#endif

  // Create a new TransportDIB, returning NULL on failure.
  //
  // The size is the minimum size in bytes of the memory backing the transport
  // DIB (we may actually allocate more than that to give us better reuse when
  // cached).
  //
  // The sequence number is used to uniquely identify the transport DIB. It
  // should be unique for all transport DIBs ever created in the same
  // renderer.
  static TransportDIB* Create(size_t size, uint32 sequence_num);

  // Map the referenced transport DIB.  The caller owns the returned object.
  // Returns NULL on failure.
  static TransportDIB* Map(Handle transport_dib);

  // Create a new |TransportDIB| with a handle to the shared memory. This
  // always returns a valid pointer. The DIB is not mapped.
  static TransportDIB* CreateWithHandle(Handle handle);

  // Returns true if the handle is valid.
  static bool is_valid_handle(Handle dib);

  // Returns true if the ID refers to a valid dib.
  static bool is_valid_id(Id id);

  // Returns a canvas using the memory of this TransportDIB. The returned
  // pointer will be owned by the caller. The bitmap will be of the given size,
  // which should fit inside this memory.
  //
  // On POSIX, this |TransportDIB| will be mapped if not already. On Windows,
  // this |TransportDIB| will NOT be mapped and should not be mapped prior,
  // because PlatformCanvas will map the file internally.
  //
  // Will return NULL on allocation failure. This could be because the image
  // is too large to map into the current process' address space.
  SkCanvas* GetPlatformCanvas(int w, int h);

  // Map the DIB into the current process if it is not already. This is used to
  // map a DIB that has already been created. Returns true if the DIB is mapped.
  bool Map();

  // Return a pointer to the shared memory.
  void* memory() const;

  // Return the maximum size of the shared memory. This is not the amount of
  // data which is valid, you have to know that via other means, this is simply
  // the maximum amount that /could/ be valid.
  size_t size() const { return size_; }

  // Return the identifier which can be used to refer to this shared memory
  // on the wire.
  Id id() const;

  // Return a handle to the underlying shared memory. This can be sent over the
  // wire to give this transport DIB to another process.
  Handle handle() const;

 private:
  TransportDIB();

  // Verifies that the dib can hold a canvas of the requested dimensions.
  bool VerifyCanvasSize(int w, int h);

  explicit TransportDIB(base::SharedMemoryHandle dib);
  base::SharedMemory shared_memory_;
  uint32 sequence_num_;
  size_t size_;  // length, in bytes

  DISALLOW_COPY_AND_ASSIGN(TransportDIB);
};

#endif  // UI_SURFACE_TRANSPORT_DIB_H_
