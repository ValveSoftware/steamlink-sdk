// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
#define PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_

#include <string>

#include "printing/printing_context.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class PRINTING_EXPORT PrintingContextNoSystemDialog : public PrintingContext {
 public:
  explicit PrintingContextNoSystemDialog(const std::string& app_locale);
  virtual ~PrintingContextNoSystemDialog();

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
  DISALLOW_COPY_AND_ASSIGN(PrintingContextNoSystemDialog);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
