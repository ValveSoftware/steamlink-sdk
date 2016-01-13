// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_DESTINATION_INTERFACE_H_
#define PRINTING_PRINT_DESTINATION_INTERFACE_H_

#include "base/memory/ref_counted.h"
#include "printing/printing_export.h"

namespace printing {

class PrintDestinationInterface
    : public base::RefCountedThreadSafe<PrintDestinationInterface> {
 public:
  // Sets the number of pages to print as soon as it is known.
  virtual void SetPageCount(int page_count) = 0;

  // Sets the metafile bits for a given page as soon as it is ready.
  virtual void SetPageContent(int page_number,
                              void* content,
                              size_t content_size) = 0;
 protected:
  friend class base::RefCountedThreadSafe<PrintDestinationInterface>;
  virtual ~PrintDestinationInterface() {}
};

PRINTING_EXPORT PrintDestinationInterface* CreatePrintDestination();

}  // namespace printing

#endif  // PRINTING_PRINT_DESTINATION_INTERFACE_H_
