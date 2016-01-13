// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/print_document_source.h"

#include <windows.graphics.display.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"


namespace {

class D2DFactoryAutoLock {
 public:
  explicit D2DFactoryAutoLock(ID2D1Factory* d2d_factory) {
    HRESULT hr = d2d_factory->QueryInterface(IID_PPV_ARGS(&d2d_multithread_));
    if (d2d_multithread_.Get())
      d2d_multithread_->Enter();
    else
      NOTREACHED() << "Failed to QI for ID2D1Multithread " << std::hex << hr;
  }

  ~D2DFactoryAutoLock() {
    if (d2d_multithread_.Get())
      d2d_multithread_->Leave();
  }

 private:
  mswr::ComPtr<ID2D1Multithread> d2d_multithread_;
};

// TODO(mad): remove once we don't run mixed SDK/OS anymore.
const GUID kOldPackageTargetGuid =
    {0xfb2a33c0, 0x8c35, 0x465f,
      {0xbe, 0xd5, 0x9f, 0x36, 0x89, 0x51, 0x77, 0x52}};
const GUID kNewPackageTargetGuid =
    {0x1a6dd0ad, 0x1e2a, 0x4e99,
      {0xa5, 0xba, 0x91, 0xf1, 0x78, 0x18, 0x29, 0x0e}};


}  // namespace

namespace metro_driver {

PrintDocumentSource::PrintDocumentSource()
    : page_count_ready_(true, false),
      parent_lock_(NULL),
      height_(0),
      width_(0),
      dpi_(96),
      aborted_(false),
      using_old_preview_interface_(false) {
}

HRESULT PrintDocumentSource::RuntimeClassInitialize(
    const DirectXContext& directx_context,
    base::Lock* parent_lock) {
  DCHECK(parent_lock != NULL);
  DCHECK(directx_context.d2d_context.Get() != NULL);
  DCHECK(directx_context.d2d_device.Get() != NULL);
  DCHECK(directx_context.d2d_factory.Get() != NULL);
  DCHECK(directx_context.d3d_device.Get() != NULL);
  DCHECK(directx_context.wic_factory.Get() != NULL);
  directx_context_ = directx_context;

  // No other method can be called before RuntimeClassInitialize which is called
  // during the construction via mswr::MakeAndInitialize(), so it's safe for all
  // other methods to use the parent_lock_ without checking if it's NULL.
  DCHECK(parent_lock_ == NULL);
  parent_lock_ = parent_lock;

  return S_OK;
}

void PrintDocumentSource::Abort() {
  base::AutoLock lock(*parent_lock_);
  aborted_ = true;
  if (page_count_ready_.IsSignaled()) {
    pages_.clear();
    for (size_t i = 0; i < pages_ready_state_.size(); ++i)
      pages_ready_state_[i]->Broadcast();
  } else {
    DCHECK(pages_.empty() && pages_ready_state_.empty());
  }
}

STDMETHODIMP PrintDocumentSource::GetPreviewPageCollection(
    IPrintDocumentPackageTarget* package_target,
    IPrintPreviewPageCollection** page_collection) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(package_target != NULL);
  DCHECK(page_collection != NULL);

  HRESULT hr = package_target->GetPackageTarget(
      __uuidof(IPrintPreviewDxgiPackageTarget),
      IID_PPV_ARGS(&dxgi_preview_target_));
  if (FAILED(hr)) {
    // TODO(mad): remove once we don't run mixed SDK/OS anymore.
    // The IID changed from one version of the SDK to another, so try the other
    // one in case we are running a build from a different SDK than the one
    // related to the OS version we are running.
    GUID package_target_uuid = kNewPackageTargetGuid;
    if (package_target_uuid == __uuidof(IPrintPreviewDxgiPackageTarget)) {
      package_target_uuid = kOldPackageTargetGuid;
      using_old_preview_interface_ = true;
    }
    hr = package_target->GetPackageTarget(package_target_uuid,
                                          package_target_uuid,
                                          &dxgi_preview_target_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get IPrintPreviewDXGIPackageTarget " << std::hex
                 << hr;
      return hr;
    }
  } else {
    using_old_preview_interface_ = (__uuidof(IPrintPreviewDxgiPackageTarget) ==
                                    kOldPackageTargetGuid);
  }

  mswr::ComPtr<IPrintPreviewPageCollection> preview_page_collection;
  mswr::ComPtr<PrintDocumentSource> print_document_source(this);
  hr = print_document_source.As(&preview_page_collection);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get preview_page_collection " << std::hex << hr;
    return hr;
  }

  hr = preview_page_collection.CopyTo(page_collection);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to copy preview_page_collection " << std::hex << hr;
    return hr;
  }
  return hr;
}

STDMETHODIMP PrintDocumentSource::MakeDocument(
    IInspectable* options,
    IPrintDocumentPackageTarget* package_target) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(options != NULL);
  DCHECK(package_target != NULL);

  mswr::ComPtr<wingfx::Printing::IPrintTaskOptionsCore> print_task_options;
  HRESULT hr = options->QueryInterface(
      wingfx::Printing::IID_IPrintTaskOptionsCore,
      reinterpret_cast<void**>(print_task_options.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI for IPrintTaskOptionsCore " << std::hex << hr;
    return hr;
  }

  // Use the first page's description for the whole document. Page numbers
  // are 1-based in this context.
  // TODO(mad): Check if it would be useful to use per page descriptions.
  wingfx::Printing::PrintPageDescription page_desc = {};
  hr = print_task_options->GetPageDescription(1 /* page */, &page_desc);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to GetPageDescription " << std::hex << hr;
    return hr;
  }

  D2D1_PRINT_CONTROL_PROPERTIES print_control_properties;
  if (page_desc.DpiX > page_desc.DpiY)
    print_control_properties.rasterDPI = static_cast<float>(page_desc.DpiY);
  else
    print_control_properties.rasterDPI = static_cast<float>(page_desc.DpiX);

  // Color space for vector graphics in D2D print control.
  print_control_properties.colorSpace = D2D1_COLOR_SPACE_SRGB;
  print_control_properties.fontSubset = D2D1_PRINT_FONT_SUBSET_MODE_DEFAULT;

  mswr::ComPtr<ID2D1PrintControl> print_control;
  hr = directx_context_.d2d_device->CreatePrintControl(
      directx_context_.wic_factory.Get(),
      package_target,
      print_control_properties,
      print_control.GetAddressOf());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreatePrintControl " << std::hex << hr;
    return hr;
  }

  D2D1_SIZE_F page_size = D2D1::SizeF(page_desc.PageSize.Width,
                                      page_desc.PageSize.Height);

  // Wait for the number of pages to be available.
  // If an abort occured, we'll get 0 and won't enter the loop below.
  size_t page_count = WaitAndGetPageCount();

  mswr::ComPtr<ID2D1GdiMetafile> gdi_metafile;
  for (size_t page = 0; page < page_count; ++page) {
    gdi_metafile.Reset();
    hr = WaitAndGetPage(page, gdi_metafile.GetAddressOf());
    LOG_IF(ERROR, FAILED(hr)) << "Failed to get page's metafile " << std::hex
                              << hr;
    // S_FALSE means we got aborted.
    if (hr == S_FALSE || FAILED(hr))
      break;
    hr = PrintPage(print_control.Get(), gdi_metafile.Get(), page_size);
    if (FAILED(hr))
      break;
  }

  HRESULT close_hr = print_control->Close();
  if (FAILED(close_hr) && SUCCEEDED(hr))
    return close_hr;
  else
    return hr;
}

STDMETHODIMP PrintDocumentSource::Paginate(uint32 page,
                                           IInspectable* options) {
  DVLOG(1) << __FUNCTION__ << ", page = " << page;
  DCHECK(options != NULL);
  // GetPreviewPageCollection must have been successfuly called.
  DCHECK(dxgi_preview_target_.Get() != NULL);

  // Get print settings from PrintTaskOptions for preview, such as page
  // description, which contains page size, imageable area, DPI.
  // TODO(mad): obtain other print settings in the same way, such as ColorMode,
  // NumberOfCopies, etc...
  mswr::ComPtr<wingfx::Printing::IPrintTaskOptionsCore> print_options;
  HRESULT hr = options->QueryInterface(
      wingfx::Printing::IID_IPrintTaskOptionsCore,
      reinterpret_cast<void**>(print_options.GetAddressOf()));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI for IPrintTaskOptionsCore " << std::hex << hr;
    return hr;
  }

  wingfx::Printing::PrintPageDescription page_desc = {};
  hr = print_options->GetPageDescription(1 /* page */, &page_desc);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to GetPageDescription " << std::hex << hr;
    return hr;
  }

  width_ = page_desc.PageSize.Width;
  height_ = page_desc.PageSize.Height;

  hr = dxgi_preview_target_->InvalidatePreview();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to InvalidatePreview " << std::hex << hr;
    return hr;
  }

  size_t page_count = WaitAndGetPageCount();
  // A page_count of 0 means abort...
  if (page_count == 0)
    return S_FALSE;
  hr = dxgi_preview_target_->SetJobPageCount(
           PageCountType::FinalPageCount,
           base::checked_cast<UINT32>(page_count));
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to SetJobPageCount " << std::hex << hr;
    return hr;
  }
  return hr;
}

STDMETHODIMP PrintDocumentSource::MakePage(uint32 job_page,
                                           float width,
                                           float height) {
  DVLOG(1) << __FUNCTION__ << ", width: " << width << ", height: " << height
          << ", job_page: " << job_page;
  DCHECK(width > 0 && height > 0);
  // Paginate must have been called before this.
  if (width_ <= 0.0 || height_ <= 0.0)
    return S_FALSE;

  // When job_page is JOB_PAGE_APPLICATION_DEFINED, it means a new preview
  // begins. TODO(mad): Double check if we need to cancel pending resources.
  if (job_page == JOB_PAGE_APPLICATION_DEFINED)
    job_page = 1;

  winfoundtn::Size preview_size;
  preview_size.Width  = width;
  preview_size.Height = height;
  float scale = width_ / width;

  mswr::ComPtr<ID2D1Factory> factory;
  directx_context_.d2d_device->GetFactory(&factory);

  mswr::ComPtr<ID2D1GdiMetafile> gdi_metafile;
  HRESULT hr = WaitAndGetPage(job_page - 1, gdi_metafile.GetAddressOf());
  LOG_IF(ERROR, FAILED(hr)) << "Failed to get page's metafile " << std::hex
                            << hr;
  // Again, S_FALSE means we got aborted.
  if (hr == S_FALSE || FAILED(hr))
    return hr;

  // We are accessing D3D resources directly without D2D's knowledge, so we
  // must manually acquire the D2D factory lock.
  D2DFactoryAutoLock factory_lock(directx_context_.d2d_factory.Get());

  CD3D11_TEXTURE2D_DESC texture_desc(
      DXGI_FORMAT_B8G8R8A8_UNORM,
      static_cast<UINT32>(ceil(width  * dpi_ / 96)),
      static_cast<UINT32>(ceil(height * dpi_ / 96)),
      1,
      1,
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
      );
  mswr::ComPtr<ID3D11Texture2D> texture;
  hr = directx_context_.d3d_device->CreateTexture2D(
      &texture_desc, NULL, &texture);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create a 2D texture " << std::hex << hr;
    return hr;
  }

  mswr::ComPtr<IDXGISurface> dxgi_surface;
  hr = texture.As<IDXGISurface>(&dxgi_surface);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI for IDXGISurface " << std::hex << hr;
    return hr;
  }

  // D2D device contexts are stateful, and hence a unique device context must
  // be used on each call.
  mswr::ComPtr<ID2D1DeviceContext> d2d_context;
  hr = directx_context_.d2d_device->CreateDeviceContext(
      D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_context);

  d2d_context->SetDpi(dpi_, dpi_);

  mswr::ComPtr<ID2D1Bitmap1> d2dSurfaceBitmap;
  hr = d2d_context->CreateBitmapFromDxgiSurface(dxgi_surface.Get(),
                                                NULL,  // default properties.
                                                &d2dSurfaceBitmap);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateBitmapFromDxgiSurface " << std::hex << hr;
    return hr;
  }

  d2d_context->SetTarget(d2dSurfaceBitmap.Get());
  d2d_context->BeginDraw();
  d2d_context->Clear();
  d2d_context->SetTransform(D2D1::Matrix3x2F(1/scale, 0, 0, 1/scale, 0, 0));
  d2d_context->DrawGdiMetafile(gdi_metafile.Get());

  hr = d2d_context->EndDraw();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to EndDraw " << std::hex << hr;
    return hr;
  }

// TODO(mad): remove once we don't run mixed SDK/OS anymore.
#ifdef __IPrintPreviewDxgiPackageTarget_FWD_DEFINED__
  FLOAT dpi = dpi_;
  if (using_old_preview_interface_) {
    // We compiled with the new API but run on the old OS, so we must cheat
    // and send something that looks like a float but has a UINT32 value.
    *reinterpret_cast<UINT32*>(&dpi) = static_cast<UINT32>(dpi_);
  }
#else
  UINT32 dpi = static_cast<UINT32>(dpi_);
  if (!using_old_preview_interface_) {
    // We compiled with the old API but run on the new OS, so we must cheat
    // and send something that looks like a UINT32 but has a float value.
    *reinterpret_cast<FLOAT*>(&dpi) = dpi_;
  }
#endif  // __IPrintPreviewDxgiPackageTarget_FWD_DEFINED__
  hr = dxgi_preview_target_->DrawPage(job_page, dxgi_surface.Get(), dpi, dpi);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to DrawPage " << std::hex << hr;
    return hr;
  }
  return hr;
}

void PrintDocumentSource::ResetDpi(float dpi) {
  {
    base::AutoLock lock(*parent_lock_);
    if (dpi == dpi_)
      return;
    dpi_ = dpi;
  }
  directx_context_.d2d_context->SetDpi(dpi, dpi);
}

void PrintDocumentSource::SetPageCount(size_t page_count) {
  DCHECK(page_count > 0);
  {
    base::AutoLock lock(*parent_lock_);
    DCHECK(!page_count_ready_.IsSignaled());
    DCHECK(pages_.empty() && pages_ready_state_.empty());

    pages_.resize(page_count);
    pages_ready_state_.resize(page_count);

    for (size_t i = 0; i < page_count; ++i)
      pages_ready_state_[i].reset(new base::ConditionVariable(parent_lock_));
  }
  page_count_ready_.Signal();
}

void PrintDocumentSource::AddPage(size_t page_number,
                                  IStream* metafile_stream) {
  DCHECK(metafile_stream != NULL);
  base::AutoLock lock(*parent_lock_);

  DCHECK(page_count_ready_.IsSignaled());
  DCHECK(page_number < pages_.size());

  pages_[page_number] = metafile_stream;
  pages_ready_state_[page_number]->Signal();
}

HRESULT PrintDocumentSource::PrintPage(ID2D1PrintControl* print_control,
                                       ID2D1GdiMetafile* gdi_metafile,
                                       D2D1_SIZE_F page_size) {
  DVLOG(1) << __FUNCTION__ << ", page_size: (" << page_size.width << ", "
          << page_size.height << ")";
  DCHECK(print_control != NULL);
  DCHECK(gdi_metafile != NULL);

  // D2D device contexts are stateful, and hence a unique device context must
  // be used on each call.
  mswr::ComPtr<ID2D1DeviceContext> d2d_context;
  HRESULT hr = directx_context_.d2d_device->CreateDeviceContext(
      D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2d_context);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateDeviceContext " << std::hex << hr;
    return hr;
  }

  mswr::ComPtr<ID2D1CommandList> print_command_list;
  hr = d2d_context->CreateCommandList(&print_command_list);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateCommandList " << std::hex << hr;
    return hr;
  }

  d2d_context->SetTarget(print_command_list.Get());

  d2d_context->BeginDraw();
  d2d_context->DrawGdiMetafile(gdi_metafile);
  hr = d2d_context->EndDraw();
  LOG_IF(ERROR, FAILED(hr)) << "Failed to EndDraw " << std::hex << hr;

  // Make sure to always close the command list.
  HRESULT close_hr = print_command_list->Close();
  LOG_IF(ERROR, FAILED(close_hr)) << "Failed to close command list " << std::hex
                                  << hr;
  if (SUCCEEDED(hr) && SUCCEEDED(close_hr))
    hr = print_control->AddPage(print_command_list.Get(), page_size, NULL);
  if (FAILED(hr))
    return hr;
  else
    return close_hr;
}

size_t PrintDocumentSource::WaitAndGetPageCount() {
  // Properly protect the wait/access to the page count.
  {
    base::AutoLock lock(*parent_lock_);
    if (aborted_)
      return 0;
    DCHECK(pages_.size() == pages_ready_state_.size());
    if (!pages_.empty())
      return pages_.size();
  }
  page_count_ready_.Wait();
  {
    base::AutoLock lock(*parent_lock_);
    if (!aborted_) {
      DCHECK(pages_.size() == pages_ready_state_.size());
      return pages_.size();
    }
  }
  // A page count of 0 means abort.
  return 0;
}

HRESULT PrintDocumentSource::WaitAndGetPage(size_t page_number,
                                            ID2D1GdiMetafile** gdi_metafile) {
  // Properly protect the wait/access to the page data.
  base::AutoLock lock(*parent_lock_);
  // Make sure we weren't canceled before getting here.
  // And the page count should have been received before we get here too.
  if (aborted_)
    return S_FALSE;

  // We shouldn't be asked for a page until we got the page count.
  DCHECK(page_count_ready_.IsSignaled());
  DCHECK(page_number <= pages_ready_state_.size());
  DCHECK(pages_.size() == pages_ready_state_.size());
  while (!aborted_ && pages_[page_number].Get() == NULL)
    pages_ready_state_[page_number]->Wait();

  // Make sure we weren't aborted while we waited unlocked.
  if (aborted_)
    return S_FALSE;
  DCHECK(page_number < pages_.size());

  mswr::ComPtr<ID2D1Factory> factory;
  directx_context_.d2d_device->GetFactory(&factory);

  mswr::ComPtr<ID2D1Factory1> factory1;
  HRESULT hr = factory.As(&factory1);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to QI for ID2D1Factory1 " << std::hex << hr;
    return hr;
  }

  ULARGE_INTEGER result;
  LARGE_INTEGER seek_pos;
  seek_pos.QuadPart = 0;
  hr = pages_[page_number]->Seek(seek_pos, STREAM_SEEK_SET, &result);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to Seek page stream " << std::hex << hr;
    return hr;
  }

  hr = factory1->CreateGdiMetafile(pages_[page_number].Get(), gdi_metafile);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to CreateGdiMetafile " << std::hex << hr;
    return hr;
  }
  return hr;
}

}  // namespace metro_driver
