// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include <winspool.h>

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/printing_info_win.h"
#include "printing/backend/win_helper.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_initializer_win.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "skia/ext/platform_device.h"

#if defined(USE_AURA)
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

namespace {

HWND GetRootWindow(gfx::NativeView view) {
  HWND window = NULL;
#if defined(USE_AURA)
  if (view)
    window = view->GetHost()->GetAcceleratedWidget();
#else
  if (view && IsWindow(view)) {
    window = GetAncestor(view, GA_ROOTOWNER);
  }
#endif
  if (!window) {
    // TODO(maruel):  bug 1214347 Get the right browser window instead.
    return GetDesktopWindow();
  }
  return window;
}

}  // anonymous namespace

namespace printing {

class PrintingContextWin::CallbackHandler : public IPrintDialogCallback,
                                            public IObjectWithSite {
 public:
  CallbackHandler(PrintingContextWin& owner, HWND owner_hwnd)
      : owner_(owner),
        owner_hwnd_(owner_hwnd),
        services_(NULL) {
  }

  ~CallbackHandler() {
    if (services_)
      services_->Release();
  }

  IUnknown* ToIUnknown() {
    return static_cast<IUnknown*>(static_cast<IPrintDialogCallback*>(this));
  }

  // IUnknown
  virtual HRESULT WINAPI QueryInterface(REFIID riid, void**object) {
    if (riid == IID_IUnknown) {
      *object = ToIUnknown();
    } else if (riid == IID_IPrintDialogCallback) {
      *object = static_cast<IPrintDialogCallback*>(this);
    } else if (riid == IID_IObjectWithSite) {
      *object = static_cast<IObjectWithSite*>(this);
    } else {
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  // No real ref counting.
  virtual ULONG WINAPI AddRef() {
    return 1;
  }
  virtual ULONG WINAPI Release() {
    return 1;
  }

  // IPrintDialogCallback methods
  virtual HRESULT WINAPI InitDone() {
    return S_OK;
  }

  virtual HRESULT WINAPI SelectionChange() {
    if (services_) {
      // TODO(maruel): Get the devmode for the new printer with
      // services_->GetCurrentDevMode(&devmode, &size), send that information
      // back to our client and continue. The client needs to recalculate the
      // number of rendered pages and send back this information here.
    }
    return S_OK;
  }

  virtual HRESULT WINAPI HandleMessage(HWND dialog,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       LRESULT* result) {
    // Cheap way to retrieve the window handle.
    if (!owner_.dialog_box_) {
      // The handle we receive is the one of the groupbox in the General tab. We
      // need to get the grand-father to get the dialog box handle.
      owner_.dialog_box_ = GetAncestor(dialog, GA_ROOT);
      // Trick to enable the owner window. This can cause issues with navigation
      // events so it may have to be disabled if we don't fix the side-effects.
      EnableWindow(owner_hwnd_, TRUE);
    }
    return S_FALSE;
  }

  virtual HRESULT WINAPI SetSite(IUnknown* site) {
    if (!site) {
      DCHECK(services_);
      services_->Release();
      services_ = NULL;
      // The dialog box is destroying, PrintJob::Worker don't need the handle
      // anymore.
      owner_.dialog_box_ = NULL;
    } else {
      DCHECK(services_ == NULL);
      HRESULT hr = site->QueryInterface(IID_IPrintDialogServices,
                                        reinterpret_cast<void**>(&services_));
      DCHECK(SUCCEEDED(hr));
    }
    return S_OK;
  }

  virtual HRESULT WINAPI GetSite(REFIID riid, void** site) {
    return E_NOTIMPL;
  }

 private:
  PrintingContextWin& owner_;
  HWND owner_hwnd_;
  IPrintDialogServices* services_;

  DISALLOW_COPY_AND_ASSIGN(CallbackHandler);
};

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextWin(app_locale));
}

PrintingContextWin::PrintingContextWin(const std::string& app_locale)
    : PrintingContext(app_locale), context_(NULL), dialog_box_(NULL) {}

PrintingContextWin::~PrintingContextWin() {
  ReleaseContext();
}

void PrintingContextWin::AskUserForSettings(
    gfx::NativeView view, int max_pages, bool has_selection,
    const PrintSettingsCallback& callback) {
  DCHECK(!in_print_job_);
  dialog_box_dismissed_ = false;

  HWND window = GetRootWindow(view);
  DCHECK(window);

  // Show the OS-dependent dialog box.
  // If the user press
  // - OK, the settings are reset and reinitialized with the new settings. OK is
  //   returned.
  // - Apply then Cancel, the settings are reset and reinitialized with the new
  //   settings. CANCEL is returned.
  // - Cancel, the settings are not changed, the previous setting, if it was
  //   initialized before, are kept. CANCEL is returned.
  // On failure, the settings are reset and FAILED is returned.
  PRINTDLGEX dialog_options = { sizeof(PRINTDLGEX) };
  dialog_options.hwndOwner = window;
  // Disable options we don't support currently.
  // TODO(maruel):  Reuse the previously loaded settings!
  dialog_options.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE  |
                         PD_NOCURRENTPAGE | PD_HIDEPRINTTOFILE;
  if (!has_selection)
    dialog_options.Flags |= PD_NOSELECTION;

  PRINTPAGERANGE ranges[32];
  dialog_options.nStartPage = START_PAGE_GENERAL;
  if (max_pages) {
    // Default initialize to print all the pages.
    memset(ranges, 0, sizeof(ranges));
    ranges[0].nFromPage = 1;
    ranges[0].nToPage = max_pages;
    dialog_options.nPageRanges = 1;
    dialog_options.nMaxPageRanges = arraysize(ranges);
    dialog_options.nMinPage = 1;
    dialog_options.nMaxPage = max_pages;
    dialog_options.lpPageRanges = ranges;
  } else {
    // No need to bother, we don't know how many pages are available.
    dialog_options.Flags |= PD_NOPAGENUMS;
  }

  if (ShowPrintDialog(&dialog_options) != S_OK) {
    ResetSettings();
    callback.Run(FAILED);
  }

  // TODO(maruel):  Support PD_PRINTTOFILE.
  callback.Run(ParseDialogResultEx(dialog_options));
}

PrintingContext::Result PrintingContextWin::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  PRINTDLG dialog_options = { sizeof(PRINTDLG) };
  dialog_options.Flags = PD_RETURNDC | PD_RETURNDEFAULT;
  if (PrintDlg(&dialog_options))
    return ParseDialogResult(dialog_options);

  // No default printer configured, do we have any printers at all?
  DWORD bytes_needed = 0;
  DWORD count_returned = 0;
  (void)::EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS,
                       NULL, 2, NULL, 0, &bytes_needed, &count_returned);
  if (bytes_needed) {
    DCHECK_GE(bytes_needed, count_returned * sizeof(PRINTER_INFO_2));
    scoped_ptr<BYTE[]> printer_info_buffer(new BYTE[bytes_needed]);
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
        scoped_ptr<DEVMODE, base::FreeDeleter> dev_mode =
            CreateDevMode(printer, NULL);
        if (!dev_mode || !AllocateContext(info_2->pPrinterName, dev_mode.get(),
                                          &context_)) {
          continue;
        }
        if (InitializeSettings(*dev_mode.get(), info_2->pPrinterName, NULL, 0,
                               false)) {
          return OK;
        }
        ReleaseContext();
      }
      if (context_)
        return OK;
    }
  }

  ResetSettings();
  return FAILED;
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
    bool external_preview) {
  DCHECK(!in_print_job_);
  DCHECK(!external_preview) << "Not implemented";

  ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(settings_.device_name().c_str()))
    return OnError();

  // Make printer changes local to Chrome.
  // See MSDN documentation regarding DocumentProperties.
  scoped_ptr<DEVMODE, base::FreeDeleter> scoped_dev_mode =
      CreateDevModeWithColor(printer, settings_.device_name(),
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
  scoped_dev_mode = CreateDevMode(printer, scoped_dev_mode.get());
  if (!scoped_dev_mode)
    return OnError();

  // Set printer then refresh printer settings.
  if (!AllocateContext(settings_.device_name(), scoped_dev_mode.get(),
                       &context_)) {
    return OnError();
  }
  PrintSettingsInitializerWin::InitPrintSettings(context_,
                                                 *scoped_dev_mode.get(),
                                                 &settings_);
  return OK;
}

PrintingContext::Result PrintingContextWin::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  // TODO(maruel): settings_.ToDEVMODE()
  ScopedPrinterHandle printer;
  if (!printer.OpenPrinter(settings_.device_name().c_str())) {
    return FAILED;
  }

  Result status = OK;

  if (!GetPrinterSettings(printer, settings_.device_name()))
    status = FAILED;

  if (status != OK)
    ResetSettings();
  return status;
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

  // Intentional No-op. NativeMetafile::SafePlayback takes care of calling
  // ::StartPage().

  return OK;
}

PrintingContext::Result PrintingContextWin::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op. NativeMetafile::SafePlayback takes care of calling
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
  if (dialog_box_) {
    DestroyWindow(dialog_box_);
    dialog_box_dismissed_ = true;
  }
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

bool PrintingContextWin::InitializeSettings(const DEVMODE& dev_mode,
                                            const std::wstring& new_device_name,
                                            const PRINTPAGERANGE* ranges,
                                            int number_ranges,
                                            bool selection_only) {
  skia::InitializeDC(context_);
  DCHECK(GetDeviceCaps(context_, CLIPCAPS));
  DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_STRETCHDIB);
  DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_BITMAP64);
  // Some printers don't advertise these.
  // DCHECK(GetDeviceCaps(context_, RASTERCAPS) & RC_SCALING);
  // DCHECK(GetDeviceCaps(context_, SHADEBLENDCAPS) & SB_CONST_ALPHA);
  // DCHECK(GetDeviceCaps(context_, SHADEBLENDCAPS) & SB_PIXEL_ALPHA);

  // StretchDIBits() support is needed for printing.
  if (!(GetDeviceCaps(context_, RASTERCAPS) & RC_STRETCHDIB) ||
      !(GetDeviceCaps(context_, RASTERCAPS) & RC_BITMAP64)) {
    NOTREACHED();
    ResetSettings();
    return false;
  }

  DCHECK(!in_print_job_);
  DCHECK(context_);
  PageRanges ranges_vector;
  if (!selection_only) {
    // Convert the PRINTPAGERANGE array to a PrintSettings::PageRanges vector.
    ranges_vector.reserve(number_ranges);
    for (int i = 0; i < number_ranges; ++i) {
      PageRange range;
      // Transfer from 1-based to 0-based.
      range.from = ranges[i].nFromPage - 1;
      range.to = ranges[i].nToPage - 1;
      ranges_vector.push_back(range);
    }
  }

  settings_.set_ranges(ranges_vector);
  settings_.set_device_name(new_device_name);
  settings_.set_selection_only(selection_only);
  PrintSettingsInitializerWin::InitPrintSettings(context_, dev_mode,
                                                 &settings_);

  return true;
}

bool PrintingContextWin::GetPrinterSettings(HANDLE printer,
                                            const std::wstring& device_name) {
  DCHECK(!in_print_job_);

  scoped_ptr<DEVMODE, base::FreeDeleter> dev_mode =
      CreateDevMode(printer, NULL);

  if (!dev_mode || !AllocateContext(device_name, dev_mode.get(), &context_)) {
    ResetSettings();
    return false;
  }

  return InitializeSettings(*dev_mode.get(), device_name, NULL, 0, false);
}

// static
bool PrintingContextWin::AllocateContext(const std::wstring& device_name,
                                         const DEVMODE* dev_mode,
                                         gfx::NativeDrawingContext* context) {
  *context = CreateDC(L"WINSPOOL", device_name.c_str(), NULL, dev_mode);
  DCHECK(*context);
  return *context != NULL;
}

PrintingContext::Result PrintingContextWin::ParseDialogResultEx(
    const PRINTDLGEX& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  if (dialog_options.dwResultAction != PD_RESULT_CANCEL) {
    // Start fresh.
    ResetSettings();

    DEVMODE* dev_mode = NULL;
    if (dialog_options.hDevMode) {
      dev_mode =
          reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
      DCHECK(dev_mode);
    }

    std::wstring device_name;
    if (dialog_options.hDevNames) {
      DEVNAMES* dev_names =
          reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
      DCHECK(dev_names);
      if (dev_names) {
        device_name = reinterpret_cast<const wchar_t*>(dev_names) +
                      dev_names->wDeviceOffset;
        GlobalUnlock(dialog_options.hDevNames);
      }
    }

    bool success = false;
    if (dev_mode && !device_name.empty()) {
      context_ = dialog_options.hDC;
      PRINTPAGERANGE* page_ranges = NULL;
      DWORD num_page_ranges = 0;
      bool print_selection_only = false;
      if (dialog_options.Flags & PD_PAGENUMS) {
        page_ranges = dialog_options.lpPageRanges;
        num_page_ranges = dialog_options.nPageRanges;
      }
      if (dialog_options.Flags & PD_SELECTION) {
        print_selection_only = true;
      }
      success = InitializeSettings(*dev_mode,
                                   device_name,
                                   page_ranges,
                                   num_page_ranges,
                                   print_selection_only);
    }

    if (!success && dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
      context_ = NULL;
    }

    if (dev_mode) {
      GlobalUnlock(dialog_options.hDevMode);
    }
  } else {
    if (dialog_options.hDC) {
      DeleteDC(dialog_options.hDC);
    }
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  switch (dialog_options.dwResultAction) {
    case PD_RESULT_PRINT:
      return context_ ? OK : FAILED;
    case PD_RESULT_APPLY:
      return context_ ? CANCEL : FAILED;
    case PD_RESULT_CANCEL:
      return CANCEL;
    default:
      return FAILED;
  }
}

HRESULT PrintingContextWin::ShowPrintDialog(PRINTDLGEX* options) {
  // Note that this cannot use ui::BaseShellDialog as the print dialog is
  // system modal: opening it from a background thread can cause Windows to
  // get the wrong Z-order which will make the print dialog appear behind the
  // browser frame (but still being modal) so neither the browser frame nor
  // the print dialog will get any input. See http://crbug.com/342697
  // http://crbug.com/180997 for details.
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());

  return PrintDlgEx(options);
}

PrintingContext::Result PrintingContextWin::ParseDialogResult(
    const PRINTDLG& dialog_options) {
  // If the user clicked OK or Apply then Cancel, but not only Cancel.
  // Start fresh.
  ResetSettings();

  DEVMODE* dev_mode = NULL;
  if (dialog_options.hDevMode) {
    dev_mode =
        reinterpret_cast<DEVMODE*>(GlobalLock(dialog_options.hDevMode));
    DCHECK(dev_mode);
  }

  std::wstring device_name;
  if (dialog_options.hDevNames) {
    DEVNAMES* dev_names =
        reinterpret_cast<DEVNAMES*>(GlobalLock(dialog_options.hDevNames));
    DCHECK(dev_names);
    if (dev_names) {
      device_name =
          reinterpret_cast<const wchar_t*>(
              reinterpret_cast<const wchar_t*>(dev_names) +
                  dev_names->wDeviceOffset);
      GlobalUnlock(dialog_options.hDevNames);
    }
  }

  bool success = false;
  if (dev_mode && !device_name.empty()) {
    context_ = dialog_options.hDC;
    success = InitializeSettings(*dev_mode, device_name, NULL, 0, false);
  }

  if (!success && dialog_options.hDC) {
    DeleteDC(dialog_options.hDC);
    context_ = NULL;
  }

  if (dev_mode) {
    GlobalUnlock(dialog_options.hDevMode);
  }

  if (dialog_options.hDevMode != NULL)
    GlobalFree(dialog_options.hDevMode);
  if (dialog_options.hDevNames != NULL)
    GlobalFree(dialog_options.hDevNames);

  return context_ ? OK : FAILED;
}

}  // namespace printing
