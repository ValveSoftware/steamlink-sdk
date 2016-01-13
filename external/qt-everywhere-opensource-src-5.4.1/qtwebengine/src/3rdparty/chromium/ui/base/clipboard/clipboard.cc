// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace ui {

namespace {

// Valides a shared bitmap on the clipboard.
// Returns true if the clipboard data makes sense and it's safe to access the
// bitmap.
bool ValidateAndMapSharedBitmap(size_t bitmap_bytes,
                                base::SharedMemory* bitmap_data) {
  using base::SharedMemory;

  if (!bitmap_data || !SharedMemory::IsHandleValid(bitmap_data->handle()))
    return false;

  if (!bitmap_data->Map(bitmap_bytes)) {
    PLOG(ERROR) << "Failed to map bitmap memory";
    return false;
  }
  return true;
}

// A list of allowed threads. By default, this is empty and no thread checking
// is done (in the unit test case), but a user (like content) can set which
// threads are allowed to call this method.
typedef std::vector<base::PlatformThreadId> AllowedThreadsVector;
static base::LazyInstance<AllowedThreadsVector> g_allowed_threads =
    LAZY_INSTANCE_INITIALIZER;

// Mapping from threads to clipboard objects.
typedef std::map<base::PlatformThreadId, Clipboard*> ClipboardMap;
static base::LazyInstance<ClipboardMap> g_clipboard_map =
    LAZY_INSTANCE_INITIALIZER;

// Mutex that controls access to |g_clipboard_map|.
static base::LazyInstance<base::Lock>::Leaky
    g_clipboard_map_lock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void Clipboard::SetAllowedThreads(
    const std::vector<base::PlatformThreadId>& allowed_threads) {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  g_allowed_threads.Get().clear();
  std::copy(allowed_threads.begin(), allowed_threads.end(),
            std::back_inserter(g_allowed_threads.Get()));
}

// static
Clipboard* Clipboard::GetForCurrentThread() {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  base::PlatformThreadId id = base::PlatformThread::CurrentId();

  AllowedThreadsVector* allowed_threads = g_allowed_threads.Pointer();
  if (!allowed_threads->empty()) {
    bool found = false;
    for (AllowedThreadsVector::const_iterator it = allowed_threads->begin();
         it != allowed_threads->end(); ++it) {
      if (*it == id) {
        found = true;
        break;
      }
    }

    DCHECK(found);
  }

  ClipboardMap* clipboard_map = g_clipboard_map.Pointer();
  ClipboardMap::iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    return it->second;

  Clipboard* clipboard = new ui::Clipboard;
  clipboard_map->insert(std::make_pair(id, clipboard));
  return clipboard;
}

void Clipboard::DestroyClipboardForCurrentThread() {
  base::AutoLock lock(g_clipboard_map_lock.Get());

  ClipboardMap* clipboard_map = g_clipboard_map.Pointer();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();
  ClipboardMap::iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end()) {
    delete it->second;
    clipboard_map->erase(it);
  }
}

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  // All types apart from CBF_WEBKIT need at least 1 non-empty param.
  if (type != CBF_WEBKIT && (params.empty() || params[0].empty()))
    return;
  // Some other types need a non-empty 2nd param.
  if ((type == CBF_BOOKMARK || type == CBF_SMBITMAP || type == CBF_DATA) &&
      (params.size() != 2 || params[1].empty()))
    return;
  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      }
      break;

    case CBF_RTF:
      WriteRTF(&(params[0].front()), params[0].size());
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_SMBITMAP: {
      using base::SharedMemory;
      using base::SharedMemoryHandle;

      if (params[0].size() != sizeof(SharedMemory*) ||
          params[1].size() != sizeof(gfx::Size)) {
        return;
      }

      SkBitmap bitmap;
      const gfx::Size* unvalidated_size =
          reinterpret_cast<const gfx::Size*>(&params[1].front());
      // Let Skia do some sanity checking for us (no negative widths/heights, no
      // overflows while calculating bytes per row, etc).
      if (!bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                            unvalidated_size->width(),
                            unvalidated_size->height())) {
        return;
      }
      // Make sure the size is representable as a signed 32-bit int, so
      // SkBitmap::getSize() won't be truncated.
      if (!sk_64_isS32(bitmap.computeSize64()))
        return;

      // It's OK to cast away constness here since we map the handle as
      // read-only.
      const char* raw_bitmap_data_const =
          reinterpret_cast<const char*>(&params[0].front());
      char* raw_bitmap_data = const_cast<char*>(raw_bitmap_data_const);
      scoped_ptr<SharedMemory> bitmap_data(
          *reinterpret_cast<SharedMemory**>(raw_bitmap_data));

      if (!ValidateAndMapSharedBitmap(bitmap.getSize(), bitmap_data.get()))
        return;
      bitmap.setPixels(bitmap_data->memory());

      WriteBitmap(bitmap);
      break;
    }

    case CBF_DATA:
      WriteData(
          FormatType::Deserialize(
              std::string(&(params[0].front()), params[0].size())),
          &(params[1].front()),
          params[1].size());
      break;

    default:
      NOTREACHED();
  }
}

// static
bool Clipboard::ReplaceSharedMemHandle(ObjectMap* objects,
                                       base::SharedMemoryHandle bitmap_handle,
                                       base::ProcessHandle process) {
  using base::SharedMemory;
  bool has_shared_bitmap = false;

  for (ObjectMap::iterator iter = objects->begin(); iter != objects->end();
       ++iter) {
    if (iter->first == CBF_SMBITMAP) {
      // The code currently only accepts sending a single bitmap over this way.
      // Fail if we ever encounter more than one shmem bitmap structure to fill.
      if (has_shared_bitmap)
        return false;

#if defined(OS_WIN)
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true, process);
#else
      SharedMemory* bitmap = new SharedMemory(bitmap_handle, true);
#endif

      // There must always be two parameters associated with each shmem bitmap.
      if (iter->second.size() != 2)
        return false;

      // We store the shared memory object pointer so it can be retrieved by the
      // UI thread (see DispatchObject()).
      iter->second[0].clear();
      for (size_t i = 0; i < sizeof(SharedMemory*); ++i)
        iter->second[0].push_back(reinterpret_cast<char*>(&bitmap)[i]);
      has_shared_bitmap = true;
    }
  }
  return true;
}

}  // namespace ui
