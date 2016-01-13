// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/prefix_selector.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/prefix_delegate.h"
#include "ui/views/test/views_test_base.h"

using base::ASCIIToUTF16;

namespace views {

class TestPrefixDelegate : public PrefixDelegate {
 public:
  TestPrefixDelegate() : selected_row_(0) {
    rows_.push_back(ASCIIToUTF16("aardvark"));
    rows_.push_back(ASCIIToUTF16("antelope"));
    rows_.push_back(ASCIIToUTF16("badger"));
    rows_.push_back(ASCIIToUTF16("gnu"));
  }

  virtual ~TestPrefixDelegate() {}

  virtual int GetRowCount() OVERRIDE {
    return static_cast<int>(rows_.size());
  }

  virtual int GetSelectedRow() OVERRIDE {
    return selected_row_;
  }

  virtual void SetSelectedRow(int row) OVERRIDE {
    selected_row_ = row;
  }

  virtual base::string16 GetTextForRow(int row) OVERRIDE {
    return rows_[row];
  }

 private:
  std::vector<base::string16> rows_;
  int selected_row_;

  DISALLOW_COPY_AND_ASSIGN(TestPrefixDelegate);
};

class PrefixSelectorTest : public ViewsTestBase {
 public:
  PrefixSelectorTest() {
    selector_.reset(new PrefixSelector(&delegate_));
  }

 protected:
  scoped_ptr<PrefixSelector> selector_;
  TestPrefixDelegate delegate_;

 private:
    DISALLOW_COPY_AND_ASSIGN(PrefixSelectorTest);
};

TEST_F(PrefixSelectorTest, PrefixSelect) {
  selector_->InsertText(ASCIIToUTF16("an"));
  EXPECT_EQ(1, delegate_.GetSelectedRow());

  // Invoke OnViewBlur() to reset time.
  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(0, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("g"));
  EXPECT_EQ(3, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("b"));
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(2, delegate_.GetSelectedRow());

  selector_->OnViewBlur();
  selector_->InsertText(ASCIIToUTF16("\t"));
  selector_->InsertText(ASCIIToUTF16("b"));
  selector_->InsertText(ASCIIToUTF16("a"));
  EXPECT_EQ(2, delegate_.GetSelectedRow());
}

}  // namespace views
