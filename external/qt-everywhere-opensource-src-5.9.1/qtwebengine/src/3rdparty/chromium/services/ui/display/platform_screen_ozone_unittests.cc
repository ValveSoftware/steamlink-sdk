// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/display/platform_screen_ozone.h"
#include "services/ui/display/viewport_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/display/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {

using ui::DisplayMode;
using ui::DisplaySnapshot;
using testing::IsEmpty;
using testing::SizeIs;

namespace {

// Holds info about the display state we want to test.
struct DisplayState {
  int64_t id;
  ViewportMetrics metrics;
};

// Matchers that operate on DisplayState.
MATCHER_P(DisplayId, display_id, "") {
  *result_listener << "has id " << arg.id;
  return arg.id == display_id;
}

MATCHER_P(DisplaySize, size_string, "") {
  *result_listener << "has size " << arg.metrics.bounds.size().ToString();
  return arg.metrics.bounds.size().ToString() == size_string;
}

MATCHER_P(DisplayOrigin, origin_string, "") {
  *result_listener << "has origin " << arg.metrics.bounds.origin().ToString();
  return arg.metrics.bounds.origin().ToString() == origin_string;
}

// Make a DisplaySnapshot with specified id and size.
std::unique_ptr<DisplaySnapshot> MakeSnapshot(int64_t id,
                                              const gfx::Size& size) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetNativeMode(size)
      .SetCurrentMode(size)
      .Build();
}

// Test delegate to track what functions calls the delegate receives.
class TestPlatformScreenDelegate : public PlatformScreenDelegate {
 public:
  TestPlatformScreenDelegate() {}
  ~TestPlatformScreenDelegate() override {}

  const std::vector<DisplayState>& added() const { return added_; }
  const std::vector<DisplayState>& modified() const { return modified_; }

  // Returns a string containing the function calls that PlatformScreenDelegate
  // has received in the order they occured. Each function call will be in the
  // form "<action>(<id>)" and multiple function calls will be separated by ";".
  // For example, if display 2 was added then display 1 was modified, changes()
  // would return "Added(2);Modified(1)".
  const std::string& changes() const { return changes_; }

  void Reset() {
    added_.clear();
    modified_.clear();
    changes_.clear();
  }

 private:
  void AddChange(const std::string& name, const std::string& value) {
    if (!changes_.empty())
      changes_ += ";";
    changes_ += name + "(" + value + ")";
  }

  void OnDisplayAdded(int64_t id, const ViewportMetrics& metrics) override {
    added_.push_back({id, metrics});
    AddChange("Added", base::Int64ToString(id));
  }

  void OnDisplayRemoved(int64_t id) override {
    AddChange("Removed", base::Int64ToString(id));
  }

  void OnDisplayModified(int64_t id, const ViewportMetrics& metrics) override {
    modified_.push_back({id, metrics});
    AddChange("Modified", base::Int64ToString(id));
  }

  void OnPrimaryDisplayChanged(int64_t primary_display_id) override {
    AddChange("Primary", base::Int64ToString(primary_display_id));
  }

  std::vector<DisplayState> added_;
  std::vector<DisplayState> modified_;
  std::string changes_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformScreenDelegate);
};

}  // namespace

// Test fixture with helpers to act like ui::DisplayConfigurator and send
// OnDisplayModeChanged() to PlatformScreenOzone.
class PlatformScreenOzoneTest : public testing::Test {
 public:
  PlatformScreenOzoneTest() {}
  ~PlatformScreenOzoneTest() override {}

  PlatformScreenOzone* platform_screen() { return platform_screen_.get(); }
  TestPlatformScreenDelegate* delegate() { return &delegate_; }

  // Adds a display snapshot with specified ID and default size.
  void AddDisplay(int64_t id) { return AddDisplay(id, gfx::Size(1024, 768)); }

  // Adds a display snapshot with specified ID and size to list of snapshots.
  void AddDisplay(int64_t id, const gfx::Size& size) {
    snapshots_.push_back(MakeSnapshot(id, size));
    TriggerOnDisplayModeChanged();
  }

  // Removes display snapshot with specified ID.
  void RemoveDisplay(int64_t id) {
    snapshots_.erase(
        std::remove_if(snapshots_.begin(), snapshots_.end(),
                       [id](std::unique_ptr<DisplaySnapshot>& snapshot) {
                         return snapshot->display_id() == id;
                       }));
    TriggerOnDisplayModeChanged();
  }

  // Modify the size of the display snapshot with specified ID.
  void ModifyDisplay(int64_t id, const gfx::Size& size) {
    DisplaySnapshot* snapshot = GetSnapshotById(id);

    const DisplayMode* new_mode = nullptr;
    for (auto& mode : snapshot->modes()) {
      if (mode->size() == size) {
        new_mode = mode.get();
        break;
      }
    }

    if (!new_mode) {
      snapshot->add_mode(new DisplayMode(size, false, 30.0f));
      new_mode = snapshot->modes().back().get();
    }

    snapshot->set_current_mode(new_mode);
    TriggerOnDisplayModeChanged();
  }

  // Calls OnDisplayModeChanged with our list of display snapshots.
  void TriggerOnDisplayModeChanged() {
    std::vector<DisplaySnapshot*> snapshots_ptrs;
    for (auto& snapshot : snapshots_) {
      snapshots_ptrs.push_back(snapshot.get());
    }
    platform_screen_->OnDisplayModeChanged(snapshots_ptrs);
  }

 private:
  DisplaySnapshot* GetSnapshotById(int64_t id) {
    for (auto& snapshot : snapshots_) {
      if (snapshot->display_id() == id)
        return snapshot.get();
    }
    return nullptr;
  }

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
        switches::kScreenConfig, "none");

    testing::Test::SetUp();
    ui::OzonePlatform::InitializeForUI();
    platform_screen_ = base::MakeUnique<PlatformScreenOzone>();
    platform_screen_->Init(&delegate_);

    // Have all tests start with a 1024x768 display by default.
    AddDisplay(1, gfx::Size(1024, 768));
    TriggerOnDisplayModeChanged();

    // Double check the expected display exists and clear counters.
    ASSERT_THAT(delegate()->added(), SizeIs(1));
    ASSERT_THAT(delegate_.added()[0], DisplayId(1));
    ASSERT_THAT(delegate_.added()[0], DisplayOrigin("0,0"));
    ASSERT_THAT(delegate_.added()[0], DisplaySize("1024x768"));
    ASSERT_EQ("Added(1);Primary(1)", delegate()->changes());
    delegate_.Reset();
  }

  void TearDown() override {
    snapshots_.clear();
    delegate_.Reset();
    platform_screen_.reset();
  }

  TestPlatformScreenDelegate delegate_;
  std::unique_ptr<PlatformScreenOzone> platform_screen_;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots_;
};

TEST_F(PlatformScreenOzoneTest, AddDisplay) {
  AddDisplay(2);

  // Check that display 2 was added.
  EXPECT_EQ("Added(2)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, RemoveDisplay) {
  AddDisplay(2);
  delegate()->Reset();

  RemoveDisplay(2);

  // Check that display 2 was removed.
  EXPECT_EQ("Removed(2)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, RemoveFirstDisplay) {
  AddDisplay(2);
  delegate()->Reset();

  RemoveDisplay(1);

  // Check that display 1 was removed and display 2 was modified due to the
  // origin changing.
  EXPECT_EQ("Primary(2);Removed(1);Modified(2)", delegate()->changes());
  ASSERT_THAT(delegate()->modified(), SizeIs(1));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[0], DisplayOrigin("0,0"));
}

TEST_F(PlatformScreenOzoneTest, RemoveMultipleDisplay) {
  AddDisplay(2);
  AddDisplay(3);
  delegate()->Reset();

  RemoveDisplay(2);
  RemoveDisplay(3);

  // Check that display 2 was removed and display 3 is modifed (origin change),
  // then display 3 was removed.
  EXPECT_EQ("Removed(2);Modified(3);Removed(3)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, ModifyDisplaySize) {
  const gfx::Size size1(1920, 1200);
  const gfx::Size size2(1680, 1050);

  AddDisplay(2, size1);

  // Check that display 2 was added with expected size.
  ASSERT_THAT(delegate()->added(), SizeIs(1));
  EXPECT_THAT(delegate()->added()[0], DisplayId(2));
  EXPECT_THAT(delegate()->added()[0], DisplaySize(size1.ToString()));
  EXPECT_EQ("Added(2)", delegate()->changes());
  delegate()->Reset();

  ModifyDisplay(2, size2);

  // Check that display 2 was modified to have the new expected size.
  ASSERT_THAT(delegate()->modified(), SizeIs(1));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[0], DisplaySize(size2.ToString()));
  EXPECT_EQ("Modified(2)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, ModifyFirstDisplaySize) {
  const gfx::Size size(1920, 1200);

  AddDisplay(2, size);

  // Check that display 2 has the expected initial origin.
  EXPECT_EQ("Added(2)", delegate()->changes());
  ASSERT_THAT(delegate()->added(), SizeIs(1));
  EXPECT_THAT(delegate()->added()[0], DisplayOrigin("1024,0"));
  delegate()->Reset();

  ModifyDisplay(1, size);

  // Check that display 1 was modified with a new size and display 2 origin was
  // modified after.
  EXPECT_EQ("Modified(1);Modified(2)", delegate()->changes());
  ASSERT_THAT(delegate()->modified(), SizeIs(2));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(1));
  EXPECT_THAT(delegate()->modified()[0], DisplaySize(size.ToString()));
  EXPECT_THAT(delegate()->modified()[1], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[1], DisplayOrigin("1920,0"));
}

TEST_F(PlatformScreenOzoneTest, RemovePrimaryDisplay) {
  AddDisplay(2);
  delegate()->Reset();

  RemoveDisplay(1);

  // Check the primary display changed because the old primary was removed.
  EXPECT_EQ("Primary(2);Removed(1);Modified(2)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, RemoveLastDisplay) {
  RemoveDisplay(1);

  // Check that display 1 is removed and no updates for the primary display are
  // received.
  EXPECT_EQ("Removed(1)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, SwapPrimaryDisplay) {
  AddDisplay(2);
  delegate()->Reset();

  platform_screen()->SwapPrimaryDisplay();
  EXPECT_EQ("Primary(2)", delegate()->changes());
}

TEST_F(PlatformScreenOzoneTest, SwapPrimaryThreeDisplays) {
  AddDisplay(2);
  AddDisplay(3);
  EXPECT_EQ("Added(2);Added(3)", delegate()->changes());
  delegate()->Reset();

  platform_screen()->SwapPrimaryDisplay();
  platform_screen()->SwapPrimaryDisplay();
  platform_screen()->SwapPrimaryDisplay();
  EXPECT_EQ("Primary(2);Primary(3);Primary(1)", delegate()->changes());
}

}  // namespace display
