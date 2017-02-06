// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_header.h"

#include <stddef.h>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/native_cursor.h"

namespace views {

namespace {

const int kVerticalPadding = 4;

// The minimum width we allow a column to go down to.
const int kMinColumnWidth = 10;

// Distace from edge columns can be resized by.
const int kResizePadding = 5;

// Amount of space above/below the separator.
const int kSeparatorPadding = 4;

const SkColor kTextColor = SK_ColorBLACK;
const SkColor kBackgroundColor1 = SkColorSetRGB(0xF9, 0xF9, 0xF9);
const SkColor kBackgroundColor2 = SkColorSetRGB(0xE8, 0xE8, 0xE8);
const SkColor kSeparatorColor = SkColorSetRGB(0xAA, 0xAA, 0xAA);

// Size of the sort indicator (doesn't include padding).
const int kSortIndicatorSize = 8;

}  // namespace

// static
const char TableHeader::kViewClassName[] = "TableHeader";
// static
const int TableHeader::kHorizontalPadding = 7;
// static
const int TableHeader::kSortIndicatorWidth = kSortIndicatorSize +
    TableHeader::kHorizontalPadding * 2;

typedef std::vector<TableView::VisibleColumn> Columns;

TableHeader::TableHeader(TableView* table) : table_(table) {
  set_background(Background::CreateVerticalGradientBackground(
                     kBackgroundColor1, kBackgroundColor2));
}

TableHeader::~TableHeader() {
}

void TableHeader::Layout() {
  SetBounds(x(), y(), table_->width(), GetPreferredSize().height());
}

void TableHeader::OnPaint(gfx::Canvas* canvas) {
  // Paint the background and a separator at the bottom. The separator color
  // matches that of the border around the scrollview.
  OnPaintBackground(canvas);
  SkColor border_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_UnfocusedBorderColor);
  canvas->DrawLine(gfx::Point(0, height() - 1),
                   gfx::Point(width(), height() - 1), border_color);

  const Columns& columns = table_->visible_columns();
  const int sorted_column_id = table_->sort_descriptors().empty() ? -1 :
      table_->sort_descriptors()[0].column_id;
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].width >= 2) {
      const int separator_x = GetMirroredXInView(
          columns[i].x + columns[i].width - 1);
      canvas->DrawLine(gfx::Point(separator_x, kSeparatorPadding),
                       gfx::Point(separator_x, height() - kSeparatorPadding),
                       kSeparatorColor);
    }

    const int x = columns[i].x + kHorizontalPadding;
    int width = columns[i].width - kHorizontalPadding - kHorizontalPadding;
    if (width <= 0)
      continue;

    const int title_width =
        gfx::GetStringWidth(columns[i].column.title, font_list_);
    const bool paint_sort_indicator =
        (columns[i].column.id == sorted_column_id &&
         title_width + kSortIndicatorWidth <= width);

    if (paint_sort_indicator &&
        columns[i].column.alignment == ui::TableColumn::RIGHT) {
      width -= kSortIndicatorWidth;
    }

    canvas->DrawStringRectWithFlags(
        columns[i].column.title, font_list_, kTextColor,
        gfx::Rect(GetMirroredXWithWidthInView(x, width), kVerticalPadding,
                  width, height() - kVerticalPadding * 2),
        TableColumnAlignmentToCanvasAlignment(columns[i].column.alignment));

    if (paint_sort_indicator) {
      SkPaint paint;
      paint.setColor(kTextColor);
      paint.setStyle(SkPaint::kFill_Style);
      paint.setAntiAlias(true);

      int indicator_x = 0;
      ui::TableColumn::Alignment alignment = columns[i].column.alignment;
      if (base::i18n::IsRTL()) {
        if (alignment == ui::TableColumn::LEFT)
          alignment = ui::TableColumn::RIGHT;
        else if (alignment == ui::TableColumn::RIGHT)
          alignment = ui::TableColumn::LEFT;
      }
      switch (alignment) {
        case ui::TableColumn::LEFT:
          indicator_x = x + title_width;
          break;
        case ui::TableColumn::CENTER:
          indicator_x = x + width / 2;
          break;
        case ui::TableColumn::RIGHT:
          indicator_x = x + width;
          break;
      }

      const int scale = base::i18n::IsRTL() ? -1 : 1;
      indicator_x += (kSortIndicatorWidth - kSortIndicatorSize) / 2;
      indicator_x = GetMirroredXInView(indicator_x);
      int indicator_y = height() / 2 - kSortIndicatorSize / 2;
      SkPath indicator_path;
      if (table_->sort_descriptors()[0].ascending) {
        indicator_path.moveTo(
            SkIntToScalar(indicator_x),
            SkIntToScalar(indicator_y + kSortIndicatorSize));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize * scale),
            SkIntToScalar(indicator_y + kSortIndicatorSize));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize / 2 * scale),
            SkIntToScalar(indicator_y));
      } else {
        indicator_path.moveTo(SkIntToScalar(indicator_x),
                              SkIntToScalar(indicator_y));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize * scale),
            SkIntToScalar(indicator_y));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize / 2 * scale),
            SkIntToScalar(indicator_y + kSortIndicatorSize));
      }
      indicator_path.close();
      canvas->DrawPath(indicator_path, paint);
    }
  }
}

const char* TableHeader::GetClassName() const {
  return kViewClassName;
}

gfx::Size TableHeader::GetPreferredSize() const {
  return gfx::Size(1, kVerticalPadding * 2 + font_list_.GetHeight());
}

gfx::NativeCursor TableHeader::GetCursor(const ui::MouseEvent& event) {
  return GetResizeColumn(GetMirroredXInView(event.x())) != -1 ?
      GetNativeColumnResizeCursor() : View::GetCursor(event);
}

bool TableHeader::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    StartResize(event);
    return true;
  }

  // Return false so that context menus on ancestors work.
  return false;
}

bool TableHeader::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueResize(event);
  return true;
}

void TableHeader::OnMouseReleased(const ui::MouseEvent& event) {
  const bool was_resizing = resize_details_ != NULL;
  resize_details_.reset();
  if (!was_resizing && event.IsOnlyLeftMouseButton())
    ToggleSortOrder(event);
}

void TableHeader::OnMouseCaptureLost() {
  if (is_resizing()) {
    table_->SetVisibleColumnWidth(resize_details_->column_index,
                                  resize_details_->initial_width);
  }
  resize_details_.reset();
}

void TableHeader::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (!resize_details_.get())
        ToggleSortOrder(*event);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      StartResize(*event);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      ContinueResize(*event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      resize_details_.reset();
      break;
    default:
      return;
  }
  event->SetHandled();
}

bool TableHeader::StartResize(const ui::LocatedEvent& event) {
  if (is_resizing())
    return false;

  const int index = GetResizeColumn(GetMirroredXInView(event.x()));
  if (index == -1)
    return false;

  resize_details_.reset(new ColumnResizeDetails);
  resize_details_->column_index = index;
  resize_details_->initial_x = event.root_location().x();
  resize_details_->initial_width = table_->visible_columns()[index].width;
  return true;
}

void TableHeader::ContinueResize(const ui::LocatedEvent& event) {
  if (!is_resizing())
    return;

  const int scale = base::i18n::IsRTL() ? -1 : 1;
  const int delta = scale *
      (event.root_location().x() - resize_details_->initial_x);
  table_->SetVisibleColumnWidth(
      resize_details_->column_index,
      std::max(kMinColumnWidth, resize_details_->initial_width + delta));
}

void TableHeader::ToggleSortOrder(const ui::LocatedEvent& event) {
  if (table_->visible_columns().empty())
    return;

  const int x = GetMirroredXInView(event.x());
  const int index = GetClosestVisibleColumnIndex(table_, x);
  const TableView::VisibleColumn& column(table_->visible_columns()[index]);
  if (x >= column.x && x < column.x + column.width && event.y() >= 0 &&
      event.y() < height())
    table_->ToggleSortOrder(index);
}

int TableHeader::GetResizeColumn(int x) const {
  const Columns& columns(table_->visible_columns());
  if (columns.empty())
    return -1;

  const int index = GetClosestVisibleColumnIndex(table_, x);
  DCHECK_NE(-1, index);
  const TableView::VisibleColumn& column(table_->visible_columns()[index]);
  if (index > 0 && x >= column.x - kResizePadding &&
      x <= column.x + kResizePadding) {
    return index - 1;
  }
  const int max_x = column.x + column.width;
  return (x >= max_x - kResizePadding && x <= max_x + kResizePadding) ?
      index : -1;
}

}  // namespace views
