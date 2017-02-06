// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/logging.h"
#include "printing/page_number.h"
#include "printing/printed_page.h"
#include "printing/printing_context_linux.h"

namespace printing {

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
void PrintedDocument::RenderPrintedPage(
    const PrintedPage& page, PrintingContext* context) const {
#ifndef NDEBUG
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif

  DCHECK(context);

  {
    base::AutoLock lock(lock_);
    if (page.page_number() - 1 == mutable_.first_page) {
      static_cast<PrintingContextLinux*>(context)
          ->PrintDocument(*page.metafile());
    }
  }
}
#endif  // !OS_CHROMEOS && !OS_ANDROID

}  // namespace printing
