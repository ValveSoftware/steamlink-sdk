// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <stddef.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

#include <X11/Xlib.h>

namespace ui {

namespace {

const char* kAtomsToCache[] = {
    "STRING",
    NULL
};

}  // namespace

class SelectionRequestorTest : public testing::Test {
 public:
  SelectionRequestorTest()
      : x_display_(gfx::GetXDisplay()),
        x_window_(None),
        atom_cache_(gfx::GetXDisplay(), kAtomsToCache) {
    atom_cache_.allow_uncached_atoms();
  }

  ~SelectionRequestorTest() override {}

  // Responds to the SelectionRequestor's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   |value|.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(XAtom selection,
                           XAtom target,
                           const std::string& value) {
    ui::SetStringProperty(x_window_,
                          requestor_->x_property_,
                          atom_cache_.GetAtom("STRING"),
                          value);

    XEvent xev;
    xev.type = SelectionNotify;
    xev.xselection.serial = 0u;
    xev.xselection.display = x_display_;
    xev.xselection.requestor = x_window_;
    xev.xselection.selection = selection;
    xev.xselection.target = target;
    xev.xselection.property = requestor_->x_property_;
    xev.xselection.time = CurrentTime;
    xev.xselection.type = SelectionNotify;
    requestor_->OnSelectionNotify(xev);
  }

 protected:
  void SetUp() override {
    // Make X11 synchronous for our display connection.
    XSynchronize(x_display_, True);

    // Create a window for the selection requestor to use.
    x_window_ = XCreateWindow(x_display_,
                              DefaultRootWindow(x_display_),
                              0, 0, 10, 10,    // x, y, width, height
                              0,               // border width
                              CopyFromParent,  // depth
                              InputOnly,
                              CopyFromParent,  // visual
                              0,
                              NULL);

    event_source_ = ui::PlatformEventSource::CreateDefault();
    CHECK(ui::PlatformEventSource::GetInstance());
    requestor_.reset(new SelectionRequestor(x_display_, x_window_, NULL));
  }

  void TearDown() override {
    requestor_.reset();
    event_source_.reset();
    XDestroyWindow(x_display_, x_window_);
    XSynchronize(x_display_, False);
  }

  Display* x_display_;

  // |requestor_|'s window.
  XID x_window_;

  std::unique_ptr<ui::PlatformEventSource> event_source_;
  std::unique_ptr<SelectionRequestor> requestor_;

  base::MessageLoopForUI message_loop_;
  X11AtomCache atom_cache_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionRequestorTest);
};

namespace {

// Converts |selection| to |target| and checks the returned values.
void PerformBlockingConvertSelection(SelectionRequestor* requestor,
                                     X11AtomCache* atom_cache,
                                     XAtom selection,
                                     XAtom target,
                                     const std::string& expected_data) {
  scoped_refptr<base::RefCountedMemory> out_data;
  size_t out_data_items = 0u;
  XAtom out_type = None;
  EXPECT_TRUE(requestor->PerformBlockingConvertSelection(
      selection, target, &out_data, &out_data_items, &out_type));
  EXPECT_EQ(expected_data, ui::RefCountedMemoryToString(out_data));
  EXPECT_EQ(expected_data.size(), out_data_items);
  EXPECT_EQ(atom_cache->GetAtom("STRING"), out_type);
}

}  // namespace

// Test that SelectionRequestor correctly handles receiving a request while it
// is processing another request.
TEST_F(SelectionRequestorTest, NestedRequests) {
  // Assume that |selection| will have no owner. If there is an owner, the owner
  // will set the property passed into the XConvertSelection() request which is
  // undesirable.
  XAtom selection = atom_cache_.GetAtom("FAKE_SELECTION");

  XAtom target1 = atom_cache_.GetAtom("TARGET1");
  XAtom target2 = atom_cache_.GetAtom("TARGET2");

  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  loop->PostTask(FROM_HERE,
                 base::Bind(&PerformBlockingConvertSelection,
                            base::Unretained(requestor_.get()),
                            base::Unretained(&atom_cache_),
                            selection,
                            target2,
                            "Data2"));
  loop->PostTask(FROM_HERE,
                 base::Bind(&SelectionRequestorTest::SendSelectionNotify,
                            base::Unretained(this),
                            selection,
                            target1,
                            "Data1"));
  loop->PostTask(FROM_HERE,
                 base::Bind(&SelectionRequestorTest::SendSelectionNotify,
                            base::Unretained(this),
                            selection,
                            target2,
                            "Data2"));
  PerformBlockingConvertSelection(
      requestor_.get(), &atom_cache_, selection, target1, "Data1");
}

}  // namespace ui
