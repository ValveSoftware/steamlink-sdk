// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/layout.h"

using content::RenderWidgetHostViewMac;

// Bare bones obj-c class for testing purposes.
@interface RenderWidgetHostViewMacEditCommandHelperTestClass : NSObject
@end

@implementation RenderWidgetHostViewMacEditCommandHelperTestClass
@end

// Class that owns a RenderWidgetHostViewMac.
@interface RenderWidgetHostViewMacOwner :
    NSObject<RenderWidgetHostViewMacOwner> {
  RenderWidgetHostViewMac* rwhvm_;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm;
@end

@implementation RenderWidgetHostViewMacOwner

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm {
  if ((self = [super init])) {
    rwhvm_ = rwhvm;
  }
  return self;
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return rwhvm_;
}

@end

namespace content {
namespace {

// Returns true if all the edit command names in the array are present in
// test_obj.  edit_commands is a list of NSStrings, selector names are formed
// by appending a trailing ':' to the string.
bool CheckObjectRespondsToEditCommands(NSArray* edit_commands, id test_obj) {
  for (NSString* edit_command_name in edit_commands) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    if (![test_obj respondsToSelector:NSSelectorFromString(sel_str)]) {
      return false;
    }
  }
  return true;
}

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  ~MockRenderWidgetHostDelegate() override {}

  private:
   void Cut() override {}
   void Copy() override {}
   void Paste() override {}
   void SelectAll() override {}
};

// Create a RenderWidget for which we can filter messages.
class RenderWidgetHostEditCommandCounter : public RenderWidgetHostImpl {
 public:
  RenderWidgetHostEditCommandCounter(RenderWidgetHostDelegate* delegate,
                                     RenderProcessHost* process,
                                     int32_t routing_id)
      : RenderWidgetHostImpl(delegate, process, routing_id, false),
        edit_command_message_count_(0) {}

  bool Send(IPC::Message* message) override {
    if (message->type() == InputMsg_ExecuteEditCommand::ID)
      edit_command_message_count_++;
    return RenderWidgetHostImpl::Send(message);
  }

  unsigned int edit_command_message_count_;
};

class RenderWidgetHostViewMacEditCommandHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    ImageTransportFactory::InitializeForUnitTests(
        std::unique_ptr<ImageTransportFactory>(
            new NoTransportImageTransportFactory));
  }
  void TearDown() override { ImageTransportFactory::Terminate(); }
};

}  // namespace

// Tests that editing commands make it through the pipeline all the way to
// RenderWidgetHost.
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest,
       TestEditingCommandDelivery) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost process_host(&browser_context);

  // Populates |g_supported_scale_factors|.
  std::vector<ui::ScaleFactor> supported_factors;
  supported_factors.push_back(ui::SCALE_FACTOR_100P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);

  int32_t routing_id = process_host.GetNextRoutingID();
  RenderWidgetHostEditCommandCounter* render_widget =
      new RenderWidgetHostEditCommandCounter(&delegate, &process_host,
                                             routing_id);

  base::mac::ScopedNSAutoreleasePool pool;

  base::MessageLoop message_loop;
  ui::WindowResizeHelperMac::Get()->Init(
    base::MessageLoop::current()->task_runner());

  // Owned by its |cocoa_view()|, i.e. |rwhv_cocoa|.
  RenderWidgetHostViewMac* rwhv_mac = new RenderWidgetHostViewMac(
      render_widget, false);
  base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa(
      [rwhv_mac->cocoa_view() retain]);

  RenderWidgetHostViewMacEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();
  RenderWidgetHostViewMacOwner* rwhwvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc]
          initWithRenderWidgetHostViewMac:rwhv_mac] autorelease];

  helper.AddEditingSelectorsToClass([rwhwvm_owner class]);

  for (NSString* edit_command_name in edit_command_strings) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    [rwhwvm_owner performSelector:NSSelectorFromString(sel_str) withObject:nil];
  }

  size_t num_edit_commands = [edit_command_strings count];
  EXPECT_EQ(render_widget->edit_command_message_count_, num_edit_commands);
  rwhv_cocoa.reset();
  pool.Recycle();

  // The |render_widget|'s process needs to be deleted within |message_loop|.
  delete render_widget;

  ui::WindowResizeHelperMac::Get()->ShutdownForTests();
}

// Test RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest,
       TestAddEditingSelectorsToClass) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();
  ASSERT_GT([edit_command_strings count], 0U);

  // Create a class instance and add methods to the class.
  RenderWidgetHostViewMacEditCommandHelperTestClass* test_obj =
      [[[RenderWidgetHostViewMacEditCommandHelperTestClass alloc] init]
          autorelease];

  // Check that edit commands aren't already attached to the object.
  ASSERT_FALSE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands where added.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  // AddEditingSelectorsToClass() should be idempotent.
  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands are still there.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));
}

// Test RenderWidgetHostViewMacEditCommandHelper::IsMenuItemEnabled.
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest, TestMenuItemEnabling) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  RenderWidgetHostViewMacOwner* rwhvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc] init] autorelease];

  // The select all menu should always be enabled.
  SEL select_all = NSSelectorFromString(@"selectAll:");
  ASSERT_TRUE(helper.IsMenuItemEnabled(select_all, rwhvm_owner));

  // Random selectors should be enabled by the function.
  SEL garbage_selector = NSSelectorFromString(@"randomGarbageSelector:");
  ASSERT_FALSE(helper.IsMenuItemEnabled(garbage_selector, rwhvm_owner));

  // TODO(jeremy): Currently IsMenuItemEnabled just returns true for all edit
  // selectors.  Once we go past that we should do more extensive testing here.
}

}  // namespace content
