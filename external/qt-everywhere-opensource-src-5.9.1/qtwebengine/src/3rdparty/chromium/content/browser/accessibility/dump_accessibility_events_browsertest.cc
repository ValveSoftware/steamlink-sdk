// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"

namespace content {

typedef AccessibilityTreeFormatter::Filter Filter;

// Tests that the right platform-specific accessibility events are fired
// in response to things that happen in a web document.
//
// Similar to DumpAccessibilityTree in that each test consists of a
// single HTML file, possibly with a few special directives in comments,
// and then expectation files in text format for each platform.
//
// While DumpAccessibilityTree just loads the document and then
// prints out a text representation of the accessibility tree,
// DumpAccessibilityEvents loads the document, then executes the
// JavaScript function "go()", then it records and dumps all accessibility
// events generated as a result of that "go" function executing.
//
// How each event is dumped is platform-specific, but should be of the
// form:
//
// <event> on <node>
//
// ...where <event> is the name of the event, and <node> is a description
// of the node the event fired on, such as the node's role and name.
//
// As with DumpAccessibilityTree, DumpAccessibilityEvents takes the events
// dumped from that particular html file and compares it to the expectation
// file in the same directory (for example, test-name-expected-win.txt)
// and the test fails if they don't agree.
//
// Currently it's not possible to test for accessibility events that
// don't fire immediately (i.e. within the call scope of the call to "go()");
// the test framework calls "go()" and then sends a sentinel event signaling
// the end of the test; anything received after that is too late.
class DumpAccessibilityEventsTest : public DumpAccessibilityTestBase {
 public:
  void AddDefaultFilters(std::vector<Filter>* filters) override {
  }

  std::vector<std::string> Dump() override;

  void OnDiffFailed() override;
  void RunEventTest(const base::FilePath::CharType* file_path);

 private:
  base::string16 initial_tree_;
  base::string16 final_tree_;
};

std::vector<std::string> DumpAccessibilityEventsTest::Dump() {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  std::unique_ptr<AccessibilityEventRecorder> event_recorder(
      AccessibilityEventRecorder::Create(
          web_contents->GetRootBrowserAccessibilityManager()));

  // Save a copy of the accessibility tree (as a text dump); we'll
  // log this for the user later if the test fails.
  initial_tree_ = DumpUnfilteredAccessibilityTreeAsString();

  // Create a waiter that waits for any one accessibility event.
  // This will ensure that after calling the go() function, we
  // block until we've received an accessibility event generated as
  // a result of this function.
  std::unique_ptr<AccessibilityNotificationWaiter> waiter;
  waiter.reset(new AccessibilityNotificationWaiter(
      shell()->web_contents(), AccessibilityModeComplete, ui::AX_EVENT_NONE));

  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("go()"));

  // Wait for at least one accessibility event generated in response to
  // that function.
  waiter->WaitForNotification();

  // More than one accessibility event could have been generated.
  // To make sure we've received all accessibility events, add a
  // sentinel by calling AccessibilityHitTest and waiting for a HOVER
  // event in response.
  waiter.reset(new AccessibilityNotificationWaiter(
      shell()->web_contents(), AccessibilityModeComplete, ui::AX_EVENT_HOVER));
  BrowserAccessibilityManager* manager =
      web_contents->GetRootBrowserAccessibilityManager();
  manager->HitTest(gfx::Point(0, 0));
  waiter->WaitForNotification();

  // Save a copy of the final accessibility tree (as a text dump); we'll
  // log this for the user later if the test fails.
  final_tree_ = DumpUnfilteredAccessibilityTreeAsString();

  // Dump the event logs, running them through any filters specified
  // in the HTML file.
  std::vector<std::string> event_logs = event_recorder->event_logs();
  std::vector<std::string> result;
  for (size_t i = 0; i < event_logs.size(); ++i) {
    if (AccessibilityTreeFormatter::MatchesFilters(
            filters_, base::UTF8ToUTF16(event_logs[i]), true)) {
      result.push_back(event_logs[i]);
    }
  }
  return result;
}

void DumpAccessibilityEventsTest::OnDiffFailed() {
  printf("\n");
  printf("Initial accessibility tree (after load complete):\n");
  printf("%s\n", base::UTF16ToUTF8(initial_tree_).c_str());
  printf("\n");
  printf("Final accessibility tree after events fired:\n");
  printf("%s\n", base::UTF16ToUTF8(final_tree_).c_str());
  printf("\n");
}

void DumpAccessibilityEventsTest::RunEventTest(
    const base::FilePath::CharType* file_path) {
  base::FilePath test_path = GetTestFilePath("accessibility", "event");

  {
    base::ThreadRestrictions::ScopedAllowIO allow_io_for_test_verification;
    ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
  }

  base::FilePath event_file = test_path.Append(base::FilePath(file_path));
  RunTest(event_file, "accessibility/event");
}

// TODO(dmazzoni): port these tests to run on all platforms.
// TODO(crbug.com/617146): All tests flaky on Windows 8.
#if defined(OS_WIN) || defined(OS_MACOSX)

// This is tasteless, but then so's the snippet it's replacing.
#if defined(OS_WIN)
#define DISABLED_ON_WIN(name) DISABLED_ ## name
#else
#define DISABLED_ON_WIN(name) name
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#define DISABLED_ON_WIN_AND_MAC(name) DISABLED_ ## name
#else
#define DISABLED_ON_WIN_AND_MAC(name) name
#endif

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsAriaComboBoxCollapse) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-collapse.html"));
}

// https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN(AccessibilityEventsAriaComboBoxExpand)) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-expand.html"));
}

// Mac: https://crbug.com/615411, Win: https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN_AND_MAC(AccessibilityEventsAriaComboBoxFocus)) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-focus.html"));
}

// https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN(AccessibilityEventsAriaComboBoxNext)) {
  RunEventTest(FILE_PATH_LITERAL("aria-combo-box-next.html"));
}

// https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN(AccessibilityEventsAddAlert)) {
  RunEventTest(FILE_PATH_LITERAL("add-alert.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddChild) {
  RunEventTest(FILE_PATH_LITERAL("add-child.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsAddSubtree) {
  RunEventTest(FILE_PATH_LITERAL("add-subtree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsCheckedStateChanged) {
  RunEventTest(FILE_PATH_LITERAL("checked-state-changed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSDisplay) {
  RunEventTest(FILE_PATH_LITERAL("css-display.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsCSSVisibility) {
  RunEventTest(FILE_PATH_LITERAL("css-visibility.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsDescriptionChange) {
  RunEventTest(FILE_PATH_LITERAL("description-change.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsExpandedChange) {
  RunEventTest(FILE_PATH_LITERAL("expanded-change.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsInnerHtmlChange) {
  RunEventTest(FILE_PATH_LITERAL("inner-html-change.html"));
}

#if defined(OS_MACOSX)
// Mac failures: http://crbug.com/598527.
#define MAYBE_AccessibilityEventsInputTypeTextValueChanged \
    DISABLED_AccessibilityEventsInputTypeTextValueChanged
#else
#define MAYBE_AccessibilityEventsInputTypeTextValueChanged \
    AccessibilityEventsInputTypeTextValueChanged
#endif
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       MAYBE_AccessibilityEventsInputTypeTextValueChanged) {
  RunEventTest(FILE_PATH_LITERAL("input-type-text-value-changed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxFocus) {
  RunEventTest(FILE_PATH_LITERAL("listbox-focus.html"));
}

// Flaky on Windows: http://crbug.com/486861
// Flaky on Mac: http://crbug.com/588271
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsListboxNext) {
  RunEventTest(FILE_PATH_LITERAL("listbox-next.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListCollapse) {
  RunEventTest(FILE_PATH_LITERAL("menulist-collapse.html"));
}

// Mac: https://crbug.com/615411, Win: https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN_AND_MAC(AccessibilityEventsMenuListExpand)) {
  RunEventTest(FILE_PATH_LITERAL("menulist-expand.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsMenuListFocus) {
  RunEventTest(FILE_PATH_LITERAL("menulist-focus.html"));
}

// Mac: https://crbug.com/615411, Win: https://crbug.com/652706
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
    DISABLED_ON_WIN_AND_MAC(AccessibilityEventsMenuListNext)) {
  RunEventTest(FILE_PATH_LITERAL("menulist-next.html"));
}

// Flaky on Windows: http://crbug.com/486861
IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       DISABLED_AccessibilityEventsMenuListPopup) {
  RunEventTest(FILE_PATH_LITERAL("menulist-popup.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsNameChange) {
  RunEventTest(FILE_PATH_LITERAL("name-change.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveChild) {
  RunEventTest(FILE_PATH_LITERAL("remove-child.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveHiddenAttribute) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute.html"));
}

IN_PROC_BROWSER_TEST_F(
    DumpAccessibilityEventsTest,
    AccessibilityEventsRemoveHiddenAttributeSubtree) {
  RunEventTest(FILE_PATH_LITERAL("remove-hidden-attribute-subtree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsRemoveSubtree) {
  RunEventTest(FILE_PATH_LITERAL("remove-subtree.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityEventsTest,
                       AccessibilityEventsTextChanged) {
  RunEventTest(FILE_PATH_LITERAL("text-changed.html"));
}

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

}  // namespace content
