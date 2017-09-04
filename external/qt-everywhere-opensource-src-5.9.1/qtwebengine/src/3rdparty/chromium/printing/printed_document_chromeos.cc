// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "base/logging.h"
#include "printing/page_number.h"
#include "printing/printed_page.h"

#if defined(USE_CUPS)
#include "printing/printing_context_chromeos.h"
#endif

namespace printing {

void PrintedDocument::RenderPrintedPage(const PrintedPage& page,
                                        PrintingContext* context) const {
#if defined(USE_CUPS)
#if defined(NDEBUG)
  {
    // Make sure the page is from our list.
    base::AutoLock lock(lock_);
    DCHECK(&page == mutable_.pages_.find(page.page_number() - 1)->second.get());
  }
#endif  // defined(NDEBUG)

  DCHECK(context);

  {
    base::AutoLock lock(lock_);
    if (page.page_number() - 1 == mutable_.first_page) {
      std::vector<char> buffer;

      if (page.metafile()->GetDataAsVector(&buffer)) {
        static_cast<PrintingContextChromeos*>(context)->StreamData(buffer);
      } else {
        LOG(WARNING) << "Failed to read data from metafile";
      }
    }
  }
#else
  NOTREACHED();
#endif  // defined(USE_CUPS)
}

}  // namespace printing
