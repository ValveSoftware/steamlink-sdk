// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(aboxhall): Create expectations on Android for these
#if defined(OS_ANDROID)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

namespace {

const char kCommentToken = '#';
const char kMarkSkipFile[] = "#<skip";
const char kMarkEndOfFile[] = "<-- End-of-file -->";
const char kSignalDiff[] = "*";

}  // namespace

typedef AccessibilityTreeFormatter::Filter Filter;

// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from chrome/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page and serialize the platform specific tree into a human
//    readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public ContentBrowserTest {
 public:
  // Utility helper that does a comment aware equality check.
  // Returns array of lines from expected file which are different.
  std::vector<int> DiffLines(const std::vector<std::string>& expected_lines,
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

  void AddDefaultFilters(std::vector<Filter>* filters) {
    filters->push_back(Filter(base::ASCIIToUTF16("FOCUSABLE"), Filter::ALLOW));
    filters->push_back(Filter(base::ASCIIToUTF16("READONLY"), Filter::ALLOW));
    filters->push_back(Filter(base::ASCIIToUTF16("*=''"), Filter::DENY));
  }

  // Parse the test html file and parse special directives, usually
  // beginning with an '@' and inside an HTML comment, that control how the
  // test is run and how the results are interpreted.
  //
  // When the accessibility tree is dumped as text, each attribute is
  // run through filters before being appended to the string. An "allow"
  // filter specifies attribute strings that should be dumped, and a "deny"
  // filter specifies strings that should be suppressed. As an example,
  // @MAC-ALLOW:AXSubrole=* means that the AXSubrole attribute should be
  // printed, while @MAC-ALLOW:AXSubrole=AXList* means that any subrole
  // beginning with the text "AXList" should be printed.
  //
  // The @WAIT-FOR:text directive allows the test to specify that the document
  // may dynamically change after initial load, and the test is to wait
  // until the given string (e.g., "text") appears in the resulting dump.
  // A test can make some changes to the document, then append a magic string
  // indicating that the test is done, and this framework will wait for that
  // string to appear before comparing the results.
  void ParseHtmlForExtraDirectives(const std::string& test_html,
                                   std::vector<Filter>* filters,
                                   std::string* wait_for) {
    std::vector<std::string> lines;
    base::SplitString(test_html, '\n', &lines);
    for (std::vector<std::string>::const_iterator iter = lines.begin();
         iter != lines.end();
         ++iter) {
      const std::string& line = *iter;
      const std::string& allow_empty_str =
          AccessibilityTreeFormatter::GetAllowEmptyString();
      const std::string& allow_str =
          AccessibilityTreeFormatter::GetAllowString();
      const std::string& deny_str =
          AccessibilityTreeFormatter::GetDenyString();
      const std::string& wait_str = "@WAIT-FOR:";
      if (StartsWithASCII(line, allow_empty_str, true)) {
        filters->push_back(
          Filter(base::UTF8ToUTF16(line.substr(allow_empty_str.size())),
                 Filter::ALLOW_EMPTY));
      } else if (StartsWithASCII(line, allow_str, true)) {
        filters->push_back(Filter(base::UTF8ToUTF16(
                                      line.substr(allow_str.size())),
                                  Filter::ALLOW));
      } else if (StartsWithASCII(line, deny_str, true)) {
        filters->push_back(Filter(base::UTF8ToUTF16(
                                      line.substr(deny_str.size())),
                                  Filter::DENY));
      } else if (StartsWithASCII(line, wait_str, true)) {
        *wait_for = line.substr(wait_str.size());
      }
    }
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // Enable <dialog>, which is used in some tests.
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void RunTest(const base::FilePath::CharType* file_path);
};

void DumpAccessibilityTreeTest::RunTest(
    const base::FilePath::CharType* file_path) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Setup test paths.
  base::FilePath dir_test_data;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &dir_test_data));
  base::FilePath test_path(
      dir_test_data.Append(FILE_PATH_LITERAL("accessibility")));
  ASSERT_TRUE(base::PathExists(test_path))
      << test_path.LossyDisplayName();

  base::FilePath html_file = test_path.Append(base::FilePath(file_path));
  // Output the test path to help anyone who encounters a failure and needs
  // to know where to look.
  printf("Testing: %s\n", html_file.MaybeAsASCII().c_str());

  std::string html_contents;
  base::ReadFileToString(html_file, &html_contents);

  // Read the expected file.
  std::string expected_contents_raw;
  base::FilePath expected_file =
    base::FilePath(html_file.RemoveExtension().value() +
                   AccessibilityTreeFormatter::GetExpectedFileSuffix());
  base::ReadFileToString(expected_file, &expected_contents_raw);

  // Tolerate Windows-style line endings (\r\n) in the expected file:
  // normalize by deleting all \r from the file (if any) to leave only \n.
  std::string expected_contents;
  base::RemoveChars(expected_contents_raw, "\r", &expected_contents);

  if (!expected_contents.compare(0, strlen(kMarkSkipFile), kMarkSkipFile)) {
    printf("Skipping this test on this platform.\n");
    return;
  }

  // Parse filters and other directives in the test file.
  std::vector<Filter> filters;
  std::string wait_for;
  AddDefaultFilters(&filters);
  ParseHtmlForExtraDirectives(html_contents, &filters, &wait_for);

  // Load the page.
  base::string16 html_contents16;
  html_contents16 = base::UTF8ToUTF16(html_contents);
  GURL url = GetTestUrl("accessibility",
                        html_file.BaseName().MaybeAsASCII().c_str());

  // If there's a @WAIT-FOR directive, set up an accessibility notification
  // waiter that returns on any event; we'll stop when we get the text we're
  // waiting for, or time out. Otherwise just wait specifically for
  // the "load complete" event.
  scoped_ptr<AccessibilityNotificationWaiter> waiter;
  if (!wait_for.empty()) {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_NONE));
  } else {
    waiter.reset(new AccessibilityNotificationWaiter(
        shell(), AccessibilityModeComplete, ui::AX_EVENT_LOAD_COMPLETE));
  }

  // Load the test html.
  NavigateToURL(shell(), url);

  // Wait for notifications. If there's a @WAIT-FOR directive, break when
  // the text we're waiting for appears in the dump, otherwise break after
  // the first notification, which will be a load complete.
  RenderWidgetHostViewBase* host_view = static_cast<RenderWidgetHostViewBase*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  std::string actual_contents;
  do {
    waiter->WaitForNotification();
    base::string16 actual_contents_utf16;
    AccessibilityTreeFormatter formatter(
        host_view->GetBrowserAccessibilityManager()->GetRoot());
    formatter.SetFilters(filters);
    formatter.FormatAccessibilityTree(&actual_contents_utf16);
    actual_contents = base::UTF16ToUTF8(actual_contents_utf16);
  } while (!wait_for.empty() &&
           actual_contents.find(wait_for) == std::string::npos);

  // Perform a diff (or write the initial baseline).
  std::vector<std::string> actual_lines, expected_lines;
  Tokenize(actual_contents, "\n", &actual_lines);
  Tokenize(expected_contents, "\n", &expected_lines);
  // Marking the end of the file with a line of text ensures that
  // file length differences are found.
  expected_lines.push_back(kMarkEndOfFile);
  actual_lines.push_back(kMarkEndOfFile);

  std::vector<int> diff_lines = DiffLines(expected_lines, actual_lines);
  bool is_different = diff_lines.size() > 0;
  EXPECT_FALSE(is_different);
  if (is_different) {
    // Mark the expected lines which did not match actual output with a *.
    printf("* Line Expected\n");
    printf("- ---- --------\n");
    for (int line = 0, diff_index = 0;
         line < static_cast<int>(expected_lines.size());
         ++line) {
      bool is_diff = false;
      if (diff_index < static_cast<int>(diff_lines.size()) &&
          diff_lines[diff_index] == line) {
        is_diff = true;
        ++diff_index;
      }
      printf("%1s %4d %s\n", is_diff? kSignalDiff : "", line + 1,
             expected_lines[line].c_str());
    }
    printf("\nActual\n");
    printf("------\n");
    printf("%s\n", actual_contents.c_str());
  }

  if (!base::PathExists(expected_file)) {
    base::FilePath actual_file =
        base::FilePath(html_file.RemoveExtension().value() +
                       AccessibilityTreeFormatter::GetActualFileSuffix());

    EXPECT_TRUE(base::WriteFile(
        actual_file, actual_contents.c_str(), actual_contents.size()));

    ADD_FAILURE() << "No expectation found. Create it by doing:\n"
                  << "mv " << actual_file.LossyDisplayName() << " "
                  << expected_file.LossyDisplayName();
  }
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityA) {
  RunTest(FILE_PATH_LITERAL("a.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAddress) {
  RunTest(FILE_PATH_LITERAL("address.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAName) {
  RunTest(FILE_PATH_LITERAL("a-name.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityANoText) {
  RunTest(FILE_PATH_LITERAL("a-no-text.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAOnclick) {
  RunTest(FILE_PATH_LITERAL("a-onclick.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaApplication) {
  RunTest(FILE_PATH_LITERAL("aria-application.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaAutocomplete) {
  RunTest(FILE_PATH_LITERAL("aria-autocomplete.html"));
}

// crbug.com/98976 will cause new elements to be added to the Blink a11y tree
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityAriaCombobox) {
  RunTest(FILE_PATH_LITERAL("aria-combobox.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       MAYBE(AccessibilityAriaFlowto)) {
  RunTest(FILE_PATH_LITERAL("aria-flowto.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaInvalid) {
  RunTest(FILE_PATH_LITERAL("aria-invalid.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaLabelledByHeading) {
  RunTest(FILE_PATH_LITERAL("aria-labelledby-heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaLevel) {
  RunTest(FILE_PATH_LITERAL("aria-level.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaList) {
  RunTest(FILE_PATH_LITERAL("aria-list.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAriaMenu) {
  RunTest(FILE_PATH_LITERAL("aria-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaMenuitemradio) {
  RunTest(FILE_PATH_LITERAL("aria-menuitemradio.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaPressed) {
  RunTest(FILE_PATH_LITERAL("aria-pressed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaProgressbar) {
  RunTest(FILE_PATH_LITERAL("aria-progressbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaToolbar) {
  RunTest(FILE_PATH_LITERAL("toolbar.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaValueMin) {
  RunTest(FILE_PATH_LITERAL("aria-valuemin.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaValueMax) {
  RunTest(FILE_PATH_LITERAL("aria-valuemax.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityArticle) {
  RunTest(FILE_PATH_LITERAL("article.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityAWithImg) {
  RunTest(FILE_PATH_LITERAL("a-with-img.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBdo) {
  RunTest(FILE_PATH_LITERAL("bdo.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityBR) {
  RunTest(FILE_PATH_LITERAL("br.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityButtonNameCalc) {
  RunTest(FILE_PATH_LITERAL("button-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityCanvas) {
  RunTest(FILE_PATH_LITERAL("canvas.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityCheckboxNameCalc) {
  RunTest(FILE_PATH_LITERAL("checkbox-name-calc.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDialog) {
  RunTest(FILE_PATH_LITERAL("dialog.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDiv) {
  RunTest(FILE_PATH_LITERAL("div.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityDl) {
  RunTest(FILE_PATH_LITERAL("dl.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityContenteditableDescendants) {
  RunTest(FILE_PATH_LITERAL("contenteditable-descendants.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityEm) {
  RunTest(FILE_PATH_LITERAL("em.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityFooter) {
  RunTest(FILE_PATH_LITERAL("footer.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityForm) {
  RunTest(FILE_PATH_LITERAL("form.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHeading) {
  RunTest(FILE_PATH_LITERAL("heading.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityHR) {
  RunTest(FILE_PATH_LITERAL("hr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityIframeCoordinates) {
  RunTest(FILE_PATH_LITERAL("iframe-coordinates.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputButton) {
  RunTest(FILE_PATH_LITERAL("input-button.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputColor) {
  RunTest(FILE_PATH_LITERAL("input-color.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputImageButtonInMenu) {
  RunTest(FILE_PATH_LITERAL("input-image-button-in-menu.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityInputRange) {
  RunTest(FILE_PATH_LITERAL("input-range.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityInputTextNameCalc) {
  RunTest(FILE_PATH_LITERAL("input-text-name-calc.html"));
}

// crbug.com/98976 will cause new elements to be added to the Blink a11y tree
// Re-baseline after the Blink change goes in
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       DISABLED_AccessibilityInputTypes) {
  RunTest(FILE_PATH_LITERAL("input-types.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLabel) {
  RunTest(FILE_PATH_LITERAL("label.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityLandmark) {
  RunTest(FILE_PATH_LITERAL("landmark.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityListMarkers) {
  RunTest(FILE_PATH_LITERAL("list-markers.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogClosed) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-closed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogOpened) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-opened.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeClosed) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-closed.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogInIframeOpened) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-in-iframe-opened.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityModalDialogStack) {
  RunTest(FILE_PATH_LITERAL("modal-dialog-stack.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityP) {
  RunTest(FILE_PATH_LITERAL("p.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityRegion) {
  RunTest(FILE_PATH_LITERAL("region.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySelect) {
  RunTest(FILE_PATH_LITERAL("select.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpan) {
  RunTest(FILE_PATH_LITERAL("span.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilitySpinButton) {
  RunTest(FILE_PATH_LITERAL("spinbutton.html"));
}

// TODO(dmazzoni): Rebaseline this test after Blink rolls past r155083.
// See http://crbug.com/265619
IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, DISABLED_AccessibilitySvg) {
  RunTest(FILE_PATH_LITERAL("svg.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTab) {
  RunTest(FILE_PATH_LITERAL("tab.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSimple) {
  RunTest(FILE_PATH_LITERAL("table-simple.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTableSpans) {
  RunTest(FILE_PATH_LITERAL("table-spans.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityTransition) {
  RunTest(FILE_PATH_LITERAL("transition.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityToggleButton) {
  RunTest(FILE_PATH_LITERAL("togglebutton.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityUl) {
  RunTest(FILE_PATH_LITERAL("ul.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest, AccessibilityWbr) {
  RunTest(FILE_PATH_LITERAL("wbr.html"));
}

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       AccessibilityAriaActivedescendant) {
  RunTest(FILE_PATH_LITERAL("aria-activedescendant.html"));
}

}  // namespace content
