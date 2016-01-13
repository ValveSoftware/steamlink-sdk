// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "gin/handle.h"
#include "gin/per_isolate_data.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

namespace {

class TestGinObject : public gin::Wrappable<TestGinObject> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TestGinObject> Create(v8::Isolate* isolate, bool* alive) {
    return gin::CreateHandle(isolate, new TestGinObject(alive));
  }

 private:
  TestGinObject(bool* alive) : alive_(alive) { *alive_ = true; }
  virtual ~TestGinObject() { *alive_ = false; }

  bool* alive_;

  DISALLOW_COPY_AND_ASSIGN(TestGinObject);
};

gin::WrapperInfo TestGinObject::kWrapperInfo = { gin::kEmbedderNativeGin };

class GinBrowserTest : public RenderViewTest {
 public:
  GinBrowserTest() {}
  virtual ~GinBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kJavaScriptFlags, "--expose_gc");

    RenderViewTest::SetUp();
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(GinBrowserTest);
};

// Test that garbage collection doesn't crash if a gin-wrapped object is
// present.
TEST_F(GinBrowserTest, GinAndGarbageCollection) {
  LoadHTML("<!doctype html>");

  bool alive = false;

  {
    v8::Isolate* isolate = blink::mainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(
        view_->GetWebView()->mainFrame()->mainWorldScriptContext());

    // We create the object inside a scope so it's not kept alive by a handle
    // on the stack.
    TestGinObject::Create(isolate, &alive);
  }

  CHECK(alive);

  // Should not crash.
  v8::V8::LowMemoryNotification();

  CHECK(!alive);
}

} // namespace

}  // namespace content
