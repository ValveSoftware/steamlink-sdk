// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/frame_load_waiter.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "url/gurl.h"

using blink::WebFrame;

namespace extensions {
namespace {

class ScriptContextTest : public ChromeRenderViewTest {
 protected:
  GURL GetEffectiveDocumentURL(const WebFrame* frame) {
    return ScriptContext::GetEffectiveDocumentURL(
        frame, frame->document().url(), true);
  }
};

TEST_F(ScriptContextTest, GetEffectiveDocumentURL) {
  GURL top_url("http://example.com/");
  GURL different_url("http://example.net/");
  GURL blank_url("about:blank");
  GURL srcdoc_url("about:srcdoc");

  const char frame_html[] =
      "<iframe name='frame1' srcdoc=\""
      "  <iframe name='frame1_1'></iframe>"
      "  <iframe name='frame1_2' sandbox=''></iframe>"
      "\"></iframe>"
      "<iframe name='frame2' sandbox='' srcdoc=\""
      "  <iframe name='frame2_1'></iframe>"
      "\"></iframe>"
      "<iframe name='frame3'></iframe>";

  const char frame3_html[] = "<iframe name='frame3_1'></iframe>";

  WebFrame* frame = GetMainFrame();
  ASSERT_TRUE(frame);

  frame->loadHTMLString(frame_html, top_url);
  content::FrameLoadWaiter(content::RenderFrame::FromWebFrame(frame)).Wait();

  WebFrame* frame1 = frame->firstChild();
  ASSERT_TRUE(frame1);
  ASSERT_EQ("frame1", frame1->uniqueName());
  WebFrame* frame1_1 = frame1->firstChild();
  ASSERT_TRUE(frame1_1);
  ASSERT_EQ("frame1_1", frame1_1->uniqueName());
  WebFrame* frame1_2 = frame1_1->nextSibling();
  ASSERT_TRUE(frame1_2);
  ASSERT_EQ("frame1_2", frame1_2->uniqueName());
  WebFrame* frame2 = frame1->nextSibling();
  ASSERT_TRUE(frame2);
  ASSERT_EQ("frame2", frame2->uniqueName());
  WebFrame* frame2_1 = frame2->firstChild();
  ASSERT_TRUE(frame2_1);
  ASSERT_EQ("frame2_1", frame2_1->uniqueName());
  WebFrame* frame3 = frame2->nextSibling();
  ASSERT_TRUE(frame3);
  ASSERT_EQ("frame3", frame3->uniqueName());

  // Load a blank document in a frame from a different origin.
  frame3->loadHTMLString(frame3_html, different_url);
  content::FrameLoadWaiter(content::RenderFrame::FromWebFrame(frame3)).Wait();

  WebFrame* frame3_1 = frame3->firstChild();
  ASSERT_TRUE(frame3_1);
  ASSERT_EQ("frame3_1", frame3_1->uniqueName());

  // Top-level frame
  EXPECT_EQ(GetEffectiveDocumentURL(frame), top_url);
  // top -> srcdoc = inherit
  EXPECT_EQ(GetEffectiveDocumentURL(frame1), top_url);
  // top -> srcdoc -> about:blank = inherit
  EXPECT_EQ(GetEffectiveDocumentURL(frame1_1), top_url);
  // top -> srcdoc -> about:blank sandboxed = same URL
  EXPECT_EQ(GetEffectiveDocumentURL(frame1_2), blank_url);

  // top -> srcdoc [sandboxed] = same URL
  EXPECT_EQ(GetEffectiveDocumentURL(frame2), srcdoc_url);
  // top -> srcdoc [sandboxed] -> about:blank = same URL
  EXPECT_EQ(GetEffectiveDocumentURL(frame2_1), blank_url);

  // top -> different origin = different origin
  EXPECT_EQ(GetEffectiveDocumentURL(frame3), different_url);
  // top -> different origin -> about:blank = inherit
  EXPECT_EQ(GetEffectiveDocumentURL(frame3_1), different_url);
}

}  // namespace
}  // namespace extensions
