// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/print_handler.h"

#include <windows.graphics.display.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/winrt_utils.h"

namespace {

typedef winfoundtn::ITypedEventHandler<
    wingfx::Printing::PrintManager*,
    wingfx::Printing::PrintTaskRequestedEventArgs*> PrintRequestedHandler;

typedef winfoundtn::ITypedEventHandler<
    wingfx::Printing::PrintTask*,
    wingfx::Printing::PrintTaskCompletedEventArgs*> PrintTaskCompletedHandler;

typedef winfoundtn::ITypedEventHandler<
    wingfx::Printing::PrintTask*, IInspectable*> PrintTaskInspectableHandler;

typedef winfoundtn::ITypedEventHandler<
    wingfx::Printing::PrintTask*,
    wingfx::Printing::PrintTaskProgressingEventArgs*>
    PrintTaskProgressingHandler;

}  // namespace

namespace metro_driver {

mswr::ComPtr<PrintDocumentSource> PrintHandler::current_document_source_;
bool PrintHandler::printing_enabled_ = false;
base::Lock* PrintHandler::lock_ = NULL;
base::Thread* PrintHandler::thread_ = NULL;

PrintHandler::PrintHandler() {
  DCHECK(lock_ == NULL);
  lock_ = new  base::Lock();

  DCHECK(thread_ == NULL);
  thread_ = new base::Thread("Metro Print Handler");
  thread_->Start();
}

PrintHandler::~PrintHandler() {
  ClearPrintTask();
  DCHECK(current_document_source_.Get() == NULL);

  // Get all pending tasks to complete cleanly by Stopping the thread.
  // They should complete quickly since current_document_source_ is NULL.
  DCHECK(thread_ != NULL);
  DCHECK(thread_->IsRunning());
  thread_->Stop();
  delete thread_;
  thread_ = NULL;

  DCHECK(lock_ != NULL);
  delete lock_;
  lock_ = NULL;
}

HRESULT PrintHandler::Initialize(winui::Core::ICoreWindow* window) {
  // Register for Print notifications.
  mswr::ComPtr<wingfx::Printing::IPrintManagerStatic> print_mgr_static;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Graphics_Printing_PrintManager,
      print_mgr_static.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create PrintManagerStatic " << std::hex << hr;
    return hr;
  }

  mswr::ComPtr<wingfx::Printing::IPrintManager> print_mgr;
  hr = print_mgr_static->GetForCurrentView(&print_mgr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get PrintManager for current view " << std::hex
               << hr;
    return hr;
  }

  hr = print_mgr->add_PrintTaskRequested(
      mswr::Callback<PrintRequestedHandler>(
          this, &PrintHandler::OnPrintRequested).Get(),
      &print_requested_token_);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to register PrintTaskRequested "
                            << std::hex << hr;

  mswr::ComPtr<wingfx::Display::IDisplayPropertiesStatics> display_properties;
  hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Graphics_Display_DisplayProperties,
      display_properties.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create DisplayPropertiesStatics " << std::hex
               << hr;
    return hr;
  }

  hr = display_properties->add_LogicalDpiChanged(
      mswr::Callback<
          wingfx::Display::IDisplayPropertiesEventHandler,
          PrintHandler>(this, &PrintHandler::LogicalDpiChanged).Get(),
      &dpi_change_token_);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to register LogicalDpiChanged "
                            << std::hex << hr;

  // This flag adds support for surfaces with a different color channel
  // ordering than the API default. It is recommended usage, and is required
  // for compatibility with Direct2D.
  UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
  creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  // This array defines the set of DirectX hardware feature levels we support.
  // The ordering MUST be preserved. All applications are assumed to support
  // 9.1 unless otherwise stated by the application, which is not our case.
  D3D_FEATURE_LEVEL feature_levels[] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1 };

  mswr::ComPtr<ID3D11Device> device;
  mswr::ComPtr<ID3D11DeviceContext> context;
  hr = D3D11CreateDevice(
      NULL,  // Specify null to use the default adapter.
      D3D_DRIVER_TYPE_HARDWARE,
      0,  // Leave as 0 unless software device.
      creation_flags,
      feature_levels,
      ARRAYSIZE(feature_levels),
      D3D11_SDK_VERSION,  // Must always use this value in Metro apps.
      &device,
      NULL,  // Returns feature level of device created.
      &context);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // The hardware is not supported, try a reference driver instead.
    hr = D3D11CreateDevice(
        NULL,  // Specify null to use the default adapter.
        D3D_DRIVER_TYPE_REFERENCE,
        0,  // Leave as 0 unless software device.
        creation_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,  // Must always use this value in Metro apps.
        &device,
        NULL,  // Returns feature level of device created.
        &context);
  }
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D3D11 device/context " << std::hex << hr;
    return hr;
  }

  hr = device.As(&directx_context_.d3d_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI D3D11 device " << std::hex << hr;
    return hr;
  }

  D2D1_FACTORY_OPTIONS options;
  ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
  options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,
                         __uuidof(ID2D1Factory1),
                         &options,
                         &directx_context_.d2d_factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create D2D1 factory " << std::hex << hr;
    return hr;
  }

  mswr::ComPtr<IDXGIDevice> dxgi_device;
  hr = directx_context_.d3d_device.As(&dxgi_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI for IDXGIDevice " << std::hex << hr;
    return hr;
  }

  hr = directx_context_.d2d_factory->CreateDevice(
      dxgi_device.Get(), &directx_context_.d2d_device);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to Create D2DDevice " << std::hex << hr;
    return hr;
  }

  hr = directx_context_.d2d_device->CreateDeviceContext(
      D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
      &directx_context_.d2d_context);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to Create D2DDeviceContext " << std::hex << hr;
    return hr;
  }

  hr = CoCreateInstance(CLSID_WICImagingFactory,
                        NULL,
                        CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&directx_context_.wic_factory));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CoCreate WICImagingFactory " << std::hex << hr;
    return hr;
  }
  return hr;
}

void PrintHandler::EnablePrinting(bool printing_enabled) {
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::OnEnablePrinting, printing_enabled));
}

void PrintHandler::SetPageCount(size_t page_count) {
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::OnSetPageCount, page_count));
}

void PrintHandler::AddPage(size_t page_number, IStream* metafile_stream) {
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::OnAddPage,
                 page_number,
                 mswr::ComPtr<IStream>(metafile_stream)));
}

void PrintHandler::ShowPrintUI() {
  // Post the print UI request over to the metro thread.
  DCHECK(globals.appview_msg_loop != NULL);
  bool posted = globals.appview_msg_loop->PostTask(
      FROM_HERE, base::Bind(&metro_driver::PrintHandler::OnShowPrintUI));
  DCHECK(posted);
}

HRESULT PrintHandler::OnPrintRequested(
    wingfx::Printing::IPrintManager* print_mgr,
    wingfx::Printing::IPrintTaskRequestedEventArgs* event_args) {
  DVLOG(1) << __FUNCTION__;

  HRESULT hr = S_OK;
  if (printing_enabled_) {
    mswr::ComPtr<wingfx::Printing::IPrintTaskRequest> print_request;
    hr = event_args->get_Request(print_request.GetAddressOf());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get the Print Task request " << std::hex << hr;
      return hr;
    }

    mswrw::HString title;
    title.Attach(MakeHString(L"Printing"));
    hr = print_request->CreatePrintTask(
        title.Get(),
        mswr::Callback<
            wingfx::Printing::IPrintTaskSourceRequestedHandler,
            PrintHandler>(this, &PrintHandler::OnPrintTaskSourceRequest).Get(),
        print_task_.GetAddressOf());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create the Print Task " << std::hex << hr;
      return hr;
    }

    hr = print_task_->add_Completed(
        mswr::Callback<PrintTaskCompletedHandler>(
            this, &PrintHandler::OnCompleted).Get(), &print_completed_token_);
    LOG_IF(ERROR, FAILED(hr)) << "Failed to create the Print Task " << std::hex
                              << hr;
  }
  return hr;
}

HRESULT PrintHandler::OnPrintTaskSourceRequest(
    wingfx::Printing::IPrintTaskSourceRequestedArgs* args) {
  DVLOG(1) << __FUNCTION__;
  mswr::ComPtr<PrintDocumentSource> print_document_source;
  HRESULT hr = mswr::MakeAndInitialize<PrintDocumentSource>(
      &print_document_source, directx_context_, lock_);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create document source " << std::hex << hr;
    return hr;
  }

  print_document_source->ResetDpi(GetLogicalDpi());

  mswr::ComPtr<wingfx::Printing::IPrintDocumentSource> print_source;
  hr = print_document_source.As(&print_source);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to cast document Source " << std::hex << hr;
    return hr;
  }

  hr = args->SetSource(print_source.Get());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to set document Source " << std::hex << hr;
    return hr;
  }

  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::SetPrintDocumentSource,
                 print_document_source));

  return hr;
}

HRESULT PrintHandler::OnCompleted(
    wingfx::Printing::IPrintTask* task,
    wingfx::Printing::IPrintTaskCompletedEventArgs* args) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(globals.appview_msg_loop->BelongsToCurrentThread());
  ClearPrintTask();
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::ReleasePrintDocumentSource));

  return S_OK;
}

void PrintHandler::ClearPrintTask() {
  if (!print_task_.Get())
    return;

  HRESULT hr = print_task_->remove_Completed(print_completed_token_);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to remove completed event from Task "
                            << std::hex << hr;
  print_task_.Reset();
}

float PrintHandler::GetLogicalDpi() {
  mswr::ComPtr<wingfx::Display::IDisplayPropertiesStatics> display_properties;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Graphics_Display_DisplayProperties,
      display_properties.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get display properties " << std::hex << hr;
    return 0.0;
  }

  FLOAT dpi = 0.0;
  hr = display_properties->get_LogicalDpi(&dpi);
  LOG_IF(ERROR, FAILED(hr)) << "Failed to get Logical DPI " << std::hex << hr;

  return dpi;
}

HRESULT PrintHandler::LogicalDpiChanged(IInspectable *sender) {
  DVLOG(1) << __FUNCTION__;
  thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PrintHandler::OnLogicalDpiChanged, GetLogicalDpi()));
  return S_OK;
}

void PrintHandler::OnLogicalDpiChanged(float dpi) {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  // No need to protect the access to the static variable,
  // since it's set/released in this same thread.
  if (current_document_source_.Get() != NULL)
    current_document_source_->ResetDpi(dpi);
}

void PrintHandler::SetPrintDocumentSource(
    const mswr::ComPtr<PrintDocumentSource>& print_document_source) {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  DCHECK(current_document_source_.Get() == NULL);
  {
    // Protect against the other thread which might try to access it.
    base::AutoLock lock(*lock_);
    current_document_source_ = print_document_source;
  }
  // Start generating the images to print.
  // TODO(mad): Use a registered message with more information about the print
  // request, and at a more appropriate time too, and maybe one page at a time.
  ::PostMessageW(globals.host_windows.front().first,
                 WM_SYSCOMMAND,
                 IDC_PRINT_TO_DESTINATION,
                 0);
}

void PrintHandler::ReleasePrintDocumentSource() {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  mswr::ComPtr<PrintDocumentSource> print_document_source;
  {
    // Must wait for other thread to be done with the pointer first.
    base::AutoLock lock(*lock_);
    current_document_source_.Swap(print_document_source);
  }
  // This may happen before we get a chance to set the value.
  if (print_document_source.Get() != NULL)
    print_document_source->Abort();
}

void PrintHandler::OnEnablePrinting(bool printing_enabled) {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  base::AutoLock lock(*lock_);
  printing_enabled_ = printing_enabled;
  // Don't abort if we are being disabled since we may be finishing a previous
  // print request which was valid and should be finished. We just need to
  // prevent any new print requests. And don't assert that we are NOT printing
  // if we are becoming enabled since we may be finishing a long print while
  // we got disabled and then enabled again...
}

void PrintHandler::OnSetPageCount(size_t page_count) {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  // No need to protect the access to the static variable,
  // since it's set/released in this same thread.
  if (current_document_source_.Get() != NULL)
    current_document_source_->SetPageCount(page_count);
}

void PrintHandler::OnAddPage(size_t page_number,
                             mswr::ComPtr<IStream> metafile_stream) {
  DCHECK(base::MessageLoop::current() == thread_->message_loop());
  // No need to protect the access to the static variable,
  // since it's set/released in this same thread.
  if (current_document_source_.Get() != NULL)
    current_document_source_->AddPage(page_number, metafile_stream.Get());
}

void PrintHandler::OnShowPrintUI() {
  DCHECK(globals.appview_msg_loop->BelongsToCurrentThread());
  mswr::ComPtr<wingfx::Printing::IPrintManagerStatic> print_mgr_static;
  HRESULT hr = winrt_utils::CreateActivationFactory(
      RuntimeClass_Windows_Graphics_Printing_PrintManager,
      print_mgr_static.GetAddressOf());
  if (SUCCEEDED(hr)) {
    DCHECK(print_mgr_static.Get() != NULL);
    // Note that passing NULL to ShowPrintUIAsync crashes,
    // so we need to setup a temp pointer.
    mswr::ComPtr<winfoundtn::IAsyncOperation<bool>> unused_async_op;
    hr = print_mgr_static->ShowPrintUIAsync(unused_async_op.GetAddressOf());
    LOG_IF(ERROR, FAILED(hr)) << "Failed to ShowPrintUIAsync "
                              << std::hex << std::showbase << hr;
  } else {
    LOG(ERROR) << "Failed to create PrintManagerStatic "
               << std::hex << std::showbase << hr;
  }
}

}  // namespace metro_driver

void MetroEnablePrinting(BOOL printing_enabled) {
  metro_driver::PrintHandler::EnablePrinting(!!printing_enabled);
}

void MetroSetPrintPageCount(size_t page_count) {
  DVLOG(1) << __FUNCTION__ << " Page count: " << page_count;
  metro_driver::PrintHandler::SetPageCount(page_count);
}

void MetroSetPrintPageContent(size_t page_number,
                              void* data,
                              size_t data_size) {
  DVLOG(1) << __FUNCTION__ << " Page number: " << page_number;
  DCHECK(data != NULL);
  DCHECK(data_size > 0);
  mswr::ComPtr<IStream> metafile_stream;
  HRESULT hr = ::CreateStreamOnHGlobal(
      NULL, TRUE, metafile_stream.GetAddressOf());
  if (metafile_stream.Get() != NULL) {
    ULONG bytes_written = 0;
    hr = metafile_stream->Write(data,
                                base::checked_cast<ULONG>(data_size),
                                &bytes_written);
    LOG_IF(ERROR, FAILED(hr)) << "Failed to Write to Stream " << std::hex << hr;
    DCHECK(bytes_written == data_size);

    metro_driver::PrintHandler::AddPage(page_number, metafile_stream.Get());
  } else {
    NOTREACHED() << "Failed to CreateStreamOnHGlobal " << std::hex << hr;
  }
}

void MetroShowPrintUI() {
  metro_driver::PrintHandler::ShowPrintUI();
}
