// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/vector_example.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace views {
namespace examples {

namespace {

class VectorIconGallery : public View,
                          public TextfieldController,
                          public ButtonListener {
 public:
  VectorIconGallery()
      : image_view_(new ImageView()),
        image_view_container_(new views::View()),
        size_input_(new Textfield()),
        color_input_(new Textfield()),
        file_chooser_(new Textfield()),
        file_go_button_(new BlueButton(this, base::ASCIIToUTF16("Render"))),
        vector_id_(0),
        // 36dp is one of the natural sizes for MD icons, and corresponds
        // roughly to a 32dp usable area.
        size_(36),
        color_(SK_ColorRED) {
    AddChildView(size_input_);
    AddChildView(color_input_);

    image_view_container_->AddChildView(image_view_);
    BoxLayout* image_layout = new BoxLayout(BoxLayout::kHorizontal, 0, 0, 0);
    image_layout->set_cross_axis_alignment(
        BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    image_layout->set_main_axis_alignment(
        BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    image_view_container_->SetLayoutManager(image_layout);
    image_view_->SetBorder(
        Border::CreateSolidSidedBorder(1, 1, 1, 1, SK_ColorBLACK));
    AddChildView(image_view_container_);

    BoxLayout* box = new BoxLayout(BoxLayout::kVertical, 10, 10, 10);
    SetLayoutManager(box);
    box->SetFlexForView(image_view_container_, 1);

    file_chooser_->set_placeholder_text(
        base::ASCIIToUTF16("Or enter a file to read"));
    View* file_container = new View();
    BoxLayout* file_box = new BoxLayout(BoxLayout::kHorizontal, 10, 10, 10);
    file_container->SetLayoutManager(file_box);
    file_container->AddChildView(file_chooser_);
    file_container->AddChildView(file_go_button_);
    file_box->SetFlexForView(file_chooser_, 1);
    AddChildView(file_container);

    size_input_->set_placeholder_text(base::ASCIIToUTF16("Size in dip"));
    size_input_->set_controller(this);
    color_input_->set_placeholder_text(base::ASCIIToUTF16("Color (AARRGGBB)"));
    color_input_->set_controller(this);

    UpdateImage();
  }

  ~VectorIconGallery() override {}

  // View implementation.
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (GetEventHandlerForPoint(event.location()) == image_view_container_) {
      int increment = event.IsOnlyRightMouseButton() ? -1 : 1;
      int icon_count = static_cast<int>(gfx::VectorIconId::VECTOR_ICON_NONE);
      vector_id_ = (icon_count + vector_id_ + increment) % icon_count;
      UpdateImage();
      return true;
    }
    return false;
  }

  // TextfieldController implementation.
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override {
    if (sender == size_input_) {
      if (base::StringToSizeT(new_contents, &size_))
        UpdateImage();
      else
        size_input_->SetText(base::string16());

      return;
    }

    DCHECK_EQ(color_input_, sender);
    if (new_contents.size() != 8u)
      return;
    unsigned new_color =
        strtoul(base::UTF16ToASCII(new_contents).c_str(), nullptr, 16);
    if (new_color <= 0xffffffff) {
      color_ = new_color;
      UpdateImage();
    }
  }

  // ButtonListener
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK_EQ(file_go_button_, sender);
    std::string contents;
#if defined(OS_POSIX)
    base::FilePath path(base::UTF16ToUTF8(file_chooser_->text()));
#elif defined(OS_WIN)
    base::FilePath path(file_chooser_->text());
#endif
    base::ReadFileToString(path, &contents);
    image_view_->SetImage(
        gfx::CreateVectorIconFromSource(contents, size_, color_));
  }

  void UpdateImage() {
    image_view_->SetImage(gfx::CreateVectorIcon(
        static_cast<gfx::VectorIconId>(vector_id_), size_, color_));
    Layout();
  }

 private:
  ImageView* image_view_;
  View* image_view_container_;
  Textfield* size_input_;
  Textfield* color_input_;
  Textfield* file_chooser_;
  Button* file_go_button_;

  int vector_id_;
  size_t size_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconGallery);
};

}  // namespace

VectorExample::VectorExample() : ExampleBase("Vector Icon") {}

VectorExample::~VectorExample() {}

void VectorExample::CreateExampleView(View* container) {
  container->SetLayoutManager(new FillLayout());
  container->AddChildView(new VectorIconGallery());
}

}  // namespace examples
}  // namespace views
