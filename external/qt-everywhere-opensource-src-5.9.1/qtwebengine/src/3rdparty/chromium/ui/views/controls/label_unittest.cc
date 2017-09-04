// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/label.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/switches.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(ASCIIToUTF16(ascii), utf16)

namespace views {

namespace {

#if defined(OS_MACOSX)
const int kControlCommandModifier = ui::EF_COMMAND_DOWN;
#else
const int kControlCommandModifier = ui::EF_CONTROL_DOWN;
#endif

// All text sizing measurements (width and height) should be greater than this.
const int kMinTextDimension = 4;

class TestLabel : public Label {
 public:
  TestLabel() : Label(ASCIIToUTF16("TestLabel")) { SizeToPreferredSize(); }

  int schedule_paint_count() const { return schedule_paint_count_; }

  void SimulatePaint() {
    gfx::Canvas canvas(bounds().size(), 1.0, false /* is_opaque */);
    Paint(ui::CanvasPainter(&canvas, 1.f).context());
  }

  // View:
  void SchedulePaintInRect(const gfx::Rect& r) override {
    ++schedule_paint_count_;
    Label::SchedulePaintInRect(r);
  }

 private:
  int schedule_paint_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestLabel);
};

// A test utility function to set the application default text direction.
void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  EXPECT_EQ(rtl, base::i18n::IsRTL());
}

// Returns true if |current| is bigger than |last|. Sets |last| to |current|.
bool Increased(int current, int* last) {
  bool increased = current > *last;
  *last = current;
  return increased;
}

base::string16 GetClipboardText(ui::ClipboardType clipboard_type) {
  base::string16 clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(clipboard_type,
                                                 &clipboard_text);
  return clipboard_text;
}

}  // namespace

class LabelTest : public ViewsTestBase {
 public:
  LabelTest() {}

  // ViewsTestBase overrides:
  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
    View* container = new View();
    widget_.SetContentsView(container);

    label_ = new Label();
    container->AddChildView(label_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  Label* label() { return label_; }

  Widget* widget() { return &widget_; }

 private:
  Label* label_ = nullptr;
  Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(LabelTest);
};

// Test fixture for text selection related tests.
class LabelSelectionTest : public LabelTest {
 public:
  LabelSelectionTest() {}

  // LabelTest overrides:
  void SetUp() override {
#if defined(OS_MACOSX)
    // On Mac, by default RenderTextMac is used for labels which does not
    // support text selection. Instead use RenderTextHarfBuzz for selection
    // related tests. TODO(crbug.com/661394): Remove this once Mac also uses
    // RenderTextHarfBuzz for Labels.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableHarfBuzzRenderText);
#endif
    LabelTest::SetUp();
    event_generator_ =
        base::MakeUnique<ui::test::EventGenerator>(widget()->GetNativeWindow());
  }

 protected:
  View* GetFocusedView() {
    return widget()->GetFocusManager()->GetFocusedView();
  }

  void PerformMousePress(const gfx::Point& point, int extra_flags = 0) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON | extra_flags);
    label()->OnMousePressed(pressed_event);
  }

  void PerformMouseRelease(const gfx::Point& point) {
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    label()->OnMouseReleased(released_event);
  }

  void PerformClick(const gfx::Point& point, int extra_flags = 0) {
    PerformMousePress(point, extra_flags);
    PerformMouseRelease(point);
  }

  void PerformMouseDragTo(const gfx::Point& point) {
    ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, point, point,
                        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    label()->OnMouseDragged(drag);
  }

  gfx::Point GetCursorPoint(int cursor_pos) {
    return label()
        ->GetRenderTextForSelectionController()
        ->GetCursorBounds(gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD),
                          false)
        .origin();
  }

  base::string16 GetSelectedText() { return label()->GetSelectedText(); }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  bool IsMenuCommandEnabled(int command_id) {
    return label()->IsCommandIdEnabled(command_id);
  }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(LabelSelectionTest);
};

// Crashes on Linux only. http://crbug.com/612406
#if defined(OS_LINUX)
#define MAYBE_FontPropertySymbol DISABLED_FontPropertySymbol
#else
#define MAYBE_FontPropertySymbol FontPropertySymbol
#endif
TEST_F(LabelTest, MAYBE_FontPropertySymbol) {
  std::string font_name("symbol");
  gfx::Font font(font_name, 26);
  label()->SetFontList(gfx::FontList(font));
  gfx::Font font_used = label()->font_list().GetPrimaryFont();
  EXPECT_EQ(font_name, font_used.GetFontName());
  EXPECT_EQ(26, font_used.GetFontSize());
}

TEST_F(LabelTest, FontPropertyArial) {
  std::string font_name("arial");
  gfx::Font font(font_name, 30);
  label()->SetFontList(gfx::FontList(font));
  gfx::Font font_used = label()->font_list().GetPrimaryFont();
  EXPECT_EQ(font_name, font_used.GetFontName());
  EXPECT_EQ(30, font_used.GetFontSize());
}

TEST_F(LabelTest, TextProperty) {
  base::string16 test_text(ASCIIToUTF16("A random string."));
  label()->SetText(test_text);
  EXPECT_EQ(test_text, label()->text());
}

TEST_F(LabelTest, ColorProperty) {
  SkColor color = SkColorSetARGB(20, 40, 10, 5);
  label()->SetAutoColorReadabilityEnabled(false);
  label()->SetEnabledColor(color);
  EXPECT_EQ(color, label()->enabled_color());
}

TEST_F(LabelTest, AlignmentProperty) {
  const bool was_rtl = base::i18n::IsRTL();

  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTL(!base::i18n::IsRTL());
    bool reverse_alignment = base::i18n::IsRTL();

    // The alignment should be flipped in RTL UI.
    label()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    EXPECT_EQ(reverse_alignment ? gfx::ALIGN_LEFT : gfx::ALIGN_RIGHT,
              label()->horizontal_alignment());
    label()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    EXPECT_EQ(reverse_alignment ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT,
              label()->horizontal_alignment());
    label()->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    EXPECT_EQ(gfx::ALIGN_CENTER, label()->horizontal_alignment());

    for (size_t j = 0; j < 2; ++j) {
      label()->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
      const bool rtl = j == 0;
      label()->SetText(rtl ? base::WideToUTF16(L"\x5d0") : ASCIIToUTF16("A"));
      EXPECT_EQ(gfx::ALIGN_TO_HEAD, label()->horizontal_alignment());
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}

TEST_F(LabelTest, ElideBehavior) {
  base::string16 text(ASCIIToUTF16("This is example text."));
  label()->SetText(text);
  EXPECT_EQ(gfx::ELIDE_TAIL, label()->elide_behavior());
  gfx::Size size = label()->GetPreferredSize();
  label()->SetBoundsRect(gfx::Rect(size));
  EXPECT_EQ(text, label()->GetDisplayTextForTesting());

  size.set_width(size.width() / 2);
  label()->SetBoundsRect(gfx::Rect(size));
  EXPECT_GT(text.size(), label()->GetDisplayTextForTesting().size());

  label()->SetElideBehavior(gfx::NO_ELIDE);
  EXPECT_EQ(text, label()->GetDisplayTextForTesting());
}

TEST_F(LabelTest, MultiLineProperty) {
  EXPECT_FALSE(label()->multi_line());
  label()->SetMultiLine(true);
  EXPECT_TRUE(label()->multi_line());
  label()->SetMultiLine(false);
  EXPECT_FALSE(label()->multi_line());
}

TEST_F(LabelTest, ObscuredProperty) {
  base::string16 test_text(ASCIIToUTF16("Password!"));
  label()->SetText(test_text);
  label()->SizeToPreferredSize();

  // The text should be unobscured by default.
  EXPECT_FALSE(label()->obscured());
  EXPECT_EQ(test_text, label()->GetDisplayTextForTesting());
  EXPECT_EQ(test_text, label()->text());

  label()->SetObscured(true);
  label()->SizeToPreferredSize();
  EXPECT_TRUE(label()->obscured());
  EXPECT_EQ(base::string16(test_text.size(),
                           gfx::RenderText::kPasswordReplacementChar),
            label()->GetDisplayTextForTesting());
  EXPECT_EQ(test_text, label()->text());

  label()->SetText(test_text + test_text);
  label()->SizeToPreferredSize();
  EXPECT_EQ(base::string16(test_text.size() * 2,
                           gfx::RenderText::kPasswordReplacementChar),
            label()->GetDisplayTextForTesting());
  EXPECT_EQ(test_text + test_text, label()->text());

  label()->SetObscured(false);
  label()->SizeToPreferredSize();
  EXPECT_FALSE(label()->obscured());
  EXPECT_EQ(test_text + test_text, label()->GetDisplayTextForTesting());
  EXPECT_EQ(test_text + test_text, label()->text());
}

TEST_F(LabelTest, ObscuredSurrogatePair) {
  // 'MUSICAL SYMBOL G CLEF': represented in UTF-16 as two characters
  // forming the surrogate pair 0x0001D11E.
  base::string16 test_text = base::UTF8ToUTF16("\xF0\x9D\x84\x9E");
  label()->SetText(test_text);
  label()->SetObscured(true);
  label()->SizeToPreferredSize();
  EXPECT_EQ(base::string16(1, gfx::RenderText::kPasswordReplacementChar),
            label()->GetDisplayTextForTesting());
  EXPECT_EQ(test_text, label()->text());
}

// This test case verifies the label preferred size will change based on the
// current layout, which may seem wrong. However many of our code base assumes
// this behavior, therefore this behavior will have to be kept until the code
// with this assumption is fixed. See http://crbug.com/468494 and
// http://crbug.com/467526.
// TODO(mukai): fix the code assuming this behavior and then fix Label
// implementation, and remove this test case.
TEST_F(LabelTest, MultilinePreferredSizeTest) {
  label()->SetText(ASCIIToUTF16("This is an example."));

  gfx::Size single_line_size = label()->GetPreferredSize();

  label()->SetMultiLine(true);
  gfx::Size multi_line_size = label()->GetPreferredSize();
  EXPECT_EQ(single_line_size, multi_line_size);

  int new_width = multi_line_size.width() / 2;
  label()->SetBounds(0, 0, new_width, label()->GetHeightForWidth(new_width));
  gfx::Size new_size = label()->GetPreferredSize();
  EXPECT_GT(multi_line_size.width(), new_size.width());
  EXPECT_LT(multi_line_size.height(), new_size.height());
}

TEST_F(LabelTest, TooltipProperty) {
  label()->SetText(ASCIIToUTF16("My cool string."));

  // Initially, label has no bounds, its text does not fit, and therefore its
  // text should be returned as the tooltip text.
  base::string16 tooltip;
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(label()->text(), tooltip);

  // While tooltip handling is disabled, GetTooltipText() should fail.
  label()->SetHandlesTooltips(false);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));
  label()->SetHandlesTooltips(true);

  // When set, custom tooltip text should be returned instead of the label's
  // text.
  base::string16 tooltip_text(ASCIIToUTF16("The tooltip!"));
  label()->SetTooltipText(tooltip_text);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);

  // While tooltip handling is disabled, GetTooltipText() should fail.
  label()->SetHandlesTooltips(false);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));
  label()->SetHandlesTooltips(true);

  // When the tooltip text is set to an empty string, the original behavior is
  // restored.
  label()->SetTooltipText(base::string16());
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(label()->text(), tooltip);

  // While tooltip handling is disabled, GetTooltipText() should fail.
  label()->SetHandlesTooltips(false);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));
  label()->SetHandlesTooltips(true);

  // Make the label big enough to hold the text
  // and expect there to be no tooltip.
  label()->SetBounds(0, 0, 1000, 40);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Shrinking the single-line label's height shouldn't trigger a tooltip.
  label()->SetBounds(0, 0, 1000, label()->GetPreferredSize().height() / 2);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Verify that explicitly set tooltip text is shown, regardless of size.
  label()->SetTooltipText(tooltip_text);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
  // Clear out the explicitly set tooltip text.
  label()->SetTooltipText(base::string16());

  // Shrink the bounds and the tooltip should come back.
  label()->SetBounds(0, 0, 10, 10);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Make the label obscured and there is no tooltip.
  label()->SetObscured(true);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Obscuring the text shouldn't permanently clobber the tooltip.
  label()->SetObscured(false);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Making the label multiline shouldn't eliminate the tooltip.
  label()->SetMultiLine(true);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  // Expanding the multiline label bounds should eliminate the tooltip.
  label()->SetBounds(0, 0, 1000, 1000);
  EXPECT_FALSE(label()->GetTooltipText(gfx::Point(), &tooltip));

  // Verify that setting the tooltip still shows it.
  label()->SetTooltipText(tooltip_text);
  EXPECT_TRUE(label()->GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
  // Clear out the tooltip.
  label()->SetTooltipText(base::string16());
}

TEST_F(LabelTest, Accessibility) {
  label()->SetText(ASCIIToUTF16("My special text."));

  ui::AXNodeData node_data;
  label()->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, node_data.role);
  EXPECT_EQ(label()->text(), node_data.GetString16Attribute(ui::AX_ATTR_NAME));
  EXPECT_TRUE(node_data.HasStateFlag(ui::AX_STATE_READ_ONLY));
}

TEST_F(LabelTest, TextChangeWithoutLayout) {
  label()->SetText(ASCIIToUTF16("Example"));
  label()->SetBounds(0, 0, 200, 200);

  gfx::Canvas canvas(gfx::Size(200, 200), 1.0f, true);
  label()->OnPaint(&canvas);
  EXPECT_EQ(1u, label()->lines_.size());
  EXPECT_EQ(ASCIIToUTF16("Example"), label()->lines_[0]->GetDisplayText());

  label()->SetText(ASCIIToUTF16("Altered"));
  // The altered text should be painted even though Layout() or SetBounds() are
  // not called.
  label()->OnPaint(&canvas);
  EXPECT_EQ(1u, label()->lines_.size());
  EXPECT_EQ(ASCIIToUTF16("Altered"), label()->lines_[0]->GetDisplayText());
}

TEST_F(LabelTest, EmptyLabelSizing) {
  const gfx::Size expected_size(0, label()->font_list().GetHeight());
  EXPECT_EQ(expected_size, label()->GetPreferredSize());
  label()->SetMultiLine(!label()->multi_line());
  EXPECT_EQ(expected_size, label()->GetPreferredSize());
}

TEST_F(LabelTest, SingleLineSizing) {
  label()->SetText(ASCIIToUTF16("A not so random string in one line."));
  const gfx::Size size = label()->GetPreferredSize();
  EXPECT_GT(size.height(), kMinTextDimension);
  EXPECT_GT(size.width(), kMinTextDimension);

  // Setting a size smaller than preferred should not change the preferred size.
  label()->SetSize(gfx::Size(size.width() / 2, size.height() / 2));
  EXPECT_EQ(size, label()->GetPreferredSize());

  const gfx::Insets border(10, 20, 30, 40);
  label()->SetBorder(CreateEmptyBorder(border.top(), border.left(),
                                       border.bottom(), border.right()));
  const gfx::Size size_with_border = label()->GetPreferredSize();
  EXPECT_EQ(size_with_border.height(), size.height() + border.height());
  EXPECT_EQ(size_with_border.width(), size.width() + border.width());
  EXPECT_EQ(size.height() + border.height(),
            label()->GetHeightForWidth(size_with_border.width()));
}

TEST_F(LabelTest, MultilineSmallAvailableWidthSizing) {
  label()->SetMultiLine(true);
  label()->SetAllowCharacterBreak(true);
  label()->SetText(ASCIIToUTF16("Too Wide."));

  // Check that Label can be laid out at a variety of small sizes,
  // splitting the words into up to one character per line if necessary.
  // Incorrect word splitting may cause infinite loops in text layout.
  gfx::Size required_size = label()->GetPreferredSize();
  for (int i = 1; i < required_size.width(); ++i)
    EXPECT_GT(label()->GetHeightForWidth(i), 0);
}

// Verifies if SetAllowCharacterBreak(true) doesn't change the preferred size.
// See crbug.com/469559
TEST_F(LabelTest, PreferredSizeForAllowCharacterBreak) {
  label()->SetText(base::ASCIIToUTF16("Example"));
  gfx::Size preferred_size = label()->GetPreferredSize();

  label()->SetMultiLine(true);
  label()->SetAllowCharacterBreak(true);
  EXPECT_EQ(preferred_size, label()->GetPreferredSize());
}

TEST_F(LabelTest, MultiLineSizing) {
  label()->SetText(
      ASCIIToUTF16("A random string\nwith multiple lines\nand returns!"));
  label()->SetMultiLine(true);

  // GetPreferredSize
  gfx::Size required_size = label()->GetPreferredSize();
  EXPECT_GT(required_size.height(), kMinTextDimension);
  EXPECT_GT(required_size.width(), kMinTextDimension);

  // SizeToFit with unlimited width.
  label()->SizeToFit(0);
  int required_width = label()->GetLocalBounds().width();
  EXPECT_GT(required_width, kMinTextDimension);

  // SizeToFit with limited width.
  label()->SizeToFit(required_width - 1);
  int constrained_width = label()->GetLocalBounds().width();
#if defined(OS_WIN)
  // Canvas::SizeStringInt (in ui/gfx/canvas_linux.cc)
  // has to be fixed to return the size that fits to given width/height.
  EXPECT_LT(constrained_width, required_width);
#endif
  EXPECT_GT(constrained_width, kMinTextDimension);

  // Change the width back to the desire width.
  label()->SizeToFit(required_width);
  EXPECT_EQ(required_width, label()->GetLocalBounds().width());

  // General tests for GetHeightForWidth.
  int required_height = label()->GetHeightForWidth(required_width);
  EXPECT_GT(required_height, kMinTextDimension);
  int height_for_constrained_width =
      label()->GetHeightForWidth(constrained_width);
#if defined(OS_WIN)
  // Canvas::SizeStringInt (in ui/gfx/canvas_linux.cc)
  // has to be fixed to return the size that fits to given width/height.
  EXPECT_GT(height_for_constrained_width, required_height);
#endif
  // Using the constrained width or the required_width - 1 should give the
  // same result for the height because the constrainted width is the tight
  // width when given "required_width - 1" as the max width.
  EXPECT_EQ(height_for_constrained_width,
            label()->GetHeightForWidth(required_width - 1));

  // Test everything with borders.
  gfx::Insets border(10, 20, 30, 40);
  label()->SetBorder(CreateEmptyBorder(border.top(), border.left(),
                                       border.bottom(), border.right()));

  // SizeToFit and borders.
  label()->SizeToFit(0);
  int required_width_with_border = label()->GetLocalBounds().width();
  EXPECT_EQ(required_width_with_border, required_width + border.width());

  // GetHeightForWidth and borders.
  int required_height_with_border =
      label()->GetHeightForWidth(required_width_with_border);
  EXPECT_EQ(required_height_with_border, required_height + border.height());

  // Test that the border width is subtracted before doing the height
  // calculation.  If it is, then the height will grow when width
  // is shrunk.
  int height1 = label()->GetHeightForWidth(required_width_with_border - 1);
#if defined(OS_WIN)
  // Canvas::SizeStringInt (in ui/gfx/canvas_linux.cc)
  // has to be fixed to return the size that fits to given width/height.
  EXPECT_GT(height1, required_height_with_border);
#endif
  EXPECT_EQ(height1, height_for_constrained_width + border.height());

  // GetPreferredSize and borders.
  label()->SetBounds(0, 0, 0, 0);
  gfx::Size required_size_with_border = label()->GetPreferredSize();
  EXPECT_EQ(required_size_with_border.height(),
            required_size.height() + border.height());
  EXPECT_EQ(required_size_with_border.width(),
            required_size.width() + border.width());
}

// Verifies if the combination of text eliding and multiline doesn't cause
// any side effects of size / layout calculation.
TEST_F(LabelTest, MultiLineSizingWithElide) {
  const base::string16 text =
      ASCIIToUTF16("A random string\nwith multiple lines\nand returns!");
  label()->SetText(text);
  label()->SetMultiLine(true);

  gfx::Size required_size = label()->GetPreferredSize();
  EXPECT_GT(required_size.height(), kMinTextDimension);
  EXPECT_GT(required_size.width(), kMinTextDimension);
  label()->SetBoundsRect(gfx::Rect(required_size));

  label()->SetElideBehavior(gfx::ELIDE_TAIL);
  EXPECT_EQ(required_size.ToString(), label()->GetPreferredSize().ToString());
  EXPECT_EQ(text, label()->GetDisplayTextForTesting());

  label()->SizeToFit(required_size.width() - 1);
  gfx::Size narrow_size = label()->GetPreferredSize();
  EXPECT_GT(required_size.width(), narrow_size.width());
  EXPECT_LT(required_size.height(), narrow_size.height());

  // SetBounds() doesn't change the preferred size.
  label()->SetBounds(0, 0, narrow_size.width() - 1, narrow_size.height());
  EXPECT_EQ(narrow_size.ToString(), label()->GetPreferredSize().ToString());

  // Paint() doesn't change the preferred size.
  gfx::Canvas canvas;
  label()->OnPaint(&canvas);
  EXPECT_EQ(narrow_size.ToString(), label()->GetPreferredSize().ToString());
}

// Check that labels support GetTooltipHandlerForPoint.
TEST_F(LabelTest, GetTooltipHandlerForPoint) {
  label()->SetText(
      ASCIIToUTF16("A string that's long enough to exceed the bounds"));
  label()->SetBounds(0, 0, 10, 10);

  // By default, labels start out as tooltip handlers.
  ASSERT_TRUE(label()->handles_tooltips());

  // There's a default tooltip if the text is too big to fit.
  EXPECT_EQ(label(), label()->GetTooltipHandlerForPoint(gfx::Point(2, 2)));

  // If tooltip handling is disabled, the label should not provide a tooltip
  // handler.
  label()->SetHandlesTooltips(false);
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(2, 2)));
  label()->SetHandlesTooltips(true);

  // If there's no default tooltip, this should return NULL.
  label()->SetBounds(0, 0, 500, 50);
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(2, 2)));

  label()->SetTooltipText(ASCIIToUTF16("a tooltip"));
  // If the point hits the label, and tooltip is set, the label should be
  // returned as its tooltip handler.
  EXPECT_EQ(label(), label()->GetTooltipHandlerForPoint(gfx::Point(2, 2)));

  // Additionally, GetTooltipHandlerForPoint should verify that the label
  // actually contains the point.
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(2, 51)));
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(-1, 20)));

  // Again, if tooltip handling is disabled, the label should not provide a
  // tooltip handler.
  label()->SetHandlesTooltips(false);
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(2, 2)));
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(2, 51)));
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(-1, 20)));
  label()->SetHandlesTooltips(true);

  // GetTooltipHandlerForPoint works should work in child bounds.
  label()->SetBounds(2, 2, 10, 10);
  EXPECT_EQ(label(), label()->GetTooltipHandlerForPoint(gfx::Point(1, 5)));
  EXPECT_FALSE(label()->GetTooltipHandlerForPoint(gfx::Point(3, 11)));
}

// Check that label releases its internal layout data when it's unnecessary.
TEST_F(LabelTest, ResetRenderTextData) {
  label()->SetText(ASCIIToUTF16("Example"));
  label()->SizeToPreferredSize();
  gfx::Size preferred_size = label()->GetPreferredSize();

  EXPECT_NE(gfx::Size().ToString(), preferred_size.ToString());
  EXPECT_EQ(0u, label()->lines_.size());

  gfx::Canvas canvas(preferred_size, 1.0f, true);
  label()->OnPaint(&canvas);
  EXPECT_EQ(1u, label()->lines_.size());

  // Label should recreate its RenderText object when it's invisible, to release
  // the layout structures and data.
  label()->SetVisible(false);
  EXPECT_EQ(0u, label()->lines_.size());

  // Querying fields or size information should not recompute the layout
  // unnecessarily.
  EXPECT_EQ(ASCIIToUTF16("Example"), label()->text());
  EXPECT_EQ(0u, label()->lines_.size());

  EXPECT_EQ(preferred_size.ToString(), label()->GetPreferredSize().ToString());
  EXPECT_EQ(0u, label()->lines_.size());

  // RenderText data should be back when it's necessary.
  label()->SetVisible(true);
  EXPECT_EQ(0u, label()->lines_.size());

  label()->OnPaint(&canvas);
  EXPECT_EQ(1u, label()->lines_.size());

  // Changing layout just resets |lines_|. It'll recover next time it's drawn.
  label()->SetBounds(0, 0, 10, 10);
  EXPECT_EQ(0u, label()->lines_.size());

  label()->OnPaint(&canvas);
  EXPECT_EQ(1u, label()->lines_.size());
}

#if !defined(OS_MACOSX)
TEST_F(LabelTest, MultilineSupportedRenderText) {
  std::unique_ptr<gfx::RenderText> render_text(
      gfx::RenderText::CreateInstance());
  ASSERT_TRUE(render_text->MultilineSupported());

  label()->SetText(ASCIIToUTF16("Example of\nmultilined label"));
  label()->SetMultiLine(true);
  label()->SizeToPreferredSize();

  gfx::Canvas canvas(label()->GetPreferredSize(), 1.0f, true);
  label()->OnPaint(&canvas);

  // There's only one 'line', RenderText itself supports multiple lines.
  EXPECT_EQ(1u, label()->lines_.size());
}
#endif

// Ensures SchedulePaint() calls are not made in OnPaint().
TEST_F(LabelTest, NoSchedulePaintInOnPaint) {
  TestLabel label;

  // Initialization should schedule at least one paint, but the precise number
  // doesn't really matter.
  int count = label.schedule_paint_count();
  EXPECT_LT(0, count);

  // Painting should never schedule another paint.
  label.SimulatePaint();
  EXPECT_EQ(count, label.schedule_paint_count());

  // Test a few things that should schedule paints. Multiple times is OK.
  label.SetEnabled(false);
  EXPECT_TRUE(Increased(label.schedule_paint_count(), &count));

  label.SetText(label.text() + ASCIIToUTF16("Changed"));
  EXPECT_TRUE(Increased(label.schedule_paint_count(), &count));

  label.SizeToPreferredSize();
  EXPECT_TRUE(Increased(label.schedule_paint_count(), &count));

  label.SetEnabledColor(SK_ColorBLUE);
  EXPECT_TRUE(Increased(label.schedule_paint_count(), &count));

  label.SimulatePaint();
  EXPECT_EQ(count, label.schedule_paint_count());  // Unchanged.
}

TEST_F(LabelTest, FocusBounds) {
  label()->SetText(ASCIIToUTF16("Example"));
  gfx::Size normal_size = label()->GetPreferredSize();

  label()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  label()->RequestFocus();
  gfx::Size focusable_size = label()->GetPreferredSize();
  // Focusable label requires larger size to paint the focus rectangle.
  EXPECT_GT(focusable_size.width(), normal_size.width());
  EXPECT_GT(focusable_size.height(), normal_size.height());

  label()->SizeToPreferredSize();
  gfx::Rect focus_bounds = label()->GetFocusBounds();
  EXPECT_EQ(label()->GetLocalBounds().ToString(), focus_bounds.ToString());

  label()->SetBounds(
      0, 0, focusable_size.width() * 2, focusable_size.height() * 2);
  label()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  focus_bounds = label()->GetFocusBounds();
  EXPECT_EQ(0, focus_bounds.x());
  EXPECT_LT(0, focus_bounds.y());
  EXPECT_GT(label()->bounds().bottom(), focus_bounds.bottom());
  EXPECT_EQ(focusable_size.ToString(), focus_bounds.size().ToString());

  label()->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  focus_bounds = label()->GetFocusBounds();
  EXPECT_LT(0, focus_bounds.x());
  EXPECT_EQ(label()->bounds().right(), focus_bounds.right());
  EXPECT_LT(0, focus_bounds.y());
  EXPECT_GT(label()->bounds().bottom(), focus_bounds.bottom());
  EXPECT_EQ(focusable_size.ToString(), focus_bounds.size().ToString());

  label()->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label()->SetElideBehavior(gfx::FADE_TAIL);
  label()->SetBounds(0, 0, focusable_size.width() / 2, focusable_size.height());
  focus_bounds = label()->GetFocusBounds();
  EXPECT_EQ(0, focus_bounds.x());
  EXPECT_EQ(focusable_size.width() / 2, focus_bounds.width());
}

TEST_F(LabelTest, EmptyLabel) {
  label()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  label()->RequestFocus();
  label()->SizeToPreferredSize();

  gfx::Rect focus_bounds = label()->GetFocusBounds();
  EXPECT_FALSE(focus_bounds.IsEmpty());
  EXPECT_LT(label()->font_list().GetHeight(), focus_bounds.height());
}

TEST_F(LabelSelectionTest, Selectable) {
  // By default, labels don't support text selection.
  EXPECT_FALSE(label()->selectable());

  ASSERT_TRUE(label()->SetSelectable(true));
  EXPECT_TRUE(label()->selectable());

  // Verify that making a label multiline causes the label to not support text
  // selection.
  label()->SetMultiLine(true);
  EXPECT_FALSE(label()->selectable());

  label()->SetMultiLine(false);
  ASSERT_TRUE(label()->SetSelectable(true));
  EXPECT_TRUE(label()->selectable());

  // Verify that obscuring the label text causes the label to not support text
  // selection.
  label()->SetObscured(true);
  EXPECT_FALSE(label()->selectable());
}

// Verify that labels supporting text selection get focus on clicks.
TEST_F(LabelSelectionTest, FocusOnClick) {
  label()->SetText(ASCIIToUTF16("text"));
  label()->SizeToPreferredSize();

  // By default, labels don't get focus on click.
  PerformClick(gfx::Point());
  EXPECT_NE(label(), GetFocusedView());

  ASSERT_TRUE(label()->SetSelectable(true));
  PerformClick(gfx::Point());
  EXPECT_EQ(label(), GetFocusedView());
}

// Verify that labels supporting text selection do not get focus on tab
// traversal by default.
TEST_F(LabelSelectionTest, FocusTraversal) {
  // Add another view before |label()|.
  View* view = new View();
  view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget()->GetContentsView()->AddChildViewAt(view, 0);

  // By default, labels are not focusable.
  view->RequestFocus();
  EXPECT_EQ(view, GetFocusedView());
  widget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_NE(label(), GetFocusedView());

  // On enabling text selection, labels can get focus on clicks but not via tab
  // traversal.
  view->RequestFocus();
  EXPECT_EQ(view, GetFocusedView());
  EXPECT_TRUE(label()->SetSelectable(true));
  widget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_NE(label(), GetFocusedView());

  // A label with FocusBehavior::ALWAYS should get focus via tab traversal.
  view->RequestFocus();
  EXPECT_EQ(view, GetFocusedView());
  EXPECT_TRUE(label()->SetSelectable(false));
  label()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget()->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(label(), GetFocusedView());
}

// Verify label text selection behavior on double and triple clicks.
TEST_F(LabelSelectionTest, DoubleTripleClick) {
  label()->SetText(ASCIIToUTF16("Label double click"));
  label()->SizeToPreferredSize();
  ASSERT_TRUE(label()->SetSelectable(true));

  PerformClick(gfx::Point());
  EXPECT_TRUE(GetSelectedText().empty());

  // Double clicking should select the word under cursor.
  PerformClick(gfx::Point(), ui::EF_IS_DOUBLE_CLICK);
  EXPECT_STR_EQ("Label", GetSelectedText());

  // Triple clicking should select all the text.
  PerformClick(gfx::Point());
  EXPECT_EQ(label()->text(), GetSelectedText());

  // Clicking again should alternate to double click.
  PerformClick(gfx::Point());
  EXPECT_STR_EQ("Label", GetSelectedText());

  // Clicking at another location should clear the selection.
  PerformClick(GetCursorPoint(8));
  EXPECT_TRUE(GetSelectedText().empty());
  PerformClick(GetCursorPoint(8), ui::EF_IS_DOUBLE_CLICK);
  EXPECT_STR_EQ("double", GetSelectedText());
}

// Verify label text selection behavior on mouse drag.
TEST_F(LabelSelectionTest, MouseDrag) {
  label()->SetText(ASCIIToUTF16("Label mouse drag"));
  label()->SizeToPreferredSize();
  ASSERT_TRUE(label()->SetSelectable(true));

  PerformMousePress(GetCursorPoint(5));
  PerformMouseDragTo(gfx::Point());
  EXPECT_STR_EQ("Label", GetSelectedText());

  PerformMouseDragTo(GetCursorPoint(8));
  EXPECT_STR_EQ(" mo", GetSelectedText());

  PerformMouseDragTo(gfx::Point(200, 0));
  PerformMouseRelease(gfx::Point(200, 0));
  EXPECT_STR_EQ(" mouse drag", GetSelectedText());

  event_generator()->PressKey(ui::VKEY_C, kControlCommandModifier);
  EXPECT_STR_EQ(" mouse drag", GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));
}

// Verify the initially selected word on a double click, remains selected on
// mouse dragging.
TEST_F(LabelSelectionTest, MouseDragWord) {
  label()->SetText(ASCIIToUTF16("Label drag word"));
  label()->SizeToPreferredSize();
  ASSERT_TRUE(label()->SetSelectable(true));

  PerformClick(GetCursorPoint(8));
  PerformMousePress(GetCursorPoint(8), ui::EF_IS_DOUBLE_CLICK);
  EXPECT_STR_EQ("drag", GetSelectedText());

  PerformMouseDragTo(gfx::Point());
  EXPECT_STR_EQ("Label drag", GetSelectedText());

  PerformMouseDragTo(gfx::Point(200, 0));
  PerformMouseRelease(gfx::Point(200, 0));
  EXPECT_STR_EQ("drag word", GetSelectedText());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Verify selection clipboard behavior on text selection.
TEST_F(LabelSelectionTest, SelectionClipboard) {
  label()->SetText(ASCIIToUTF16("Label selection clipboard"));
  label()->SizeToPreferredSize();
  ASSERT_TRUE(label()->SetSelectable(true));

  // Verify programmatic modification of selection, does not modify the
  // selection clipboard.
  label()->SelectRange(gfx::Range(2, 5));
  EXPECT_STR_EQ("bel", GetSelectedText());
  EXPECT_TRUE(GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION).empty());

  // Verify text selection using the mouse updates the selection clipboard.
  PerformMousePress(GetCursorPoint(5));
  PerformMouseDragTo(gfx::Point());
  PerformMouseRelease(gfx::Point());
  EXPECT_STR_EQ("Label", GetSelectedText());
  EXPECT_STR_EQ("Label", GetClipboardText(ui::CLIPBOARD_TYPE_SELECTION));
}
#endif

// Verify that keyboard shortcuts for Copy and Select All work when a selectable
// label is focused.
TEST_F(LabelSelectionTest, KeyboardActions) {
  const base::string16 initial_text = ASCIIToUTF16("Label keyboard actions");
  label()->SetText(initial_text);
  label()->SizeToPreferredSize();
  ASSERT_TRUE(label()->SetSelectable(true));

  PerformClick(gfx::Point());
  EXPECT_EQ(label(), GetFocusedView());

  event_generator()->PressKey(ui::VKEY_A, kControlCommandModifier);
  EXPECT_EQ(initial_text, GetSelectedText());

  event_generator()->PressKey(ui::VKEY_C, kControlCommandModifier);
  EXPECT_EQ(initial_text, GetClipboardText(ui::CLIPBOARD_TYPE_COPY_PASTE));

  // The selection should get cleared on changing the text, but focus should not
  // be affected.
  const base::string16 new_text = ASCIIToUTF16("Label obscured text");
  label()->SetText(new_text);
  EXPECT_FALSE(label()->HasSelection());
  EXPECT_EQ(label(), GetFocusedView());

  // Obscured labels do not support text selection.
  label()->SetObscured(true);
  EXPECT_FALSE(label()->selectable());
  event_generator()->PressKey(ui::VKEY_A, kControlCommandModifier);
  EXPECT_EQ(base::string16(), GetSelectedText());
}

// Verify the context menu options are enabled and disabled appropriately.
TEST_F(LabelSelectionTest, ContextMenuContents) {
  label()->SetText(ASCIIToUTF16("Label context menu"));
  label()->SizeToPreferredSize();

  // A non-selectable label would not show a context menu and both COPY and
  // SELECT_ALL context menu items should be disabled for it.
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_COPY));
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_SELECT_ALL));

  // For a selectable label with no selection, only SELECT_ALL should be
  // enabled.
  ASSERT_TRUE(label()->SetSelectable(true));
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_COPY));
  EXPECT_TRUE(IsMenuCommandEnabled(IDS_APP_SELECT_ALL));

  // For a selectable label with a selection, both COPY and SELECT_ALL should be
  // enabled.
  label()->SelectRange(gfx::Range(0, 4));
  EXPECT_TRUE(IsMenuCommandEnabled(IDS_APP_COPY));
  EXPECT_TRUE(IsMenuCommandEnabled(IDS_APP_SELECT_ALL));
  // Ensure unsupported commands like PASTE are not enabled.
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_PASTE));

  // An obscured label would not show a context menu and both COPY and
  // SELECT_ALL should be disabled for it.
  label()->SetObscured(true);
  EXPECT_FALSE(label()->selectable());
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_COPY));
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_SELECT_ALL));
  label()->SetObscured(false);

  // For an empty label, both COPY and SELECT_ALL should be disabled.
  label()->SetText(base::string16());
  ASSERT_TRUE(label()->SetSelectable(true));
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_COPY));
  EXPECT_FALSE(IsMenuCommandEnabled(IDS_APP_SELECT_ALL));
}

}  // namespace views
