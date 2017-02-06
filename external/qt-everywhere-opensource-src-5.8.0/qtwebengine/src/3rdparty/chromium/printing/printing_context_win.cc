// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include <algorithm>

#include "base/bind.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/win_helper.h"
#include "printing/print_settings_initializer_win.h"
#include "printing/printed_document.h"
#include "printing/printing_context_system_dialog_win.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace printing {

namespace {

void AssingResult(PrintingContext::Result* out, PrintingContext::Result in) {
  *out = in;
}

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::Create(Delegate* delegate) {
#if defined(ENABLE_BASIC_PRINTING)
  return base::WrapUnique(new PrintingContextSytemDialogWin(delegate));
#else   // ENABLE_BASIC_PRINTING
  return base::WrapUnique(new PrintingContextWin(delegate));
#endif  // EENABLE_BASIC_PRINTING
}

PrintingContextWin::PrintingContextWin(Delegate* delegate)
    : PrintingContext(delegate), context_(NULL) {
}

PrintingContextWin::~PrintingContextWin() {
  ReleaseContext();
}

void PrintingContextWin::AskUserForSettings(
    int max_pages,
    bool has_selection,
    bool is_scripted,
    const PrintSettingsCallback& callback) {
  NOTIMPLEMENTED();
}

PrintingContext::Result PrintingContextWin::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  scoped_refptr<PrintBackend> backend = PrintBackend::CreateInstance(NULL);
  base::string16 default_printer =
      base::UTF8ToWide(backend->GetDefaultPrinterName());
  if (!default_printer.empty()) {
    ScopedPrinterHandle printer;
    if (printer.OpenPrinter(default_printer.c_str())) {
      std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
          CreateDevMode(printer.Get(), NULL);
      if (InitializeSettings(default_printer, dev_mode.get()) == OK)
        return OK;
    }
  }

  ReleaseContext();

  // No default printer configured, do we have any printers at all?
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  (void)::EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,
                       NULL, 2, NULL, 0, &bytes_needed, &count_returned);
  if (bytes_needed) {
    DCHECK_GE(bytes_needed, count_returned * sizeof(PRINTER_INFO_2));
    std::unique_ptr<BYTE[]> printer_info_buffer(new BYTE[bytes_needed]);
    BOOL ret = ::EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,
                              NULL, 2, printer_info_buffer.get(),
                              bytes_needed, &bytes_needed,
                              &count_returned);
    if (ret && count_returned) {  // have printers
      // Open the first successfully found printer.
      const PRINTER_INFO_2* info_2 =
          reinterpret_cast<PRINTER_INFO_2*>(printer_info_buffer.get());
      const PRINTER_INFO_2* info_2_end = info_2 + count_returned;
      for (; info_2 < info_2_end; ++info_2) {
        ScopedPrinterHandle printer;
        if (!printer.OpenPrinter(info_2->pPrinterName))
          continue;
        std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
            CreateDevMode(printer.Get(), NULL);
        if (InitializeSettings(info_2->pPrinterName, dev_mode.get()) == OK)
          return OK;
      }
      if (context_)
        return OK;
    }
  }

  return OnError();
}

gfx::Size PrintingContextWin::GetPdfPaperSizeDeviceUnits() {
  // Default fallback to Letter size.
  gfx::SizeF paper_size(kLetterWidthInch, kLetterHeightInch);

  // Get settings from locale. Paper type buffer length is at most 4.
  const int paper_type_buffer_len = 4;
  wchar_t paper_type_buffer[paper_type_buffer_len] = {0};
  GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IPAPERSIZE, paper_type_buffer,
                paper_type_buffer_len);
  if (wcslen(paper_type_buffer)) {  // The call succeeded.
    int paper_code = _wtoi(paper_type_buffer);
    switch (paper_code) {
      case DMPAPER_LEGAL:
        paper_size.SetSize(kLegalWidthInch, kLegalHeightInch);
        break;
      case DMPAPER_A4:
        paper_size.SetSize(kA4WidthInch, kA4HeightInch);
        break;
      case DMPAPER_A3:
        paper_size.SetSize(kA3WidthInch, kA3HeightInch);
        break;
      default:  // DMPAPER_LETTER is used for default fallback.
        break;
    }
  }
  return gfx::Size(
      paper_size.width() * settings_.device_units_per_inch(),
      paper_size.height() * settings_.device_units_per_inch());
}

PrintingContext::Result PrintingContextWin::UpdatePrinterSettings(
    bool external_preview,
    bool show_system_dialog,
    int page_count) {
  DCHECK(!in_print_job_);
  DCHECK(!external_preview) << "Not implemented";

  ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(settings_.device_name().c_str()))
    return OnError();

  // Make printer changes local to Chrome.
  // See MSDN documentation regarding DocumentProperties.
  std::unique_ptr<DEVMODE, base::FreeDeleter> scoped_dev_mode =
      CreateDevModeWithColor(printer.Get(), settings_.device_name(),
                             settings_.color() != GRAY);
  if (!scoped_dev_mode)
    return OnError();

  {
    DEVMODE* dev_mode = scoped_dev_mode.get();
    dev_mode->dmCopies = std::max(settings_.copies(), 1);
    if (dev_mode->dmCopies > 1) {  // do not change unless multiple copies
      dev_mode->dmFields |= DM_COPIES;
      dev_mode->dmCollate = settings_.collate() ? DMCOLLATE_TRUE :
                                                  DMCOLLATE_FALSE;
    }

    switch (settings_.duplex_mode()) {
      case LONG_EDGE:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_VERTICAL;
        break;
      case SHORT_EDGE:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_HORIZONTAL;
        break;
      case SIMPLEX:
        dev_mode->dmFields |= DM_DUPLEX;
        dev_mode->dmDuplex = DMDUP_SIMPLEX;
        break;
      default:  // UNKNOWN_DUPLEX_MODE
        break;
    }

    dev_mode->dmFields |= DM_ORIENTATION;
    dev_mode->dmOrientation = settings_.landscape() ? DMORIENT_LANDSCAPE :
                                                      DMORIENT_PORTRAIT;

    const PrintSettings::RequestedMedia& requested_media =
        settings_.requested_media();
    static const int kFromUm = 100;  // Windows uses 0.1mm.
    int width = requested_media.size_microns.width() / kFromUm;
    int height = requested_media.size_microns.height() / kFromUm;
    unsigned id = 0;
    if (base::StringToUint(requested_media.vendor_id, &id) && id) {
      dev_mode->dmFields |= DM_PAPERSIZE;
      dev_mode->dmPaperSize = static_cast<short>(id);
    } else if (width > 0 && height > 0) {
      dev_mode->dmFields |= DM_PAPERWIDTH;
      dev_mode->dmPaperWidth = width;
      dev_mode->dmFields |= DM_PAPERLENGTH;
      dev_mode->dmPaperLength = height;
    }
  }

  // Update data using DocumentProperties.
  if (show_system_dialog) {
    PrintingContext::Result result = PrintingContext::FAILED;
    AskUserForSettings(page_count, false, false,
                       base::Bind(&AssingResult, &result));
    return result;
  } else {
    scoped_dev_mode = CreateDevMode(printer.Get(), scoped_dev_mode.get());
  }
  // Set printer then refresh printer settings.
  return InitializeSettings(settings_.device_name(), scoped_dev_mode.get());
}

PrintingContext::Result PrintingContextWin::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  // TODO(maruel): settings_.ToDEVMODE()
  ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(settings_.device_name().c_str()))
    return FAILED;

  std::unique_ptr<DEVMODE, base::FreeDeleter> dev_mode =
      CreateDevMode(printer.Get(), NULL);

  return InitializeSettings(settings_.device_name(), dev_mode.get());
}

PrintingContext::Result PrintingContextWin::NewDocument(
    const base::string16& document_name) {
  DCHECK(!in_print_job_);
  if (!context_)
    return OnError();

  // Set the flag used by the AbortPrintJob dialog procedure.
  abort_printing_ = false;

  in_print_job_ = true;

  // Register the application's AbortProc function with GDI.
  if (SP_ERROR == SetAbortProc(context_, &AbortProc))
    return OnError();

  DCHECK(SimplifyDocumentTitle(document_name) == document_name);
  DOCINFO di = { sizeof(DOCINFO) };
  di.lpszDocName = document_name.c_str();

  // Is there a debug dump directory specified? If so, force to print to a file.
  base::string16 debug_dump_path =
      PrintedDocument::CreateDebugDumpPath(document_name,
                                           FILE_PATH_LITERAL(".prn")).value();
  if (!debug_dump_path.empty())
    di.lpszOutput = debug_dump_path.c_str();

  // No message loop running in unit tests.
  DCHECK(!base::MessageLoop::current() ||
         !base::MessageLoop::current()->NestableTasksAllowed());

  // Begin a print job by calling the StartDoc function.
  // NOTE: StartDoc() starts a message loop. That causes a lot of problems with
  // IPC. Make sure recursive task processing is disabled.
  if (StartDoc(context_, &di) <= 0)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextWin::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(context_);
  DCHECK(in_print_job_);

  // Intentional No-op. PdfMetafileSkia::SafePlayback takes care of calling
  // ::StartPage().

  return OK;
}

PrintingContext::Result PrintingContextWin::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op. PdfMetafileSkia::SafePlayback takes care of calling
  // ::EndPage().

  return OK;
}

PrintingContext::Result PrintingContextWin::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(context_);

  // Inform the driver that document has ended.
  if (EndDoc(context_) <= 0)
    return OnError();

  ResetSettings();
  return OK;
}

void PrintingContextWin::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  if (context_)
    CancelDC(context_);
}

void PrintingContextWin::ReleaseContext() {
  if (context_) {
    DeleteDC(context_);
    context_ = NULL;
  }
}

gfx::NativeDrawingContext PrintingContextWin::context() const {
  return context_;
}

// static
BOOL PrintingContextWin::AbortProc(HDC hdc, int nCode) {
  if (nCode) {
    // TODO(maruel):  Need a way to find the right instance to set. Should
    // leverage PrintJobManager here?
    // abort_printing_ = true;
  }
  return true;
}

PrintingContext::Result PrintingContextWin::InitializeSettings(
    const std::wstring& device_name,
    DEVMODE* dev_mode) {
  if (!dev_mode)
    return OnError();

  ReleaseContext();
  context_ = CreateDC(L"WINSPOOL", device_name.c_str(), NULL, dev_mode);
  if (!context_)
    return OnError();

  skia::InitializeDC(context_);

  DCHECK(!in_print_job_);
  settings_.set_device_name(device_name);
  PrintSettingsInitializerWin::InitPrintSettings(
      context_, *dev_mode, &settings_);

  return OK;
}

HWND PrintingContextWin::GetRootWindow(gfx::NativeView view) {
  HWND window = NULL;
  if (view && view->GetHost())
    window = view->GetHost()->GetAcceleratedWidget();
  if (!window) {
    // TODO(maruel):  crbug.com/1214347 Get the right browser window instead.
    return GetDesktopWindow();
  }
  return window;
}

}  // namespace printing
