// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager_test.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {

namespace {

int count = 1;

const int kTopCheckBoxID = count++;  // 1
const int kLeftContainerID = count++;
const int kAppleLabelID = count++;
const int kAppleTextfieldID = count++;
const int kOrangeLabelID = count++;  // 5
const int kOrangeTextfieldID = count++;
const int kBananaLabelID = count++;
const int kBananaTextfieldID = count++;
const int kKiwiLabelID = count++;
const int kKiwiTextfieldID = count++;  // 10
const int kFruitButtonID = count++;
const int kFruitCheckBoxID = count++;
const int kComboboxID = count++;

const int kRightContainerID = count++;
const int kAsparagusButtonID = count++;  // 15
const int kBroccoliButtonID = count++;
const int kCauliflowerButtonID = count++;

const int kInnerContainerID = count++;
const int kScrollViewID = count++;
const int kRosettaLinkID = count++;  // 20
const int kStupeurEtTremblementLinkID = count++;
const int kDinerGameLinkID = count++;
const int kRidiculeLinkID = count++;
const int kClosetLinkID = count++;
const int kVisitingLinkID = count++;  // 25
const int kAmelieLinkID = count++;
const int kJoyeuxNoelLinkID = count++;
const int kCampingLinkID = count++;
const int kBriceDeNiceLinkID = count++;
const int kTaxiLinkID = count++;  // 30
const int kAsterixLinkID = count++;

const int kOKButtonID = count++;
const int kCancelButtonID = count++;
const int kHelpButtonID = count++;

const int kStyleContainerID = count++;  // 35
const int kBoldCheckBoxID = count++;
const int kItalicCheckBoxID = count++;
const int kUnderlinedCheckBoxID = count++;
const int kStyleHelpLinkID = count++;
const int kStyleTextEditID = count++;  // 40

const int kSearchContainerID = count++;
const int kSearchTextfieldID = count++;
const int kSearchButtonID = count++;
const int kHelpLinkID = count++;

const int kThumbnailContainerID = count++;  // 45
const int kThumbnailStarID = count++;
const int kThumbnailSuperStarID = count++;

class DummyComboboxModel : public ui::ComboboxModel {
 public:
  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE { return 10; }
  virtual base::string16 GetItemAt(int index) OVERRIDE {
    return ASCIIToUTF16("Item ") + base::IntToString16(index);
  }
};

// A View that can act as a pane.
class PaneView : public View, public FocusTraversable {
 public:
  PaneView() : focus_search_(NULL) {}

  // If this method is called, this view will use GetPaneFocusTraversable to
  // have this provided FocusSearch used instead of the default one, allowing
  // you to trap focus within the pane.
  void EnablePaneFocus(FocusSearch* focus_search) {
    focus_search_ = focus_search;
  }

  // Overridden from View:
  virtual FocusTraversable* GetPaneFocusTraversable() OVERRIDE {
    if (focus_search_)
      return this;
    else
      return NULL;
  }

  // Overridden from FocusTraversable:
  virtual views::FocusSearch* GetFocusSearch() OVERRIDE {
    return focus_search_;
  }
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE {
    return NULL;
  }
  virtual View* GetFocusTraversableParentView() OVERRIDE {
    return NULL;
  }

 private:
  FocusSearch* focus_search_;
};

// BorderView is a view containing a native window with its own view hierarchy.
// It is interesting to test focus traversal from a view hierarchy to an inner
// view hierarchy.
class BorderView : public NativeViewHost {
 public:
  explicit BorderView(View* child) : child_(child), widget_(NULL) {
    DCHECK(child);
    SetFocusable(false);
  }

  virtual ~BorderView() {}

  virtual internal::RootView* GetContentsRootView() {
    return static_cast<internal::RootView*>(widget_->GetRootView());
  }

  virtual FocusTraversable* GetFocusTraversable() OVERRIDE {
    return static_cast<internal::RootView*>(widget_->GetRootView());
  }

  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    NativeViewHost::ViewHierarchyChanged(details);

    if (details.child == this && details.is_add) {
      if (!widget_) {
        widget_ = new Widget;
        Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
        params.parent = details.parent->GetWidget()->GetNativeView();
        widget_->Init(params);
        widget_->SetFocusTraversableParentView(this);
        widget_->SetContentsView(child_);
      }

      // We have been added to a view hierarchy, attach the native view.
      Attach(widget_->GetNativeView());
      // Also update the FocusTraversable parent so the focus traversal works.
      static_cast<internal::RootView*>(widget_->GetRootView())->
          SetFocusTraversableParent(GetWidget()->GetFocusTraversable());
    }
  }

 private:
  View* child_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(BorderView);
};

}  // namespace

class FocusTraversalTest : public FocusManagerTest {
 public:
  virtual ~FocusTraversalTest();

  virtual void InitContentView() OVERRIDE;

 protected:
  FocusTraversalTest();

  View* FindViewByID(int id) {
    View* view = GetContentsView()->GetViewByID(id);
    if (view)
      return view;
    if (style_tab_)
      view = style_tab_->GetSelectedTab()->GetViewByID(id);
    if (view)
      return view;
    view = search_border_view_->GetContentsRootView()->GetViewByID(id);
    if (view)
      return view;
    return NULL;
  }

 protected:
  TabbedPane* style_tab_;
  BorderView* search_border_view_;
  DummyComboboxModel combobox_model_;
  PaneView* left_container_;
  PaneView* right_container_;

  DISALLOW_COPY_AND_ASSIGN(FocusTraversalTest);
};

FocusTraversalTest::FocusTraversalTest()
    : style_tab_(NULL),
      search_border_view_(NULL) {
}

FocusTraversalTest::~FocusTraversalTest() {
}

void FocusTraversalTest::InitContentView() {
  // Create a complicated view hierarchy with lots of control types for
  // use by all of the focus traversal tests.
  //
  // Class name, ID, and asterisk next to focusable views:
  //
  // View
  //   Checkbox            * kTopCheckBoxID
  //   PaneView              kLeftContainerID
  //     Label               kAppleLabelID
  //     Textfield         * kAppleTextfieldID
  //     Label               kOrangeLabelID
  //     Textfield         * kOrangeTextfieldID
  //     Label               kBananaLabelID
  //     Textfield         * kBananaTextfieldID
  //     Label               kKiwiLabelID
  //     Textfield         * kKiwiTextfieldID
  //     NativeButton      * kFruitButtonID
  //     Checkbox          * kFruitCheckBoxID
  //     Combobox          * kComboboxID
  //   PaneView              kRightContainerID
  //     RadioButton       * kAsparagusButtonID
  //     RadioButton       * kBroccoliButtonID
  //     RadioButton       * kCauliflowerButtonID
  //     View                kInnerContainerID
  //       ScrollView        kScrollViewID
  //         View
  //           Link        * kRosettaLinkID
  //           Link        * kStupeurEtTremblementLinkID
  //           Link        * kDinerGameLinkID
  //           Link        * kRidiculeLinkID
  //           Link        * kClosetLinkID
  //           Link        * kVisitingLinkID
  //           Link        * kAmelieLinkID
  //           Link        * kJoyeuxNoelLinkID
  //           Link        * kCampingLinkID
  //           Link        * kBriceDeNiceLinkID
  //           Link        * kTaxiLinkID
  //           Link        * kAsterixLinkID
  //   NativeButton        * kOKButtonID
  //   NativeButton        * kCancelButtonID
  //   NativeButton        * kHelpButtonID
  //   TabbedPane          * kStyleContainerID
  //     View
  //       Checkbox        * kBoldCheckBoxID
  //       Checkbox        * kItalicCheckBoxID
  //       Checkbox        * kUnderlinedCheckBoxID
  //       Link            * kStyleHelpLinkID
  //       Textfield       * kStyleTextEditID
  //     Other
  //   BorderView            kSearchContainerID
  //     View
  //       Textfield       * kSearchTextfieldID
  //       NativeButton    * kSearchButtonID
  //       Link            * kHelpLinkID
  //   View                * kThumbnailContainerID
  //     NativeButton      * kThumbnailStarID
  //     NativeButton      * kThumbnailSuperStarID

  GetContentsView()->set_background(
      Background::CreateSolidBackground(SK_ColorWHITE));

  Checkbox* cb = new Checkbox(ASCIIToUTF16("This is a checkbox"));
  GetContentsView()->AddChildView(cb);
  // In this fast paced world, who really has time for non hard-coded layout?
  cb->SetBounds(10, 10, 200, 20);
  cb->set_id(kTopCheckBoxID);

  left_container_ = new PaneView();
  left_container_->SetBorder(Border::CreateSolidBorder(1, SK_ColorBLACK));
  left_container_->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  left_container_->set_id(kLeftContainerID);
  GetContentsView()->AddChildView(left_container_);
  left_container_->SetBounds(10, 35, 250, 200);

  int label_x = 5;
  int label_width = 50;
  int label_height = 15;
  int text_field_width = 150;
  int y = 10;
  int gap_between_labels = 10;

  Label* label = new Label(ASCIIToUTF16("Apple:"));
  label->set_id(kAppleLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  Textfield* text_field = new Textfield();
  text_field->set_id(kAppleTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(ASCIIToUTF16("Orange:"));
  label->set_id(kOrangeLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->set_id(kOrangeTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(ASCIIToUTF16("Banana:"));
  label->set_id(kBananaLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->set_id(kBananaTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  label = new Label(ASCIIToUTF16("Kiwi:"));
  label->set_id(kKiwiLabelID);
  left_container_->AddChildView(label);
  label->SetBounds(label_x, y, label_width, label_height);

  text_field = new Textfield();
  text_field->set_id(kKiwiTextfieldID);
  left_container_->AddChildView(text_field);
  text_field->SetBounds(label_x + label_width + 5, y,
                        text_field_width, label_height);

  y += label_height + gap_between_labels;

  LabelButton* button = new LabelButton(NULL, ASCIIToUTF16("Click me"));
  button->SetStyle(Button::STYLE_BUTTON);
  button->SetBounds(label_x, y + 10, 80, 30);
  button->set_id(kFruitButtonID);
  left_container_->AddChildView(button);
  y += 40;

  cb =  new Checkbox(ASCIIToUTF16("This is another check box"));
  cb->SetBounds(label_x + label_width + 5, y, 180, 20);
  cb->set_id(kFruitCheckBoxID);
  left_container_->AddChildView(cb);
  y += 20;

  Combobox* combobox =  new Combobox(&combobox_model_);
  combobox->SetBounds(label_x + label_width + 5, y, 150, 30);
  combobox->set_id(kComboboxID);
  left_container_->AddChildView(combobox);

  right_container_ = new PaneView();
  right_container_->SetBorder(Border::CreateSolidBorder(1, SK_ColorBLACK));
  right_container_->set_background(
      Background::CreateSolidBackground(240, 240, 240));
  right_container_->set_id(kRightContainerID);
  GetContentsView()->AddChildView(right_container_);
  right_container_->SetBounds(270, 35, 300, 200);

  y = 10;
  int radio_button_height = 18;
  int gap_between_radio_buttons = 10;
  RadioButton* radio_button = new RadioButton(ASCIIToUTF16("Asparagus"), 1);
  radio_button->set_id(kAsparagusButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(ASCIIToUTF16("Broccoli"), 1);
  radio_button->set_id(kBroccoliButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  RadioButton* radio_button_to_check = radio_button;
  y += radio_button_height + gap_between_radio_buttons;
  radio_button = new RadioButton(ASCIIToUTF16("Cauliflower"), 1);
  radio_button->set_id(kCauliflowerButtonID);
  right_container_->AddChildView(radio_button);
  radio_button->SetBounds(5, y, 70, radio_button_height);
  radio_button->SetGroup(1);
  y += radio_button_height + gap_between_radio_buttons;

  View* inner_container = new View();
  inner_container->SetBorder(Border::CreateSolidBorder(1, SK_ColorBLACK));
  inner_container->set_background(
      Background::CreateSolidBackground(230, 230, 230));
  inner_container->set_id(kInnerContainerID);
  right_container_->AddChildView(inner_container);
  inner_container->SetBounds(100, 10, 150, 180);

  ScrollView* scroll_view = new ScrollView();
  scroll_view->set_id(kScrollViewID);
  inner_container->AddChildView(scroll_view);
  scroll_view->SetBounds(1, 1, 148, 178);

  View* scroll_content = new View();
  scroll_content->SetBounds(0, 0, 200, 200);
  scroll_content->set_background(
      Background::CreateSolidBackground(200, 200, 200));
  scroll_view->SetContents(scroll_content);

  static const char* const kTitles[] = {
      "Rosetta", "Stupeur et tremblement", "The diner game",
      "Ridicule", "Le placard", "Les Visiteurs", "Amelie",
      "Joyeux Noel", "Camping", "Brice de Nice",
      "Taxi", "Asterix"
  };

  static const int kIDs[] = {
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kClosetLinkID, kVisitingLinkID, kAmelieLinkID,
      kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID
  };

  DCHECK(arraysize(kTitles) == arraysize(kIDs));

  y = 5;
  for (size_t i = 0; i < arraysize(kTitles); ++i) {
    Link* link = new Link(ASCIIToUTF16(kTitles[i]));
    link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    link->set_id(kIDs[i]);
    scroll_content->AddChildView(link);
    link->SetBounds(5, y, 300, 15);
    y += 15;
  }

  y = 250;
  int width = 60;
  button = new LabelButton(NULL, ASCIIToUTF16("OK"));
  button->SetStyle(Button::STYLE_BUTTON);
  button->set_id(kOKButtonID);
  button->SetIsDefault(true);

  GetContentsView()->AddChildView(button);
  button->SetBounds(150, y, width, 30);

  button = new LabelButton(NULL, ASCIIToUTF16("Cancel"));
  button->SetStyle(Button::STYLE_BUTTON);
  button->set_id(kCancelButtonID);
  GetContentsView()->AddChildView(button);
  button->SetBounds(220, y, width, 30);

  button = new LabelButton(NULL, ASCIIToUTF16("Help"));
  button->SetStyle(Button::STYLE_BUTTON);
  button->set_id(kHelpButtonID);
  GetContentsView()->AddChildView(button);
  button->SetBounds(290, y, width, 30);

  y += 40;

  View* contents = NULL;
  Link* link = NULL;

  // Left bottom box with style checkboxes.
  contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  cb = new Checkbox(ASCIIToUTF16("Bold"));
  contents->AddChildView(cb);
  cb->SetBounds(10, 10, 50, 20);
  cb->set_id(kBoldCheckBoxID);

  cb = new Checkbox(ASCIIToUTF16("Italic"));
  contents->AddChildView(cb);
  cb->SetBounds(70, 10, 50, 20);
  cb->set_id(kItalicCheckBoxID);

  cb = new Checkbox(ASCIIToUTF16("Underlined"));
  contents->AddChildView(cb);
  cb->SetBounds(130, 10, 70, 20);
  cb->set_id(kUnderlinedCheckBoxID);

  link = new Link(ASCIIToUTF16("Help"));
  contents->AddChildView(link);
  link->SetBounds(10, 35, 70, 10);
  link->set_id(kStyleHelpLinkID);

  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 50, 100, 20);
  text_field->set_id(kStyleTextEditID);

  style_tab_ = new TabbedPane();
  style_tab_->set_id(kStyleContainerID);
  GetContentsView()->AddChildView(style_tab_);
  style_tab_->SetBounds(10, y, 210, 100);
  style_tab_->AddTab(ASCIIToUTF16("Style"), contents);
  style_tab_->AddTab(ASCIIToUTF16("Other"), new View());

  // Right bottom box with search.
  contents = new View();
  contents->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  text_field = new Textfield();
  contents->AddChildView(text_field);
  text_field->SetBounds(10, 10, 100, 20);
  text_field->set_id(kSearchTextfieldID);

  button = new LabelButton(NULL, ASCIIToUTF16("Search"));
  button->SetStyle(Button::STYLE_BUTTON);
  contents->AddChildView(button);
  button->SetBounds(112, 5, 60, 30);
  button->set_id(kSearchButtonID);

  link = new Link(ASCIIToUTF16("Help"));
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->set_id(kHelpLinkID);
  contents->AddChildView(link);
  link->SetBounds(175, 10, 30, 20);

  search_border_view_ = new BorderView(contents);
  search_border_view_->set_id(kSearchContainerID);

  GetContentsView()->AddChildView(search_border_view_);
  search_border_view_->SetBounds(300, y, 240, 50);

  y += 60;

  contents = new View();
  contents->SetFocusable(true);
  contents->set_background(Background::CreateSolidBackground(SK_ColorBLUE));
  contents->set_id(kThumbnailContainerID);
  button = new LabelButton(NULL, ASCIIToUTF16("Star"));
  button->SetStyle(Button::STYLE_BUTTON);
  contents->AddChildView(button);
  button->SetBounds(5, 5, 50, 30);
  button->set_id(kThumbnailStarID);
  button = new LabelButton(NULL, ASCIIToUTF16("SuperStar"));
  button->SetStyle(Button::STYLE_BUTTON);
  contents->AddChildView(button);
  button->SetBounds(60, 5, 100, 30);
  button->set_id(kThumbnailSuperStarID);

  GetContentsView()->AddChildView(contents);
  contents->SetBounds(250, y, 200, 50);
  // We can only call RadioButton::SetChecked() on the radio-button is part of
  // the view hierarchy.
  radio_button_to_check->SetChecked(true);
}

TEST_F(FocusTraversalTest, NormalTraversal) {
  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kBananaTextfieldID, kKiwiTextfieldID,
      kFruitButtonID, kFruitCheckBoxID, kComboboxID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID,
      kDinerGameLinkID, kRidiculeLinkID, kClosetLinkID, kVisitingLinkID,
      kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kOKButtonID, kCancelButtonID, kHelpButtonID,
      kStyleContainerID, kBoldCheckBoxID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchTextfieldID, kSearchButtonID, kHelpLinkID,
      kThumbnailContainerID, kThumbnailStarID, kThumbnailSuperStarID };

  // Let's traverse the whole focus hierarchy (several times, to make sure it
  // loops OK).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }

  // Let's traverse in reverse order.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }
}

TEST_F(FocusTraversalTest, TraversalWithNonEnabledViews) {
  const int kDisabledIDs[] = {
      kBananaTextfieldID, kFruitCheckBoxID, kComboboxID, kAsparagusButtonID,
      kCauliflowerButtonID, kClosetLinkID, kVisitingLinkID, kBriceDeNiceLinkID,
      kTaxiLinkID, kAsterixLinkID, kHelpButtonID, kBoldCheckBoxID,
      kSearchTextfieldID, kHelpLinkID };

  const int kTraversalIDs[] = { kTopCheckBoxID,  kAppleTextfieldID,
      kOrangeTextfieldID, kKiwiTextfieldID, kFruitButtonID, kBroccoliButtonID,
      kRosettaLinkID, kStupeurEtTremblementLinkID, kDinerGameLinkID,
      kRidiculeLinkID, kAmelieLinkID, kJoyeuxNoelLinkID, kCampingLinkID,
      kOKButtonID, kCancelButtonID, kStyleContainerID, kItalicCheckBoxID,
      kUnderlinedCheckBoxID, kStyleHelpLinkID, kStyleTextEditID,
      kSearchButtonID, kThumbnailContainerID, kThumbnailStarID,
      kThumbnailSuperStarID };

  // Let's disable some views.
  for (size_t i = 0; i < arraysize(kDisabledIDs); i++) {
    View* v = FindViewByID(kDisabledIDs[i]);
    ASSERT_TRUE(v != NULL);
    v->SetEnabled(false);
  }

  View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }
}

TEST_F(FocusTraversalTest, TraversalWithInvisibleViews) {
  const int kInvisibleIDs[] = { kTopCheckBoxID, kOKButtonID,
      kThumbnailContainerID };

  const int kTraversalIDs[] = { kAppleTextfieldID, kOrangeTextfieldID,
      kBananaTextfieldID, kKiwiTextfieldID, kFruitButtonID, kFruitCheckBoxID,
      kComboboxID, kBroccoliButtonID, kRosettaLinkID,
      kStupeurEtTremblementLinkID, kDinerGameLinkID, kRidiculeLinkID,
      kClosetLinkID, kVisitingLinkID, kAmelieLinkID, kJoyeuxNoelLinkID,
      kCampingLinkID, kBriceDeNiceLinkID, kTaxiLinkID, kAsterixLinkID,
      kCancelButtonID, kHelpButtonID, kStyleContainerID, kBoldCheckBoxID,
      kItalicCheckBoxID, kUnderlinedCheckBoxID, kStyleHelpLinkID,
      kStyleTextEditID, kSearchTextfieldID, kSearchButtonID, kHelpLinkID };


  // Let's make some views invisible.
  for (size_t i = 0; i < arraysize(kInvisibleIDs); i++) {
    View* v = FindViewByID(kInvisibleIDs[i]);
    ASSERT_TRUE(v != NULL);
    v->SetVisible(false);
  }

  View* focused_view;
  // Let's do one traversal (several times, to make sure it loops ok).
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }

  // Same thing in reverse.
  GetFocusManager()->ClearFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kTraversalIDs[j], focused_view->id());
    }
  }
}

TEST_F(FocusTraversalTest, PaneTraversal) {
  // Tests trapping the traversal within a pane - useful for full
  // keyboard accessibility for toolbars.

  // First test the left container.
  const int kLeftTraversalIDs[] = {
    kAppleTextfieldID,
    kOrangeTextfieldID, kBananaTextfieldID, kKiwiTextfieldID,
    kFruitButtonID, kFruitCheckBoxID, kComboboxID };

  FocusSearch focus_search_left(left_container_, true, false);
  left_container_->EnablePaneFocus(&focus_search_left);
  FindViewByID(kComboboxID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kLeftTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kLeftTraversalIDs[j], focused_view->id());
    }
  }

  // Traverse in reverse order.
  FindViewByID(kAppleTextfieldID)->RequestFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kLeftTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kLeftTraversalIDs[j], focused_view->id());
    }
  }

  // Now test the right container, but this time with accessibility mode.
  // Make some links not focusable, but mark one of them as
  // "accessibility focusable", so it should show up in the traversal.
  const int kRightTraversalIDs[] = {
    kBroccoliButtonID, kDinerGameLinkID, kRidiculeLinkID,
    kClosetLinkID, kVisitingLinkID, kAmelieLinkID, kJoyeuxNoelLinkID,
    kCampingLinkID, kBriceDeNiceLinkID, kTaxiLinkID, kAsterixLinkID };

  FocusSearch focus_search_right(right_container_, true, true);
  right_container_->EnablePaneFocus(&focus_search_right);
  FindViewByID(kRosettaLinkID)->SetFocusable(false);
  FindViewByID(kStupeurEtTremblementLinkID)->SetFocusable(false);
  FindViewByID(kDinerGameLinkID)->SetAccessibilityFocusable(true);
  FindViewByID(kDinerGameLinkID)->SetFocusable(false);
  FindViewByID(kAsterixLinkID)->RequestFocus();

  // Traverse the focus hierarchy within the pane several times.
  for (int i = 0; i < 3; ++i) {
    for (size_t j = 0; j < arraysize(kRightTraversalIDs); j++) {
      GetFocusManager()->AdvanceFocus(false);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kRightTraversalIDs[j], focused_view->id());
    }
  }

  // Traverse in reverse order.
  FindViewByID(kBroccoliButtonID)->RequestFocus();
  for (int i = 0; i < 3; ++i) {
    for (int j = arraysize(kRightTraversalIDs) - 1; j >= 0; --j) {
      GetFocusManager()->AdvanceFocus(true);
      View* focused_view = GetFocusManager()->GetFocusedView();
      EXPECT_TRUE(focused_view != NULL);
      if (focused_view)
        EXPECT_EQ(kRightTraversalIDs[j], focused_view->id());
    }
  }
}

}  // namespace views
