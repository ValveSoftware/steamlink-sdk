// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/styled_label.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

class StyledLabelTest : public ViewsTestBase, public StyledLabelListener {
 public:
  StyledLabelTest() {}
  ~StyledLabelTest() override {}

  // StyledLabelListener implementation.
  void StyledLabelLinkClicked(StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {}

 protected:
  StyledLabel* styled() { return styled_.get(); }

  void InitStyledLabel(const std::string& ascii_text) {
    styled_.reset(new StyledLabel(ASCIIToUTF16(ascii_text), this));
    styled_->set_owned_by_client();
  }

  int StyledLabelContentHeightForWidth(int w) {
    return styled_->GetHeightForWidth(w) - styled_->GetInsets().height();
  }

 private:
  std::unique_ptr<StyledLabel> styled_;

  DISALLOW_COPY_AND_ASSIGN(StyledLabelTest);
};

TEST_F(StyledLabelTest, NoWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text));
  const gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height(),
            StyledLabelContentHeightForWidth(label_preferred_size.width() * 2));
}

TEST_F(StyledLabelTest, TrailingWhitespaceiIgnored) {
  const std::string text("This is a test block of text   ");
  InitStyledLabel(text);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(1, styled()->child_count());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(0)->GetClassName());
  EXPECT_EQ(ASCIIToUTF16("This is a test block of text"),
            static_cast<Label*>(styled()->child_at(0))->text());
}

TEST_F(StyledLabelTest, RespectLeadingWhitespace) {
  const std::string text("   This is a test block of text");
  InitStyledLabel(text);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(1, styled()->child_count());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(0)->GetClassName());
  EXPECT_EQ(ASCIIToUTF16("   This is a test block of text"),
            static_cast<Label*>(styled()->child_at(0))->text());
}

TEST_F(StyledLabelTest, FirstLineNotEmptyWhenLeadingWhitespaceTooLong) {
  const std::string text("                                     a");
  InitStyledLabel(text);

  Label label(ASCIIToUTF16(text));
  gfx::Size label_preferred_size = label.GetPreferredSize();

  styled()->SetBounds(0, 0, label_preferred_size.width() / 2, 1000);
  styled()->Layout();

  ASSERT_EQ(1, styled()->child_count());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(0)->GetClassName());
  EXPECT_EQ(ASCIIToUTF16("a"),
            static_cast<Label*>(styled()->child_at(0))->text());
}

TEST_F(StyledLabelTest, BasicWrapping) {
  const std::string text("This is a test block of text");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height() * 2,
            StyledLabelContentHeightForWidth(label_preferred_size.width()));

  // Also respect the border.
  styled()->SetBorder(Border::CreateEmptyBorder(3, 3, 3, 3));
  styled()->SetBounds(
      0,
      0,
      styled()->GetInsets().width() + label_preferred_size.width(),
      styled()->GetInsets().height() + 2 * label_preferred_size.height());
  styled()->Layout();
  ASSERT_EQ(2, styled()->child_count());
  EXPECT_EQ(3, styled()->child_at(0)->x());
  EXPECT_EQ(3, styled()->child_at(0)->y());
  EXPECT_EQ(styled()->height() - 3, styled()->child_at(1)->bounds().bottom());
}

TEST_F(StyledLabelTest, WrapLongWords) {
  const std::string text("ThisIsTextAsASingleWord");
  InitStyledLabel(text);
  Label label(ASCIIToUTF16(text.substr(0, text.size() * 2 / 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  EXPECT_EQ(label_preferred_size.height() * 2,
            StyledLabelContentHeightForWidth(label_preferred_size.width()));

  styled()->SetBounds(
      0, 0, styled()->GetInsets().width() + label_preferred_size.width(),
      styled()->GetInsets().height() + 2 * label_preferred_size.height());
  styled()->Layout();

  ASSERT_EQ(2, styled()->child_count());
  ASSERT_EQ(gfx::Point(), styled()->bounds().origin());
  EXPECT_EQ(gfx::Point(), styled()->child_at(0)->bounds().origin());
  EXPECT_EQ(gfx::Point(0, styled()->height() / 2),
            styled()->child_at(1)->bounds().origin());

  EXPECT_FALSE(static_cast<Label*>(styled()->child_at(0))->text().empty());
  EXPECT_FALSE(static_cast<Label*>(styled()->child_at(1))->text().empty());
  EXPECT_EQ(ASCIIToUTF16(text),
            static_cast<Label*>(styled()->child_at(0))->text() +
                static_cast<Label*>(styled()->child_at(1))->text());
}

TEST_F(StyledLabelTest, CreateLinks) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);

  // Without links, there should be no focus border.
  EXPECT_TRUE(styled()->GetInsets().IsEmpty());

  // Now let's add some links.
  styled()->AddStyleRange(gfx::Range(0, 1),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(1, 2),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(10, 11),
                          StyledLabel::RangeStyleInfo::CreateForLink());
  styled()->AddStyleRange(gfx::Range(12, 13),
                          StyledLabel::RangeStyleInfo::CreateForLink());

  // Now there should be a focus border because there are non-empty Links.
  EXPECT_FALSE(styled()->GetInsets().IsEmpty());

  // Verify layout creates the right number of children.
  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();
  EXPECT_EQ(7, styled()->child_count());
}

TEST_F(StyledLabelTest, DontBreakLinks) {
  const std::string text("This is a test block of text, ");
  const std::string link_text("and this should be a link");
  InitStyledLabel(text + link_text);
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(text.size()),
                 static_cast<uint32_t>(text.size() + link_text.size())),
      StyledLabel::RangeStyleInfo::CreateForLink());

  Label label(ASCIIToUTF16(text + link_text.substr(0, link_text.size() / 2)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 2,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();
  ASSERT_EQ(2, styled()->child_count());
  // The label has no focus border while the link (and thus overall styled
  // label) does, so the label should be inset by the width of the focus border.
  EXPECT_EQ(Label::kFocusBorderPadding, styled()->child_at(0)->x());
  EXPECT_EQ(0, styled()->child_at(1)->x());
}

TEST_F(StyledLabelTest, StyledRangeWithDisabledLineWrapping) {
  const std::string text("This is a test block of text, ");
  const std::string unbreakable_text("and this should not be broken");
  InitStyledLabel(text + unbreakable_text);
  StyledLabel::RangeStyleInfo style_info;
  style_info.disable_line_wrapping = true;
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(text.size()),
                 static_cast<uint32_t>(text.size() + unbreakable_text.size())),
      style_info);

  Label label(ASCIIToUTF16(
      text + unbreakable_text.substr(0, unbreakable_text.size() / 2)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 2,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();
  ASSERT_EQ(2, styled()->child_count());
  EXPECT_EQ(0, styled()->child_at(0)->x());
  EXPECT_EQ(0, styled()->child_at(1)->x());
}

// TODO(mboc): Linux has never supported UNDERLINE, only virtually. Fix this.
#if defined(OS_LINUX)
#define MAYBE_StyledRangeUnderlined DISABLED_StyledRangeUnderlined
#else
#define MAYBE_StyledRangeUnderlined StyledRangeUnderlined
#endif
TEST_F(StyledLabelTest, MAYBE_StyledRangeUnderlined) {
  const std::string text("This is a test block of text, ");
  const std::string underlined_text("and this should be undelined");
  InitStyledLabel(text + underlined_text);
  StyledLabel::RangeStyleInfo style_info;
  style_info.font_style = gfx::Font::UNDERLINE;
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(text.size()),
                 static_cast<uint32_t>(text.size() + underlined_text.size())),
      style_info);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  ASSERT_EQ(2, styled()->child_count());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(1)->GetClassName());
  EXPECT_EQ(
      gfx::Font::UNDERLINE,
      static_cast<Label*>(styled()->child_at(1))->font_list().GetFontStyle());
}

// Fails on Mac, but only on 10.10. See http://crbug.com/622983.
#if defined(OS_MACOSX)
#define MAYBE_StyledRangeBold DISABLED_StyledRangeBold
#else
#define MAYBE_StyledRangeBold StyledRangeBold
#endif
TEST_F(StyledLabelTest, MAYBE_StyledRangeBold) {
  const std::string bold_text(
      "This is a block of text whose style will be set to BOLD in the test");
  const std::string text(" normal text");
  InitStyledLabel(bold_text + text);

  StyledLabel::RangeStyleInfo style_info;
  style_info.weight = gfx::Font::Weight::BOLD;
  styled()->AddStyleRange(
      gfx::Range(0u, static_cast<uint32_t>(bold_text.size())), style_info);

  // Calculate the bold text width if it were a pure label view, both with bold
  // and normal style.
  Label label(ASCIIToUTF16(bold_text));
  const gfx::Size normal_label_size = label.GetPreferredSize();
  label.SetFontList(
      label.font_list().DeriveWithWeight(gfx::Font::Weight::BOLD));
  const gfx::Size bold_label_size = label.GetPreferredSize();

  ASSERT_GE(bold_label_size.width(), normal_label_size.width());

  // Set the width so |bold_text| doesn't fit on a single line with bold style,
  // but does with normal font style.
  int styled_width = (normal_label_size.width() + bold_label_size.width()) / 2;
  int pref_height = styled()->GetHeightForWidth(styled_width);

  // Sanity check that |bold_text| with normal font style would fit on a single
  // line in a styled label with width |styled_width|.
  StyledLabel unstyled(ASCIIToUTF16(bold_text), this);
  unstyled.SetBounds(0, 0, styled_width, pref_height);
  unstyled.Layout();
  EXPECT_EQ(1, unstyled.child_count());

  styled()->SetBounds(0, 0, styled_width, pref_height);
  styled()->Layout();

  ASSERT_EQ(3, styled()->child_count());

  // The bold text should be broken up into two parts.
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(0)->GetClassName());
  EXPECT_EQ(
      gfx::Font::Weight::BOLD,
      static_cast<Label*>(styled()->child_at(0))->font_list().GetFontWeight());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(1)->GetClassName());
  EXPECT_EQ(
      gfx::Font::Weight::BOLD,
      static_cast<Label*>(styled()->child_at(1))->font_list().GetFontWeight());
  ASSERT_EQ(std::string(Label::kViewClassName),
            styled()->child_at(2)->GetClassName());
  EXPECT_EQ(
      gfx::Font::NORMAL,
      static_cast<Label*>(styled()->child_at(2))->font_list().GetFontStyle());

  // The second bold part should start on a new line.
  EXPECT_EQ(0, styled()->child_at(0)->x());
  EXPECT_EQ(0, styled()->child_at(1)->x());
  EXPECT_EQ(styled()->child_at(1)->bounds().right(),
            styled()->child_at(2)->x());
}

TEST_F(StyledLabelTest, Color) {
  const std::string text_red("RED");
  const std::string text_link("link");
  const std::string text("word");
  InitStyledLabel(text_red + text_link + text);

  StyledLabel::RangeStyleInfo style_info_red;
  style_info_red.color = SK_ColorRED;
  styled()->AddStyleRange(
      gfx::Range(0u, static_cast<uint32_t>(text_red.size())), style_info_red);

  StyledLabel::RangeStyleInfo style_info_link =
      StyledLabel::RangeStyleInfo::CreateForLink();
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(text_red.size()),
                 static_cast<uint32_t>(text_red.size() + text_link.size())),
      style_info_link);

  styled()->SetBounds(0, 0, 1000, 1000);
  styled()->Layout();

  Widget* widget = new Widget();
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* container = new View();
  widget->SetContentsView(container);
  container->AddChildView(styled());

  // Obtain the default text color for a label.
  Label* label = new Label(ASCIIToUTF16(text));
  container->AddChildView(label);
  const SkColor kDefaultTextColor = label->enabled_color();

  // Obtain the default text color for a link.
  Link* link = new Link(ASCIIToUTF16(text_link));
  container->AddChildView(link);
  const SkColor kDefaultLinkColor = link->enabled_color();

  EXPECT_EQ(SK_ColorRED,
            static_cast<Label*>(styled()->child_at(0))->enabled_color());
  EXPECT_EQ(kDefaultLinkColor,
            static_cast<Label*>(styled()->child_at(1))->enabled_color());
  EXPECT_EQ(kDefaultTextColor,
            static_cast<Label*>(styled()->child_at(2))->enabled_color());

  // Test adjusted color readability.
  styled()->SetDisplayedOnBackgroundColor(SK_ColorBLACK);
  styled()->Layout();
  label->SetBackgroundColor(SK_ColorBLACK);

  const SkColor kAdjustedTextColor = label->enabled_color();
  EXPECT_NE(kAdjustedTextColor, kDefaultTextColor);
  EXPECT_EQ(kAdjustedTextColor,
            static_cast<Label*>(styled()->child_at(2))->enabled_color());

  widget->CloseNow();
}

TEST_F(StyledLabelTest, StyledRangeWithTooltip) {
  const std::string text("This is a test block of text, ");
  const std::string tooltip_text("this should have a tooltip,");
  const std::string normal_text(" this should not have a tooltip, ");
  const std::string link_text("and this should be a link");

  const size_t tooltip_start = text.size();
  const size_t link_start =
      text.size() + tooltip_text.size() + normal_text.size();

  InitStyledLabel(text + tooltip_text + normal_text + link_text);
  StyledLabel::RangeStyleInfo tooltip_style;
  tooltip_style.tooltip = ASCIIToUTF16("tooltip");
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(tooltip_start),
                 static_cast<uint32_t>(tooltip_start + tooltip_text.size())),
      tooltip_style);
  styled()->AddStyleRange(
      gfx::Range(static_cast<uint32_t>(link_start),
                 static_cast<uint32_t>(link_start + link_text.size())),
      StyledLabel::RangeStyleInfo::CreateForLink());

  // Break line inside the range with the tooltip.
  Label label(ASCIIToUTF16(
       text + tooltip_text.substr(0, tooltip_text.size() - 3)));
  gfx::Size label_preferred_size = label.GetPreferredSize();
  int pref_height = styled()->GetHeightForWidth(label_preferred_size.width());
  EXPECT_EQ(label_preferred_size.height() * 3,
            pref_height - styled()->GetInsets().height());

  styled()->SetBounds(0, 0, label_preferred_size.width(), pref_height);
  styled()->Layout();

  EXPECT_EQ(label_preferred_size.width(), styled()->width());

  ASSERT_EQ(5, styled()->child_count());
  // The labels have no focus border while the link (and thus overall styled
  // label) does, so the labels should be inset by the width of the focus
  // border.
  EXPECT_EQ(Label::kFocusBorderPadding, styled()->child_at(0)->x());
  EXPECT_EQ(styled()->child_at(0)->bounds().right(),
            styled()->child_at(1)->x());
  EXPECT_EQ(Label::kFocusBorderPadding, styled()->child_at(2)->x());
  EXPECT_EQ(styled()->child_at(2)->bounds().right(),
            styled()->child_at(3)->x());
  EXPECT_EQ(0, styled()->child_at(4)->x());

  base::string16 tooltip;
  EXPECT_TRUE(
      styled()->child_at(1)->GetTooltipText(gfx::Point(1, 1), &tooltip));
  EXPECT_EQ(ASCIIToUTF16("tooltip"), tooltip);
  EXPECT_TRUE(
      styled()->child_at(2)->GetTooltipText(gfx::Point(1, 1), &tooltip));
  EXPECT_EQ(ASCIIToUTF16("tooltip"), tooltip);
}

TEST_F(StyledLabelTest, SetBaseFontList) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);
  std::string font_name("arial");
  gfx::Font font(font_name, 30);
  styled()->SetBaseFontList(gfx::FontList(font));
  Label label(ASCIIToUTF16(text), gfx::FontList(font));

  styled()->SetBounds(0,
                      0,
                      label.GetPreferredSize().width(),
                      label.GetPreferredSize().height());

  // Make sure we have the same sizing as a label.
  EXPECT_EQ(label.GetPreferredSize().height(), styled()->height());
  EXPECT_EQ(label.GetPreferredSize().width(), styled()->width());
}

TEST_F(StyledLabelTest, LineHeight) {
  const std::string text("one");
  InitStyledLabel(text);
  int default_height = styled()->GetHeightForWidth(100);
  const std::string newline_text("one\ntwo\nthree");
  InitStyledLabel(newline_text);
  styled()->SetLineHeight(18);
  EXPECT_EQ(18 * 2 + default_height, styled()->GetHeightForWidth(100));
}

TEST_F(StyledLabelTest, HandleEmptyLayout) {
  const std::string text("This is a test block of text.");
  InitStyledLabel(text);
  styled()->Layout();
  EXPECT_EQ(0, styled()->child_count());
}

TEST_F(StyledLabelTest, CacheSize) {
  const int preferred_height = 50;
  const int preferred_width = 100;
  const std::string text("This is a test block of text.");
  const base::string16 another_text(base::ASCIIToUTF16(
      "This is a test block of text. This text is much longer than previous"));

  InitStyledLabel(text);

  // we should be able to calculate height without any problem
  // no controls should be created
  int precalculated_height = styled()->GetHeightForWidth(preferred_width);
  EXPECT_LT(0, precalculated_height);
  EXPECT_EQ(0, styled()->child_count());

  styled()->SetBounds(0, 0, preferred_width, preferred_height);
  styled()->Layout();

  // controls should be created after layout
  // height should be the same as precalculated
  int real_height = styled()->GetHeightForWidth(styled()->width());
  View* first_child_after_layout = styled()->has_children() ?
      styled()->child_at(0) : nullptr;
  EXPECT_LT(0, styled()->child_count());
  EXPECT_LT(0, real_height);
  EXPECT_EQ(real_height, precalculated_height);

  // another call to Layout should not kill and recreate all controls
  styled()->Layout();
  View* first_child_after_second_layout = styled()->has_children() ?
      styled()->child_at(0) : nullptr;
  EXPECT_EQ(first_child_after_layout, first_child_after_second_layout);

  // if text is changed:
  // layout should be recalculated
  // all controls should be recreated
  styled()->SetText(another_text);
  int updated_height = styled()->GetHeightForWidth(styled()->width());
  EXPECT_NE(updated_height, real_height);
  View* first_child_after_text_update = styled()->has_children() ?
      styled()->child_at(0) : nullptr;
  EXPECT_NE(first_child_after_text_update, first_child_after_layout);
}

}  // namespace views
