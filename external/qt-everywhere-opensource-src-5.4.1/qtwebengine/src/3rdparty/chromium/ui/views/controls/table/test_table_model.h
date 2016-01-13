// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
#define UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_

#include "base/compiler_specific.h"
#include "ui/base/models/table_model.h"

class TestTableModel : public ui::TableModel {
 public:
  explicit TestTableModel(int row_count);
  virtual ~TestTableModel();

  // ui::TableModel overrides:
  virtual int RowCount() OVERRIDE;
  virtual base::string16 GetText(int row, int column_id) OVERRIDE;
  virtual gfx::ImageSkia GetIcon(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  int row_count_;
  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel);
};

#endif  // UI_VIEWS_CONTROLS_TABLE_TEST_TABLE_MODEL_H_
