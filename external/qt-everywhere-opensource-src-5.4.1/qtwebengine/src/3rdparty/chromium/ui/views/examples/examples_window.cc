// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_window.h"

#include <algorithm>
#include <string>

#include "base/memory/scoped_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/examples/bubble_example.h"
#include "ui/views/examples/button_example.h"
#include "ui/views/examples/checkbox_example.h"
#include "ui/views/examples/combobox_example.h"
#include "ui/views/examples/double_split_view_example.h"
#include "ui/views/examples/label_example.h"
#include "ui/views/examples/link_example.h"
#include "ui/views/examples/menu_example.h"
#include "ui/views/examples/message_box_example.h"
#include "ui/views/examples/multiline_example.h"
#include "ui/views/examples/progress_bar_example.h"
#include "ui/views/examples/radio_button_example.h"
#include "ui/views/examples/scroll_view_example.h"
#include "ui/views/examples/single_split_view_example.h"
#include "ui/views/examples/slider_example.h"
#include "ui/views/examples/tabbed_pane_example.h"
#include "ui/views/examples/table_example.h"
#include "ui/views/examples/text_example.h"
#include "ui/views/examples/textfield_example.h"
#include "ui/views/examples/throbber_example.h"
#include "ui/views/examples/tree_view_example.h"
#include "ui/views/examples/widget_example.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace examples {

typedef scoped_ptr<ScopedVector<ExampleBase> > ScopedExamples;

namespace {

// Creates the default set of examples. Caller owns the result.
ScopedExamples CreateExamples() {
  ScopedExamples examples(new ScopedVector<ExampleBase>);
  examples->push_back(new BubbleExample);
  examples->push_back(new ButtonExample);
  examples->push_back(new CheckboxExample);
  examples->push_back(new ComboboxExample);
  examples->push_back(new DoubleSplitViewExample);
  examples->push_back(new LabelExample);
  examples->push_back(new LinkExample);
  examples->push_back(new MenuExample);
  examples->push_back(new MessageBoxExample);
  examples->push_back(new MultilineExample);
  examples->push_back(new ProgressBarExample);
  examples->push_back(new RadioButtonExample);
  examples->push_back(new ScrollViewExample);
  examples->push_back(new SingleSplitViewExample);
  examples->push_back(new SliderExample);
  examples->push_back(new TabbedPaneExample);
  examples->push_back(new TableExample);
  examples->push_back(new TextExample);
  examples->push_back(new TextfieldExample);
  examples->push_back(new ThrobberExample);
  examples->push_back(new TreeViewExample);
  examples->push_back(new WidgetExample);
  return examples.Pass();
}

struct ExampleTitleCompare {
  bool operator() (ExampleBase* a, ExampleBase* b) {
    return a->example_title() < b->example_title();
  }
};

ScopedExamples GetExamplesToShow(ScopedExamples extra) {
  ScopedExamples examples(CreateExamples());
  if (extra.get()) {
    examples->insert(examples->end(), extra->begin(), extra->end());
    extra->weak_clear();
  }
  std::sort(examples->begin(), examples->end(), ExampleTitleCompare());
  return examples.Pass();
}

}  // namespace

// Model for the examples that are being added via AddExample().
class ComboboxModelExampleList : public ui::ComboboxModel {
 public:
  ComboboxModelExampleList() {}
  virtual ~ComboboxModelExampleList() {}

  void SetExamples(ScopedExamples examples) {
    example_list_.swap(*examples);
  }

  // ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE { return example_list_.size(); }
  virtual base::string16 GetItemAt(int index) OVERRIDE {
    return base::UTF8ToUTF16(example_list_[index]->example_title());
  }

  View* GetItemViewAt(int index) {
    return example_list_[index]->example_view();
  }

 private:
  ScopedVector<ExampleBase> example_list_;

  DISALLOW_COPY_AND_ASSIGN(ComboboxModelExampleList);
};

class ExamplesWindowContents : public WidgetDelegateView,
                               public ComboboxListener {
 public:
  ExamplesWindowContents(Operation operation, ScopedExamples examples)
      : combobox_(new Combobox(&combobox_model_)),
        example_shown_(new View),
        status_label_(new Label),
        operation_(operation) {
    instance_ = this;
    combobox_->set_listener(this);
    combobox_model_.SetExamples(examples.Pass());
    combobox_->ModelChanged();

    set_background(Background::CreateStandardPanelBackground());
    GridLayout* layout = new GridLayout(this);
    SetLayoutManager(layout);
    ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddPaddingColumn(0, 5);
    column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                          GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0, 5);
    layout->AddPaddingRow(0, 5);
    layout->StartRow(0 /* no expand */, 0);
    layout->AddView(combobox_);

    if (combobox_model_.GetItemCount() > 0) {
      layout->StartRow(1, 0);
      example_shown_->SetLayoutManager(new FillLayout());
      example_shown_->AddChildView(combobox_model_.GetItemViewAt(0));
      layout->AddView(example_shown_);
    }

    layout->StartRow(0 /* no expand */, 0);
    layout->AddView(status_label_);
    layout->AddPaddingRow(0, 5);
  }

  virtual ~ExamplesWindowContents() {
    // Delete |combobox_| first as it references |combobox_model_|.
    delete combobox_;
    combobox_ = NULL;
  }

  // Prints a message in the status area, at the bottom of the window.
  void SetStatus(const std::string& status) {
    status_label_->SetText(base::UTF8ToUTF16(status));
  }

  static ExamplesWindowContents* instance() { return instance_; }

 private:
  // WidgetDelegateView:
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual bool CanMaximize() const OVERRIDE { return true; }
  virtual base::string16 GetWindowTitle() const OVERRIDE {
    return base::ASCIIToUTF16("Views Examples");
  }
  virtual View* GetContentsView() OVERRIDE { return this; }
  virtual void WindowClosing() OVERRIDE {
    instance_ = NULL;
    if (operation_ == QUIT_ON_CLOSE)
      base::MessageLoopForUI::current()->Quit();
  }

  // ComboboxListener:
  virtual void OnPerformAction(Combobox* combobox) OVERRIDE {
    DCHECK_EQ(combobox, combobox_);
    DCHECK(combobox->selected_index() < combobox_model_.GetItemCount());
    example_shown_->RemoveAllChildViews(false);
    example_shown_->AddChildView(combobox_model_.GetItemViewAt(
        combobox->selected_index()));
    example_shown_->RequestFocus();
    SetStatus(std::string());
    Layout();
  }

  static ExamplesWindowContents* instance_;
  ComboboxModelExampleList combobox_model_;
  Combobox* combobox_;
  View* example_shown_;
  Label* status_label_;
  const Operation operation_;

  DISALLOW_COPY_AND_ASSIGN(ExamplesWindowContents);
};

// static
ExamplesWindowContents* ExamplesWindowContents::instance_ = NULL;

void ShowExamplesWindow(Operation operation,
                        gfx::NativeView window_context,
                        ScopedExamples extra_examples) {
  if (ExamplesWindowContents::instance()) {
    ExamplesWindowContents::instance()->GetWidget()->Activate();
  } else {
    ScopedExamples examples(GetExamplesToShow(extra_examples.Pass()));
    Widget* widget = new Widget;
    Widget::InitParams params;
    params.delegate = new ExamplesWindowContents(operation, examples.Pass());
    params.context = window_context;
    params.bounds = gfx::Rect(0, 0, 850, 300);
    params.remove_standard_frame = true;
    widget->Init(params);
    widget->Show();
  }
}

void LogStatus(const std::string& string) {
  ExamplesWindowContents::instance()->SetStatus(string);
}

}  // namespace examples
}  // namespace views
