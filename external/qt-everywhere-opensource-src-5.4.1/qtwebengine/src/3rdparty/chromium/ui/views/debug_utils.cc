// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/debug_utils.h"

#include <ostream>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/view.h"

namespace views {
namespace {
void PrintViewHierarchyImp(const View* view,
                           int indent,
                           std::wostringstream* out) {
  int ind = indent;
  while (ind-- > 0)
    *out << L' ';
  *out << base::UTF8ToWide(view->GetClassName());
  *out << L' ';
  *out << view->id();
  *out << L' ';
  *out << view->x() << L"," << view->y() << L",";
  *out << view->bounds().right() << L"," << view->bounds().bottom();
  *out << L' ';
  *out << view;
  *out << L'\n';

  for (int i = 0, count = view->child_count(); i < count; ++i)
    PrintViewHierarchyImp(view->child_at(i), indent + 2, out);
}

void PrintFocusHierarchyImp(const View* view,
                            int indent,
                            std::wostringstream* out) {
  int ind = indent;
  while (ind-- > 0)
    *out << L' ';
  *out << base::UTF8ToWide(view->GetClassName());
  *out << L' ';
  *out << view->id();
  *out << L' ';
  *out << view->GetClassName();
  *out << L' ';
  *out << view;
  *out << L'\n';

  if (view->child_count() > 0)
    PrintFocusHierarchyImp(view->child_at(0), indent + 2, out);

  const View* next_focusable = view->GetNextFocusableView();
  if (next_focusable)
    PrintFocusHierarchyImp(next_focusable, indent, out);
}
}  // namespace

void PrintViewHierarchy(const View* view) {
  std::wostringstream out;
  out << L"View hierarchy:\n";
  PrintViewHierarchyImp(view, 0, &out);
  // Error so users in the field can generate and upload logs.
  LOG(ERROR) << out.str();
}

void PrintFocusHierarchy(const View* view) {
  std::wostringstream out;
  out << L"Focus hierarchy:\n";
  PrintFocusHierarchyImp(view, 0, &out);
  // Error so users in the field can generate and upload logs.
  LOG(ERROR) << out.str();
}

}  // namespace views
