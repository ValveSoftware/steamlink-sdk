// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/table_view_observer.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

class TableViewTestHelper {
 public:
  explicit TableViewTestHelper(TableView* table) : table_(table) {}

  std::string GetPaintRegion(const gfx::Rect& bounds) {
    TableView::PaintRegion region(table_->GetPaintRegion(bounds));
    return "rows=" + base::IntToString(region.min_row) + " " +
        base::IntToString(region.max_row) + " cols=" +
        base::IntToString(region.min_column) + " " +
        base::IntToString(region.max_column);
  }

  size_t visible_col_count() {
    return table_->visible_columns().size();
  }

  TableHeader* header() { return table_->header_; }

  void SetSelectionModel(const ui::ListSelectionModel& new_selection) {
    table_->SetSelectionModel(new_selection);
  }

  void OnFocus() {
    table_->OnFocus();
  }

 private:
  TableView* table_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTestHelper);
};

namespace {

// TestTableModel2 -------------------------------------------------------------

// Trivial TableModel implementation that is backed by a vector of vectors.
// Provides methods for adding/removing/changing the contents that notify the
// observer appropriately.
//
// Initial contents are:
// 0, 1
// 1, 1
// 2, 2
// 3, 0
class TestTableModel2 : public ui::TableModel {
 public:
  TestTableModel2();

  // Adds a new row at index |row| with values |c1_value| and |c2_value|.
  void AddRow(int row, int c1_value, int c2_value);

  // Removes the row at index |row|.
  void RemoveRow(int row);

  // Changes the values of the row at |row|.
  void ChangeRow(int row, int c1_value, int c2_value);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(int row1, int row2, int column_id) override;

 private:
  ui::TableModelObserver* observer_;

  // The data.
  std::vector<std::vector<int> > rows_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel2);
};

TestTableModel2::TestTableModel2() : observer_(NULL) {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
  AddRow(3, 3, 0);
}

void TestTableModel2::AddRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}
void TestTableModel2::RemoveRow(int row) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel2::ChangeRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row < static_cast<int>(rows_.size()));
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

int TestTableModel2::RowCount() {
  return static_cast<int>(rows_.size());
}

base::string16 TestTableModel2::GetText(int row, int column_id) {
  return base::IntToString16(rows_[row][column_id]);
}

void TestTableModel2::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel2::CompareValues(int row1, int row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// Returns the view to model mapping as a string.
std::string GetViewToModelAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->RowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::IntToString(table->ViewToModel(i));
  }
  return result;
}

// Returns the model to view mapping as a string.
std::string GetModelToViewAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->RowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::IntToString(table->ModelToView(i));
  }
  return result;
}

class TestTableView : public TableView {
 public:
  TestTableView(ui::TableModel* model,
                const std::vector<ui::TableColumn>& columns)
      : TableView(model, columns, TEXT_ONLY, false) {
  }

  // View overrides:
  bool HasFocus() const override {
    // Overriden so key processing works.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTableView);
};

}  // namespace

class TableViewTest : public testing::Test {
 public:
  TableViewTest() : table_(NULL) {}

  void SetUp() override {
    model_.reset(new TestTableModel2);
    std::vector<ui::TableColumn> columns(2);
    columns[0].title = base::ASCIIToUTF16("Title Column 0");
    columns[0].sortable = true;
    columns[1].title = base::ASCIIToUTF16("Title Column 1");
    columns[1].id = 1;
    columns[1].sortable = true;
    table_ = new TestTableView(model_.get(), columns);
    parent_.reset(table_->CreateParentIfNecessary());
    parent_->SetBounds(0, 0, 10000, 10000);
    parent_->Layout();
    helper_.reset(new TableViewTestHelper(table_));
  }

  void ClickOnRow(int row, int flags) {
    const int y = row * table_->row_height();
    const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(0, y),
                                 gfx::Point(0, y), ui::EventTimeForNow(),
                                 ui::EF_LEFT_MOUSE_BUTTON | flags,
                                 ui::EF_LEFT_MOUSE_BUTTON);
    table_->OnMousePressed(pressed);
  }

  void TapOnRow(int row) {
    const int y = row * table_->row_height();
    const ui::GestureEventDetails event_details(ui::ET_GESTURE_TAP);
    ui::GestureEvent tap(0, y, 0, base::TimeTicks(), event_details);
    table_->OnGestureEvent(&tap);
  }

  // Returns the state of the selection model as a string. The format is:
  // 'active=X anchor=X selection=X X X...'.
  std::string SelectionStateAsString() const {
    const ui::ListSelectionModel& model(table_->selection_model());
    std::string result = "active=" + base::IntToString(model.active()) +
        " anchor=" + base::IntToString(model.anchor()) +
        " selection=";
    const ui::ListSelectionModel::SelectedIndices& selection(
        model.selected_indices());
    for (size_t i = 0; i < selection.size(); ++i) {
      if (i != 0)
        result += " ";
      result += base::IntToString(selection[i]);
    }
    return result;
  }

  void PressKey(ui::KeyboardCode code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, code, ui::EF_NONE);
    table_->OnKeyPressed(event);
  }

 protected:
  std::unique_ptr<TestTableModel2> model_;

  // Owned by |parent_|.
  TableView* table_;

  std::unique_ptr<TableViewTestHelper> helper_;

 private:
  std::unique_ptr<View> parent_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTest);
};

// Verifies GetPaintRegion.
TEST_F(TableViewTest, GetPaintRegion) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
  EXPECT_EQ("rows=0 4 cols=0 1",
            helper_->GetPaintRegion(gfx::Rect(0, 0, 1, table_->height())));
}

// Verifies SetColumnVisibility().
TEST_F(TableViewTest, ColumnVisibility) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Should do nothing (column already visible).
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Hide the first column.
  table_->SetColumnVisibility(0, false);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Hide the second column.
  table_->SetColumnVisibility(1, false);
  EXPECT_EQ(0u, helper_->visible_col_count());

  // Show the second column.
  table_->SetColumnVisibility(1, true);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Show the first column.
  table_->SetColumnVisibility(0, true);
  ASSERT_EQ(2u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->visible_columns()[0].column.id);
  EXPECT_EQ(0, table_->visible_columns()[1].column.id);
  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
}

// Verifies resizing a column works.
TEST_F(TableViewTest, Resize) {
  const int x = table_->visible_columns()[0].width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(x, 0),
                               gfx::Point(x, 0), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMousePressed(pressed);
  const ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, gfx::Point(x - 1, 0),
                               gfx::Point(x - 1, 0), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  helper_->header()->OnMouseDragged(dragged);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->visible_columns()[0].width);
  EXPECT_EQ(x - 1, table_->visible_columns()[1].x);
}

// Verifies resizing a column works with a gesture.
TEST_F(TableViewTest, ResizeViaGesture) {
  const int x = table_->visible_columns()[0].width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  ui::GestureEvent scroll_begin(
      x, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  helper_->header()->OnGestureEvent(&scroll_begin);
  ui::GestureEvent scroll_update(
      x - 1, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE));
  helper_->header()->OnGestureEvent(&scroll_update);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->visible_columns()[0].width);
  EXPECT_EQ(x - 1, table_->visible_columns()[1].x);
}

// Assertions for table sorting.
TEST_F(TableViewTest, Sort) {
  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Invert the sort (first column descending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 2 1 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 2 1 0", GetModelToViewAsString(table_));

  // Change cell 0x3 to -1, meaning we have 0, 1, 2, -1 (in the first column).
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 1 0 3", GetViewToModelAsString(table_));
  EXPECT_EQ("2 1 0 3", GetModelToViewAsString(table_));

  // Invert sort again (first column ascending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 0 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("1 2 3 0", GetModelToViewAsString(table_));

  // Add a row so that model has 0, 3, 1, 2, -1.
  model_->AddRow(1, 3, 4);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("4 0 2 3 1", GetViewToModelAsString(table_));
  EXPECT_EQ("1 4 2 3 0", GetModelToViewAsString(table_));

  // Delete the first row, ending up with 3, 1, 2, -1.
  model_->RemoveRow(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 1 2 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 1 2 0", GetModelToViewAsString(table_));
}

// Verfies clicking on the header sorts.
TEST_F(TableViewTest, SortOnMouse) {
  EXPECT_TRUE(table_->sort_descriptors().empty());

  const int x = table_->visible_columns()[0].width / 2;
  EXPECT_NE(0, x);
  // Press and release the mouse.
  const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, gfx::Point(x, 0),
                               gfx::Point(x, 0), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  // The header must return true, else it won't normally get the release.
  EXPECT_TRUE(helper_->header()->OnMousePressed(pressed));
  const ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(x, 0),
                               gfx::Point(x, 0), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  helper_->header()->OnMouseReleased(release);

  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
}

namespace {

class TableGrouperImpl : public TableGrouper {
 public:
  TableGrouperImpl() {}

  void SetRanges(const std::vector<int>& ranges) {
    ranges_ = ranges;
  }

  // TableGrouper overrides:
  void GetGroupRange(int model_index, GroupRange* range) override {
    int offset = 0;
    size_t range_index = 0;
    for (; range_index < ranges_.size() && offset < model_index; ++range_index)
      offset += ranges_[range_index];

    if (offset == model_index) {
      range->start = model_index;
      range->length = ranges_[range_index];
    } else {
      range->start = offset - ranges_[range_index - 1];
      range->length = ranges_[range_index - 1];
    }
  }

 private:
  std::vector<int> ranges_;

  DISALLOW_COPY_AND_ASSIGN(TableGrouperImpl);
};

}  // namespace

// Assertions around grouping.
TEST_F(TableViewTest, Grouping) {
  // Configure the grouper so that there are two groups:
  // A 0
  //   1
  // B 2
  //   3
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Sort descending, resulting:
  // B 2
  //   3
  // A 0
  //   1
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));

  // Change the entry in the 4th row to -1. The model now becomes:
  // A 0
  //   1
  // B 2
  //   -1
  // Since the first entry in the range didn't change the sort isn't impacted.
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));

  // Change the entry in the 3rd row to -1. The model now becomes:
  // A 0
  //   1
  // B -1
  //   -1
  model_->ChangeRow(2, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Toggle to ascending sort.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));
}

namespace {

class TableViewObserverImpl : public TableViewObserver {
 public:
  TableViewObserverImpl() : selection_changed_count_(0) {}

  int GetChangedCountAndClear() {
    const int count = selection_changed_count_;
    selection_changed_count_ = 0;
    return count;
  }

  // TableViewObserver overrides:
  void OnSelectionChanged() override { selection_changed_count_++; }

 private:
  int selection_changed_count_;

  DISALLOW_COPY_AND_ASSIGN(TableViewObserverImpl);
};

}  // namespace

// Assertions around changing the selection.
TEST_F(TableViewTest, Selection) {
  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select the last row.
  table_->Select(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Change sort, shouldn't notify of change (toggle twice so that order
  // actually changes).
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Remove the selected row, this should notify of a change and update the
  // selection.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Insert a row, since the selection in terms of the original model hasn't
  // changed the observer is not notified.
  model_->AddRow(0, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// 0 1 2 3:
// select 3 -> 0 1 2 [3]
// remove 3 -> 0 1 2 (none selected)
// select 1 -> 0 [1] 2
// remove 1 -> 0 1 (none selected)
// select 0 -> [0] 1
// remove 0 -> 0 (none selected)
TEST_F(TableViewTest, SelectionNoSelectOnRemove) {
  TableViewObserverImpl observer;
  table_->SetObserver(&observer);
  table_->set_select_on_remove(false);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 3.
  table_->Select(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Remove the selected row, this should notify of a change and since the
  // select_on_remove_ is set false, and the removed item is the previously
  // selected item, so no item is selected.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 1.
  table_->Select(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 0.
  table_->Select(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  table_->SetObserver(nullptr);
}

// Verifies selection works by way of a gesture.
TEST_F(TableViewTest, SelectOnTap) {
  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Click on the first row, should select it.
  TapOnRow(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// Verifies up/down correctly navigates through groups.
TEST_F(TableViewTest, KeyUpDown) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  // Sort the table descending by column 1, view now looks like:
  // B 5   model: 2
  // C 2          3
  //   3          4
  // A 0          0
  //   1          1
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);

  EXPECT_EQ("2 3 4 0 1", GetViewToModelAsString(table_));

  table_->Select(-1);
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  observer.GetChangedCountAndClear();
  // Up with nothing selected selects the first row.
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// Verifies home/end do the right thing.
TEST_F(TableViewTest, HomeEnd) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_END);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// Verifies multiple selection gestures work (control-click, shift-click ...).
TEST_F(TableViewTest, Multiselection) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Click on the first row, should select it and the second row.
  ClickOnRow(0, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  // Click on the last row, should select it and the row before it.
  ClickOnRow(4, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  // Shift click on the third row, should extend selection to it.
  ClickOnRow(2, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=4 selection=2 3 4", SelectionStateAsString());

  // Control click on third row, should toggle it.
  ClickOnRow(2, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=3 4", SelectionStateAsString());

  // Control-shift click on second row, should extend selection to it.
  ClickOnRow(1, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=2 selection=0 1 2 3 4", SelectionStateAsString());

  // Click on last row again.
  ClickOnRow(4, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// Verifies multiple selection gestures work when sorted.
TEST_F(TableViewTest, MultiselectionWithSort) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  // Sort the table descending by column 1, view now looks like:
  // B 5   model: 2
  // C 2          3
  //   3          4
  // A 0          0
  //   1          1
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->SetObserver(&observer);

  // Click on the third row, should select it and the second row.
  ClickOnRow(2, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  // Extend selection to first row.
  ClickOnRow(0, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=4 selection=2 3 4", SelectionStateAsString());

  table_->SetObserver(NULL);
}

// Verifies we don't crash after removing the selected row when there is
// sorting and the anchor/active index also match the selected row.
TEST_F(TableViewTest, FocusAfterRemovingAnchor) {
  table_->ToggleSortOrder(0);

  ui::ListSelectionModel new_selection;
  new_selection.AddIndexToSelection(0);
  new_selection.AddIndexToSelection(1);
  new_selection.set_active(0);
  new_selection.set_anchor(0);
  helper_->SetSelectionModel(new_selection);
  model_->RemoveRow(0);
  helper_->OnFocus();
}

}  // namespace views
