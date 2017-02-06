// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_WIN_H_
#define PRINTING_PRINTING_CONTEXT_WIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PrintSettings;

class PRINTING_EXPORT PrintingContextWin : public PrintingContext {
 public:
  explicit PrintingContextWin(Delegate* delegate);
  ~PrintingContextWin() override;

  // PrintingContext implementation.
  void AskUserForSettings(
      int max_pages,
      bool has_selection,
      bool is_scripted,
      const PrintSettingsCallback& callback) override;
  Result UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result InitWithSettings(const PrintSettings& settings) override;
  Result NewDocument(const base::string16& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  gfx::NativeDrawingContext context() const override;

 protected:
  static HWND GetRootWindow(gfx::NativeView view);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  virtual Result InitializeSettings(const base::string16& device_name,
                                    DEVMODE* dev_mode);

  HDC contest() const { return context_; }

  void set_context(HDC context) { context_ = context; }

 private:
  // Used in response to the user canceling the printing.
  static BOOL CALLBACK AbortProc(HDC hdc, int nCode);

  // The selected printer context.
  HDC context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextWin);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_WIN_H_
