// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/widget_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

class DialogExample : public DialogDelegateView {
 public:
  DialogExample();
  virtual ~DialogExample();
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual View* CreateExtraView() OVERRIDE;
  virtual View* CreateTitlebarExtraView() OVERRIDE;
  virtual View* CreateFootnoteView() OVERRIDE;
};

DialogExample::DialogExample() {
  set_background(Background::CreateSolidBackground(SK_ColorGRAY));
  SetLayoutManager(new BoxLayout(BoxLayout::kVertical, 10, 10, 10));
  AddChildView(new Label(ASCIIToUTF16("Dialog contents label!")));
}

DialogExample::~DialogExample() {}

base::string16 DialogExample::GetWindowTitle() const {
  return ASCIIToUTF16("Dialog Widget Example");
}

View* DialogExample::CreateExtraView() {
  LabelButton* button = new LabelButton(NULL, ASCIIToUTF16("Extra button!"));
  button->SetStyle(Button::STYLE_BUTTON);
  return button;
}

View* DialogExample::CreateTitlebarExtraView() {
  Label* label = new Label(ASCIIToUTF16("Extra view!"));
  label->SetEnabledColor(SK_ColorBLUE);
  return label;
}

View* DialogExample::CreateFootnoteView() {
  return new Label(ASCIIToUTF16("Footnote label!"));
}

}  // namespace

WidgetExample::WidgetExample() : ExampleBase("Widget") {
}

WidgetExample::~WidgetExample() {
}

void WidgetExample::CreateExampleView(View* container) {
  container->SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 10));
  BuildButton(container, "Popup widget", POPUP);
  BuildButton(container, "Dialog widget", DIALOG);
#if defined(OS_LINUX)
  // Windows does not support TYPE_CONTROL top-level widgets.
  BuildButton(container, "Child widget", CHILD);
#endif
}

void WidgetExample::BuildButton(View* container,
                                const std::string& label,
                                int tag) {
  LabelButton* button = new LabelButton(this, ASCIIToUTF16(label));
  button->SetFocusable(true);
  button->set_tag(tag);
  container->AddChildView(button);
}

void WidgetExample::ShowWidget(View* sender, Widget::InitParams params) {
  // Setup shared Widget heirarchy and bounds parameters.
  params.parent = sender->GetWidget()->GetNativeView();
  params.bounds = gfx::Rect(sender->GetBoundsInScreen().CenterPoint(),
                            gfx::Size(300, 200));

  Widget* widget = new Widget();
  widget->Init(params);

  // If the Widget has no contents by default, add a view with a 'Close' button.
  if (!widget->GetContentsView()) {
    View* contents = new View();
    contents->SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0));
    contents->set_background(Background::CreateSolidBackground(SK_ColorGRAY));
    BuildButton(contents, "Close", CLOSE_WIDGET);
    widget->SetContentsView(contents);
  }

  widget->Show();
}

void WidgetExample::ButtonPressed(Button* sender, const ui::Event& event) {
  switch (sender->tag()) {
    case POPUP:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_POPUP));
      break;
    case DIALOG: {
      DialogDelegate::CreateDialogWidget(new DialogExample(), NULL,
          sender->GetWidget()->GetNativeView())->Show();
      break;
    }
    case CHILD:
      ShowWidget(sender, Widget::InitParams(Widget::InitParams::TYPE_CONTROL));
      break;
    case CLOSE_WIDGET:
      sender->GetWidget()->Close();
      break;
  }
}

}  // namespace examples
}  // namespace views
