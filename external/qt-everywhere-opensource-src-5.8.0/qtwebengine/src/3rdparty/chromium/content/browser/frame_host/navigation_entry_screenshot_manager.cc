// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/threading/worker_pool.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

// Minimum delay between taking screenshots.
const int kMinScreenshotIntervalMS = 1000;

}

namespace content {

// Encodes the A8 SkBitmap to grayscale PNG in a worker thread.
class ScreenshotData : public base::RefCountedThreadSafe<ScreenshotData> {
 public:
  ScreenshotData() {
  }

  void EncodeScreenshot(const SkBitmap& bitmap, base::Closure callback) {
    if (!base::WorkerPool::PostTaskAndReply(FROM_HERE,
            base::Bind(&ScreenshotData::EncodeOnWorker,
                       this,
                       bitmap),
            callback,
            true)) {
      callback.Run();
    }
  }

  scoped_refptr<base::RefCountedBytes> data() const { return data_; }

 private:
  friend class base::RefCountedThreadSafe<ScreenshotData>;
  virtual ~ScreenshotData() {
  }

  void EncodeOnWorker(const SkBitmap& bitmap) {
    DCHECK_EQ(bitmap.colorType(), kAlpha_8_SkColorType);
    // Encode the A8 bitmap to grayscale PNG treating alpha as color intensity.
    std::vector<unsigned char> data;
    if (gfx::PNGCodec::EncodeA8SkBitmap(bitmap, &data))
      data_ = new base::RefCountedBytes(data);
  }

  scoped_refptr<base::RefCountedBytes> data_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotData);
};

NavigationEntryScreenshotManager::NavigationEntryScreenshotManager(
    NavigationControllerImpl* owner)
    : owner_(owner),
      min_screenshot_interval_ms_(kMinScreenshotIntervalMS),
      screenshot_factory_(this) {

}

NavigationEntryScreenshotManager::~NavigationEntryScreenshotManager() {
}

void NavigationEntryScreenshotManager::TakeScreenshot() {
  static bool overscroll_enabled = base::CommandLine::ForCurrentProcess()->
      GetSwitchValueASCII(switches::kOverscrollHistoryNavigation) != "0";
  if (!overscroll_enabled)
    return;

  NavigationEntryImpl* entry = owner_->GetLastCommittedEntry();
  if (!entry)
    return;

  if (!owner_->delegate()->CanOverscrollContent())
    return;

  RenderViewHost* render_view_host = owner_->delegate()->GetRenderViewHost();
  content::RenderWidgetHostView* view =
      render_view_host->GetWidget()->GetView();
  if (!view)
    return;

  // Make sure screenshots aren't taken too frequently.
  base::Time now = base::Time::Now();
  if (now - last_screenshot_time_ <
          base::TimeDelta::FromMilliseconds(min_screenshot_interval_ms_)) {
    return;
  }

  last_screenshot_time_ = now;

  TakeScreenshotImpl(render_view_host, entry);
}

// Implemented here and not in NavigationEntry because this manager keeps track
// of the total number of screen shots across all entries.
void NavigationEntryScreenshotManager::ClearAllScreenshots() {
  int count = owner_->GetEntryCount();
  for (int i = 0; i < count; ++i) {
    ClearScreenshot(owner_->GetEntryAtIndex(i));
  }
  DCHECK_EQ(GetScreenshotCount(), 0);
}

void NavigationEntryScreenshotManager::TakeScreenshotImpl(
    RenderViewHost* host,
    NavigationEntryImpl* entry) {
  DCHECK(host && host->GetWidget()->GetView());
  DCHECK(entry);
  host->GetWidget()->CopyFromBackingStore(
      gfx::Rect(), host->GetWidget()->GetView()->GetViewBounds().size(),
      base::Bind(&NavigationEntryScreenshotManager::OnScreenshotTaken,
                 screenshot_factory_.GetWeakPtr(), entry->GetUniqueID()),
      kAlpha_8_SkColorType);
}

void NavigationEntryScreenshotManager::SetMinScreenshotIntervalMS(
    int interval_ms) {
  DCHECK_GE(interval_ms, 0);
  min_screenshot_interval_ms_ = interval_ms;
}

void NavigationEntryScreenshotManager::OnScreenshotTaken(
    int unique_id,
    const SkBitmap& bitmap,
    ReadbackResponse response) {
  NavigationEntryImpl* entry = owner_->GetEntryWithUniqueID(unique_id);
  if (!entry) {
    LOG(ERROR) << "Invalid entry with unique id: " << unique_id;
    return;
  }

  if ((response != READBACK_SUCCESS) || bitmap.empty() || bitmap.isNull()) {
    if (!ClearScreenshot(entry))
      OnScreenshotSet(entry);
    return;
  }

  scoped_refptr<ScreenshotData> screenshot = new ScreenshotData();
  screenshot->EncodeScreenshot(
      bitmap,
      base::Bind(&NavigationEntryScreenshotManager::OnScreenshotEncodeComplete,
                 screenshot_factory_.GetWeakPtr(),
                 unique_id,
                 screenshot));
}

int NavigationEntryScreenshotManager::GetScreenshotCount() const {
  int screenshot_count = 0;
  int entry_count = owner_->GetEntryCount();
  for (int i = 0; i < entry_count; ++i) {
    NavigationEntryImpl* entry = owner_->GetEntryAtIndex(i);
    if (entry->screenshot().get())
      screenshot_count++;
  }
  return screenshot_count;
}

void NavigationEntryScreenshotManager::OnScreenshotEncodeComplete(
    int unique_id,
    scoped_refptr<ScreenshotData> screenshot) {
  NavigationEntryImpl* entry = owner_->GetEntryWithUniqueID(unique_id);
  if (!entry)
    return;
  entry->SetScreenshotPNGData(screenshot->data());
  OnScreenshotSet(entry);
}

void NavigationEntryScreenshotManager::OnScreenshotSet(
    NavigationEntryImpl* entry) {
  if (entry->screenshot().get())
    PurgeScreenshotsIfNecessary();
}

bool NavigationEntryScreenshotManager::ClearScreenshot(
    NavigationEntryImpl* entry) {
  if (!entry->screenshot().get())
    return false;

  entry->SetScreenshotPNGData(NULL);
  return true;
}

void NavigationEntryScreenshotManager::PurgeScreenshotsIfNecessary() {
  // Allow only a certain number of entries to keep screenshots.
  const int kMaxScreenshots = 10;
  int screenshot_count = GetScreenshotCount();
  if (screenshot_count < kMaxScreenshots)
    return;

  const int current = owner_->GetCurrentEntryIndex();
  const int num_entries = owner_->GetEntryCount();
  int available_slots = kMaxScreenshots;
  if (owner_->GetEntryAtIndex(current)->screenshot().get()) {
    --available_slots;
  }

  // Keep screenshots closer to the current navigation entry, and purge the ones
  // that are farther away from it. So in each step, look at the entries at
  // each offset on both the back and forward history, and start counting them
  // to make sure that the correct number of screenshots are kept in memory.
  // Note that it is possible for some entries to be missing screenshots (e.g.
  // when taking the screenshot failed for some reason). So there may be a state
  // where there are a lot of entries in the back history, but none of them has
  // any screenshot. In such cases, keep the screenshots for |kMaxScreenshots|
  // entries in the forward history list.
  int back = current - 1;
  int forward = current + 1;
  while (available_slots > 0 && (back >= 0 || forward < num_entries)) {
    if (back >= 0) {
      NavigationEntryImpl* entry = owner_->GetEntryAtIndex(back);
      if (entry->screenshot().get())
        --available_slots;
      --back;
    }

    if (available_slots > 0 && forward < num_entries) {
      NavigationEntryImpl* entry = owner_->GetEntryAtIndex(forward);
      if (entry->screenshot().get())
        --available_slots;
      ++forward;
    }
  }

  // Purge any screenshot at |back| or lower indices, and |forward| or higher
  // indices.
  while (screenshot_count > kMaxScreenshots && back >= 0) {
    NavigationEntryImpl* entry = owner_->GetEntryAtIndex(back);
    if (ClearScreenshot(entry))
      --screenshot_count;
    --back;
  }

  while (screenshot_count > kMaxScreenshots && forward < num_entries) {
    NavigationEntryImpl* entry = owner_->GetEntryAtIndex(forward);
    if (ClearScreenshot(entry))
      --screenshot_count;
    ++forward;
  }
  CHECK_GE(screenshot_count, 0);
  CHECK_LE(screenshot_count, kMaxScreenshots);
}

}  // namespace content
