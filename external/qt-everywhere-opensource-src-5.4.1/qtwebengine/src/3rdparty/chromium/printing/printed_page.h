// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_PAGE_H_
#define PRINTING_PRINTED_PAGE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "printing/metafile.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace printing {

// Contains the data to reproduce a printed page, either on screen or on
// paper. Once created, this object is immutable. It has no reference to the
// PrintedDocument containing this page.
// Note: May be accessed from many threads at the same time. This is an non
// issue since this object is immutable. The reason is that a page may be
// printed and be displayed at the same time.
class PRINTING_EXPORT PrintedPage
    : public base::RefCountedThreadSafe<PrintedPage> {
 public:
  PrintedPage(int page_number,
              Metafile* metafile,
              const gfx::Size& page_size,
              const gfx::Rect& page_content_rect);

  // Getters
  int page_number() const { return page_number_; }
  const Metafile* metafile() const;
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& page_content_rect() const { return page_content_rect_; }
#if defined(OS_WIN)
  void set_shrink_factor(double shrink_factor) {
    shrink_factor_ = shrink_factor;
  }
  double shrink_factor() const { return shrink_factor_; }
#endif  // OS_WIN

  // Get page content rect adjusted based on
  // http://dev.w3.org/csswg/css3-page/#positioning-page-box
  void GetCenteredPageContentRect(const gfx::Size& paper_size,
                                  gfx::Rect* content_rect) const;

 private:
  friend class base::RefCountedThreadSafe<PrintedPage>;

  ~PrintedPage();

  // Page number inside the printed document.
  const int page_number_;

  // Actual paint data.
  const scoped_ptr<Metafile> metafile_;

#if defined(OS_WIN)
  // Shrink done in comparison to desired_dpi.
  double shrink_factor_;
#endif  // OS_WIN

  // The physical page size. To support multiple page formats inside on print
  // job.
  const gfx::Size page_size_;

  // The printable area of the page.
  const gfx::Rect page_content_rect_;

  DISALLOW_COPY_AND_ASSIGN(PrintedPage);
};

}  // namespace printing

#endif  // PRINTING_PRINTED_PAGE_H_
