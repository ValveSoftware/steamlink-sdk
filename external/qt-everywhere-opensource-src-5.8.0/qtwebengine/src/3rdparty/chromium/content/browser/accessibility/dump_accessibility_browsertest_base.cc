// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

// Searches recursively and returns true if an accessibility node is found
// that represents a fully loaded web document with the given url.
bool AccessibilityTreeContainsLoadedDocWithUrl(BrowserAccessibility* node,
                                               const std::string& url) {
  if ((node->GetRole() == ui::AX_ROLE_WEB_AREA ||
       node->GetRole() == ui::AX_ROLE_ROOT_WEB_AREA) &&
      node->GetStringAttribute(ui::AX_ATTR_URL) == url) {
    // If possible, ensure the doc has finished loading. That's currently
    // not possible with same-process iframes until https://crbug.com/532249
    // is fixed.
    return (node->manager()->GetTreeData().url != url ||
            node->manager()->GetTreeData().loaded);
  }

  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsLoadedDocWithUrl(
            node->PlatformGetChild(i), url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

typedef AccessibilityTreeFormatter::Filter Filter;

DumpAccessibilityTestBase::DumpAccessibilityTestBase() {
}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {
}

void DumpAccessibilityTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);
}

void DumpAccessibilityTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  SetupCrossSiteRedirector(embedded_test_server());
}

base::string16
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(
      CreateAccessibilityTreeFormatter());
  std::vector<Filter> filters;
  filters.push_back(Filter(base::ASCIIToUTF16("*"), Filter::ALLOW));
  formatter->SetFilters(filters);
  formatter->set_show_ids(true);
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  base::string16 ax_tree_dump;
  formatter->FormatAccessibilityTree(
      web_contents->GetRootBrowserAccessibilityManager()->GetRoot(),
      &ax_tree_dump);
  return ax_tree_dump;
}

std::vector<int> DumpAccessibilityTestBase::DiffLines(
    const std::vector<std::string>& expected_lines,
    const std::vector<std::string>& actual_lines) {
  int actual_lines_count = actual_lines.size();
  int expected_lines_count = expected_lines.size();
  std::vector<int> diff_lines;
  int i = 0, j = 0;
  while (i < actual_lines_count && j < expected_lines_count) {
    if (expected_lines[j].size() == 0 ||
        expected_lines[j][0] == kCommentToken) {
      // Skip comment lines and blank lines in expected output.
      ++j;
      continue;
    }

    if (actual_lines[i] != expected_lines[j])
      diff_lines.push_back(j);
    ++i;
    ++j;
  }

  // Actual file has been fully checked.
  return diff_lines;
}

void DumpAccessibilityTestBase::ParseHtmlForExtraDirectives(
    const std::string& test_html,
    std::vector<Filter>* filters,
    std::vector<std::string>* wait_for) {
  for (const std::string& line :
       base::SplitString(test_html, "\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    const std::string& allow_empty_str = formatter_->GetAllowEmptyString();
    const std::string& allow_str = formatter_->GetAllowString();
    const std::string& deny_str = formatter_->GetDenyString();
    const std::string& wait_str = "@WAIT-FOR:";
    if (base::StartsWith(line, allow_empty_str,
                         base::CompareCase::SENSITIVE)) {
      filters->push_back(
          Filter(base::UTF8ToUTF16(line.substr(allow_empty_str.size())),
                 Filter::ALLOW_EMPTY));
    } else if (base::StartsWith(line, allow_str,
                                base::CompareCase::SENSITIVE)) {
      filters->push_back(Filter(base::UTF8ToUTF16(
          line.substr(allow_str.size())),
                                Filter::ALLOW));
    } else if (base::StartsWith(line, deny_str,
                                base::CompareCase::SENSITIVE)) {
      filters->push_back(Filter(base::UTF8ToUTF16(
          line.substr(deny_str.size())),
                                Filter::DENY));
    } else if (base::StartsWith(line, wait_str,
                                base::CompareCase::SENSITIVE)) {
      wait_for->push_back(line.substr(wait_str.size()));
    }
  }
}

AccessibilityTreeFormatter*
    DumpAccessibilityTestBase::CreateAccessibilityTreeFormatter() {
  if (is_blink_pass_)
    return new AccessibilityTreeFormatterBlink();
  else
    return AccessibilityTreeFormatter::Create();
}

void DumpAccessibilityTestBase::RunTest(
    const base::FilePath file_path, const char* file_dir) {
#if !defined(OS_ANDROID)
  // The blink tree is different on Android because we exclude inline
  // text boxes, for performance.
  is_blink_pass_ = true;
  RunTestForPlatform(file_path, file_dir);
#endif
  is_blink_pass_ = false;
  RunTestForPlatform(file_path, file_dir);
}

void DumpAccessibilityTestBase::RunTestForPlatform(
    const base::FilePath file_path, const char* file_dir) {
  formatter_.reset(CreateAccessibilityTreeFormatter());

  // Disable the "hot tracked" state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  BrowserAccessibilityStateImpl::GetInstance()->
      set_disable_hot_tracking_for_testing(true);

  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  LOG(INFO) << "Testing: " << file_path.LossyDisplayName()
            << (is_blink_pass_ ? " (internal Blink accessibility tree)"
                : " (native accessibility tree for this platform)");

  std::string html_contents;
  base::ReadFileToString(file_path, &html_contents);

  // Read the expected file.
  std::string expected_contents_raw;
  base::FilePath expected_file =
      base::FilePath(file_path.RemoveExtension().value() +
                     formatter_->GetExpectedFileSuffix());
  if (!base::PathExists(expected_file)) {
    LOG(INFO) << "File not found: " << expected_file.LossyDisplayName();
    LOG(INFO) << "No expectation file present, ignoring test on this platform."
              << " To run this test anyway, create "
              << expected_file.LossyDisplayName()
              << " (it can be empty) and then run content_browsertests "
              << "with the switch: --"
              << switches::kGenerateAccessibilityTestExpectations;
    return;
  }
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  // Parse filters and other directives in the test file.
  std::vector<std::string> wait_for;
  AddDefaultFilters(&filters_);
  ParseHtmlForExtraDirectives(html_contents, &filters_, &wait_for);

  // Load the test html and wait for the "load complete" AX event.
  GURL url(embedded_test_server()->GetURL(
      "/" + std::string(file_dir) + "/" + file_path.BaseName().MaybeAsASCII()));
  AccessibilityNotificationWaiter accessibility_waiter(
      shell()->web_contents(),
      AccessibilityModeComplete,
      ui::AX_EVENT_LOAD_COMPLETE);
  NavigateToURL(shell(), url);
  accessibility_waiter.WaitForNotification();

  // Get the url of every frame in the frame tree.
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  FrameTree* frame_tree = web_contents->GetFrameTree();
  std::vector<std::string> all_frame_urls;
  for (FrameTreeNode* node : frame_tree->Nodes()) {
    // Ignore about:blank urls because of the case where a parent frame A
    // has a child iframe B and it writes to the document using
    // contentDocument.open() on the child frame B.
    //
    // In this scenario, B's contentWindow.location.href matches A's url,
    // but B's url in the browser frame tree is still "about:blank".
    std::string url = node->current_url().spec();
    if (url != url::kAboutBlankURL)
      all_frame_urls.push_back(url);

    // We won't get the correct coordinate transformations for
    // out-of-process iframes until each frame's surface is ready.
    RenderFrameHostImpl* current_frame_host = node->current_frame_host();
    if (!current_frame_host || !current_frame_host->is_local_root())
      continue;
    RenderWidgetHostViewBase* rwhv =
        static_cast<RenderWidgetHostViewBase*>(current_frame_host->GetView());
    if (rwhv && rwhv->IsChildFrameForTesting()) {
      SurfaceHitTestReadyNotifier notifier(
          static_cast<RenderWidgetHostViewChildFrame*>(rwhv));
      notifier.WaitForSurfaceReady();
    }
  }

  // Wait for the accessibility tree to fully load for all frames,
  // by searching for the WEB_AREA node in the accessibility tree
  // with the url of each frame in our frame tree. Note that this
  // doesn't support cases where there are two iframes with the
  // exact same url. If all frames haven't loaded yet, set up a
  // listener for accessibility events on any frame and block
  // until the next one is received.
  //
  // If the original page has a @WAIT-FOR directive, don't break until
  // the text we're waiting for appears in the full text dump of the
  // accessibility tree, either.
  for (;;) {
    VLOG(1) << "Top of loop";
    RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
        web_contents->GetMainFrame());
    BrowserAccessibilityManager* manager =
        main_frame->browser_accessibility_manager();
    if (manager) {
      BrowserAccessibility* accessibility_root =
          manager->GetRoot();

      // Check to see if all frames have loaded.
      bool all_frames_loaded = true;
      for (const auto& url : all_frame_urls) {
        if (!AccessibilityTreeContainsLoadedDocWithUrl(
                accessibility_root, url)) {
          VLOG(1) << "Still waiting on this frame to load: " << url;
          all_frames_loaded = false;
          break;
        }
      }

      // Check to see if the @WAIT-FOR text has appeared yet.
      bool all_wait_for_strings_found = true;
      base::string16 tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      for (const auto& str : wait_for) {
        if (base::UTF16ToUTF8(tree_dump).find(str) == std::string::npos) {
          VLOG(1) << "Still waiting on this text to be found: " << str;
          all_wait_for_strings_found = false;
          break;
        }
      }

      // If all frames have loaded and the @WAIT-FOR text has appeared,
      // we're done.
      if (all_frames_loaded && all_wait_for_strings_found)
        break;
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(main_frame,
                                                         ui::AX_EVENT_NONE);
    for (FrameTreeNode* node : frame_tree->Nodes())
      accessibility_waiter.ListenToAdditionalFrame(node->current_frame_host());
    accessibility_waiter.WaitForNotification();
  }

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump();

  // Perform a diff (or write the initial baseline).
  std::vector<std::string> expected_lines = base::SplitString(
      expected_contents, "\n", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);
  std::string actual_contents = base::JoinString(actual_lines, "\n");

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    OnDiffFailed();

    std::string diff;

    // Mark the expected lines which did not match actual output with a *.
    diff += "* Line Expected\n";
    diff += "- ---- --------\n";
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size());
         ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      diff += base::StringPrintf(
          "%1s %4d %s\n", is_diff? kSignalDiff : "", line + 1,
             expected_lines[line].c_str());
    }
    diff += "\nActual\n";
    diff += "------\n";
    diff += actual_contents;
    LOG(ERROR) << "Diff:\n" << diff;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kGenerateAccessibilityTestExpectations)) {
      CHECK(base::WriteFile(
          expected_file, actual_contents.c_str(), actual_contents.size()));
      LOG(INFO) << "Wrote expectations to: "
                << expected_file.LossyDisplayName();
    }
  } else {
    LOG(INFO) << "Test output matches expectations.";
  }
}

}  // namespace content
