// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_page.h"

#include <utility>

namespace printing {

PrintedPage::PrintedPage(int page_number,
                         std::unique_ptr<MetafilePlayer> metafile,
                         const gfx::Size& page_size,
                         const gfx::Rect& page_content_rect)
    : page_number_(page_number),
      metafile_(std::move(metafile)),
#if defined(OS_WIN)
      shrink_factor_(0.0f),
#endif  // OS_WIN
      page_size_(page_size),
      page_content_rect_(page_content_rect) {
}

PrintedPage::~PrintedPage() {
}

const MetafilePlayer* PrintedPage::metafile() const {
  return metafile_.get();
}

void PrintedPage::GetCenteredPageContentRect(
    const gfx::Size& paper_size, gfx::Rect* content_rect) const {
  *content_rect = page_content_rect();
  if (paper_size.width() > page_size().width()) {
    int diff = paper_size.width() - page_size().width();
    content_rect->set_x(content_rect->x() + diff / 2);
  }
  if (paper_size.height() > page_size().height()) {
    int diff = paper_size.height() - page_size().height();
    content_rect->set_y(content_rect->y() + diff / 2);
  }
}

}  // namespace printing
