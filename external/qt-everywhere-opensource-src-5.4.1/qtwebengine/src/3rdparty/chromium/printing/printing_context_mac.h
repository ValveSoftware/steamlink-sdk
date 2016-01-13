// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_MAC_H_
#define PRINTING_PRINTING_CONTEXT_MAC_H_

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "printing/print_job_constants.h"
#include "printing/printing_context.h"

#ifdef __OBJC__
@class NSPrintInfo;
#else
class NSPrintInfo;
#endif  // __OBJC__

namespace printing {

class PRINTING_EXPORT PrintingContextMac : public PrintingContext {
 public:
  explicit PrintingContextMac(const std::string& app_locale);
  virtual ~PrintingContextMac();

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      gfx::NativeView parent_view,
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) OVERRIDE;
  virtual Result UseDefaultSettings() OVERRIDE;
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() OVERRIDE;
  virtual Result UpdatePrinterSettings(bool external_preview) OVERRIDE;
  virtual Result InitWithSettings(const PrintSettings& settings) OVERRIDE;
  virtual Result NewDocument(const base::string16& document_name) OVERRIDE;
  virtual Result NewPage() OVERRIDE;
  virtual Result PageDone() OVERRIDE;
  virtual Result DocumentDone() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void ReleaseContext() OVERRIDE;
  virtual gfx::NativeDrawingContext context() const OVERRIDE;

 private:
  // Initializes PrintSettings from |print_info_|. This must be called
  // after changes to |print_info_| in order for the changes to take effect in
  // printing.
  // This function ignores the page range information specified in the print
  // info object and use |settings_.ranges| instead.
  void InitPrintSettingsFromPrintInfo();

  // Returns the set of page ranges constructed from |print_info_|.
  PageRanges GetPageRangesFromPrintInfo();

  // Updates |print_info_| to use the given printer.
  // Returns true if the printer was set.
  bool SetPrinter(const std::string& device_name);

  // Updates |print_info_| page format with paper selected by user. If paper was
  // not selected, default system paper is used.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaperInfo();

  // Updates |print_info_| page format with |paper|.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaper(PMPaper paper, PMPageFormat page_format);

  // Sets the print job destination type as preview job.
  // Returns true if the print job destination type is set.
  bool SetPrintPreviewJob();

  // Sets |copies| in PMPrintSettings.
  // Returns true if the number of copies is set.
  bool SetCopiesInPrintSettings(int copies);

  // Sets |collate| in PMPrintSettings.
  // Returns true if |collate| is set.
  bool SetCollateInPrintSettings(bool collate);

  // Sets orientation in native print info object.
  // Returns true if the orientation was set.
  bool SetOrientationIsLandscape(bool landscape);

  // Sets duplex mode in PMPrintSettings.
  // Returns true if duplex mode is set.
  bool SetDuplexModeInPrintSettings(DuplexMode mode);

  // Sets output color mode in PMPrintSettings.
  // Returns true if color mode is set.
  bool SetOutputColor(int color_mode);

  // The native print info object.
  base::scoped_nsobject<NSPrintInfo> print_info_;

  // The current page's context; only valid between NewPage and PageDone call
  // pairs.
  CGContext* context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextMac);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_MAC_H_
