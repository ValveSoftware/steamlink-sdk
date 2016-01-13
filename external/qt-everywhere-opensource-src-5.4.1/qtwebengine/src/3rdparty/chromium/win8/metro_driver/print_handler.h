// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_PRINT_HANDLER_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_PRINT_HANDLER_H_

#include <windows.media.playto.h>
#include <windows.graphics.printing.h>
#include <windows.ui.core.h>

#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "win8/metro_driver/print_document_source.h"
#include "win8/metro_driver/winrt_utils.h"

namespace base {

class Lock;

}  // namespace base

namespace metro_driver {

// This class handles the print aspect of the devices charm.
class PrintHandler {
 public:
  PrintHandler();
  ~PrintHandler();

  HRESULT Initialize(winui::Core::ICoreWindow* window);

  // Called by the exported C functions.
  static void EnablePrinting(bool printing_enabled);
  static void SetPageCount(size_t page_count);
  static void AddPage(size_t page_number, IStream* metafile_stream);
  static void ShowPrintUI();

 private:
  // Callbacks from Metro.
  HRESULT OnPrintRequested(
      wingfx::Printing::IPrintManager* print_mgr,
      wingfx::Printing::IPrintTaskRequestedEventArgs* event_args);

  HRESULT OnPrintTaskSourceRequest(
      wingfx::Printing::IPrintTaskSourceRequestedArgs* args);

  HRESULT OnCompleted(wingfx::Printing::IPrintTask* task,
                      wingfx::Printing::IPrintTaskCompletedEventArgs* args);
  // Utility methods.
  void ClearPrintTask();
  float GetLogicalDpi();

  // Callback from Metro and entry point called on lockable thread.
  HRESULT LogicalDpiChanged(IInspectable *sender);
  static void OnLogicalDpiChanged(float dpi);

  // Called on the lockable thread to set/release the doc.
  static void SetPrintDocumentSource(
      const mswr::ComPtr<PrintDocumentSource>& print_document_source);
  static void ReleasePrintDocumentSource();

  // Called on the lockable thread for the exported C functions.
  static void OnEnablePrinting(bool printing_enabled);
  static void OnSetPageCount(size_t page_count);
  static void OnAddPage(size_t page_number,
                        mswr::ComPtr<IStream> metafile_stream);

  // Opens the prit device charm. Must be called from the metro thread.
  static void OnShowPrintUI();

  mswr::ComPtr<wingfx::Printing::IPrintTask> print_task_;
  EventRegistrationToken print_requested_token_;
  EventRegistrationToken print_completed_token_;
  EventRegistrationToken dpi_change_token_;

  mswr::ComPtr<wingfx::Printing::IPrintManager> print_manager_;
  PrintDocumentSource::DirectXContext directx_context_;

  // Hack to give access to the Print Document from the C style entry points.
  // This will go away once we can pass a pointer to this interface down to
  // the Chrome Browser as we send the command to print.
  static mswr::ComPtr<PrintDocumentSource> current_document_source_;

  // Another hack to enable/disable printing from an exported C function.
  // TODO(mad): Find a better way to do this...
  static bool printing_enabled_;

  // This is also a temporary hack until we can pass down the print document
  // to Chrome so it can call directly into it. We need to lock the access to
  // current_document_source_.
  static base::Lock* lock_;

  // This thread is used to send blocking jobs
  // out of threads we don't want to block.
  static base::Thread* thread_;
};

}  // namespace metro_driver

// Exported C functions for Chrome to call into the Metro module.
extern "C" __declspec(dllexport)
void MetroEnablePrinting(BOOL printing_enabled);

extern "C" __declspec(dllexport)
void MetroSetPrintPageCount(size_t page_count);

extern "C" __declspec(dllexport)
void MetroSetPrintPageContent(size_t current_page,
                              void* data,
                              size_t data_size);

extern "C" __declspec(dllexport)
void MetroShowPrintUI();

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_PRINT_HANDLER_H_
