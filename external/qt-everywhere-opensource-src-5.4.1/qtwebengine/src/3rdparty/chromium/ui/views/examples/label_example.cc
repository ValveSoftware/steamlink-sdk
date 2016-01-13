// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/label_example.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace views {
namespace examples {

namespace {

// A Label with a constrained preferred size to demonstrate eliding or wrapping.
class PreferredSizeLabel : public Label {
 public:
  PreferredSizeLabel() : Label() {
    SetBorder(Border::CreateSolidBorder(2, SK_ColorCYAN));
  }
  virtual ~PreferredSizeLabel() {}

  // Label:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(100, 40);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferredSizeLabel);
};

}  // namespace

LabelExample::LabelExample() : ExampleBase("Label") {}

LabelExample::~LabelExample() {}

void LabelExample::CreateExampleView(View* container) {
  // A very simple label example, followed by additional helpful examples.
  container->SetLayoutManager(new BoxLayout(BoxLayout::kVertical, 0, 0, 10));
  Label* label = new Label(ASCIIToUTF16("Hello world!"));
  container->AddChildView(label);

  const wchar_t hello_world_hebrew[] =
      L"\x5e9\x5dc\x5d5\x5dd \x5d4\x5e2\x5d5\x5dc\x5dd!";
  label = new Label(WideToUTF16(hello_world_hebrew));
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(label);

  label = new Label(WideToUTF16(L"A UTF16 surrogate pair: \x5d0\x5b0"));
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  container->AddChildView(label);

  label = new Label(ASCIIToUTF16("A left-aligned blue label."));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(SK_ColorBLUE);
  container->AddChildView(label);

  label = new Label(ASCIIToUTF16("A Courier-18 label with shadows."));
  label->SetFontList(gfx::FontList("Courier, 18px"));
  gfx::ShadowValues shadows(1, gfx::ShadowValue(gfx::Point(), 1, SK_ColorRED));
  gfx::ShadowValue shadow(gfx::Point(2, 2), 0, SK_ColorGRAY);
  shadows.push_back(shadow);
  label->set_shadows(shadows);
  container->AddChildView(label);

  label = new PreferredSizeLabel();
  label->SetText(ASCIIToUTF16("A long label will elide toward its logical end "
      "if the text's width exceeds the label's available width."));
  container->AddChildView(label);

  label = new PreferredSizeLabel();
  label->SetElideBehavior(gfx::FADE_TAIL);
  label->SetText(ASCIIToUTF16("Some long labels will fade, rather than elide, "
      "if the text's width exceeds the label's available width."));
  container->AddChildView(label);

  label = new PreferredSizeLabel();
  label->SetText(ASCIIToUTF16("A multi-line label will wrap onto subsequent "
      "lines if the text's width exceeds the label's available width."));
  label->SetMultiLine(true);
  container->AddChildView(label);

  label = new Label(WideToUTF16(L"Password!"));
  label->SetObscured(true);
  container->AddChildView(label);
}

}  // namespace examples
}  // namespace views
