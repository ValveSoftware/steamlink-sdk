// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_DRIVER_PRINT_DOCUMENT_SOURCE_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_PRINT_DOCUMENT_SOURCE_H_

#include <documentsource.h>
#include <printpreview.h>
#include <windows.graphics.printing.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/waitable_event.h"
#include "win8/metro_driver/winrt_utils.h"

// Hack to be removed once we don't need to build with an SDK earlier than
// 8441 where the name of the interface has been changed.
// TODO(mad): remove once we don't run mixed SDK/OS anymore.
#ifndef __IPrintPreviewDxgiPackageTarget_FWD_DEFINED__
typedef IPrintPreviewDXGIPackageTarget IPrintPreviewDxgiPackageTarget;
#endif


namespace base {
class Lock;
};  // namespace base

namespace metro_driver {

// This class is given to Metro as a source for print documents.
// The methodless IPrintDocumentSource interface is used to identify it as such.
// Then, the other interfaces are used to generate preview and print documents.
// It also exposes a few methods for the print handler to control the document.
class PrintDocumentSource
    : public mswr::RuntimeClass<
          mswr::RuntimeClassFlags<mswr::WinRtClassicComMix>,
          wingfx::Printing::IPrintDocumentSource,
          IPrintDocumentPageSource,
          IPrintPreviewPageCollection> {
 public:
  // A set of interfaces for the DirectX context that our parent owns
  // and that don't need to change from document to document.
  struct DirectXContext {
    DirectXContext() {}
    DirectXContext(ID3D11Device1* device_3d,
                   ID2D1Factory1* factory_2d,
                   ID2D1Device* device_2d,
                   ID2D1DeviceContext* context_2d,
                   IWICImagingFactory2* factory_wic)
        : d3d_device(device_3d),
          d2d_factory(factory_2d),
          d2d_device(device_2d),
          d2d_context(context_2d),
          wic_factory(factory_wic) {
    }
    DirectXContext(const DirectXContext& other)
        : d3d_device(other.d3d_device),
          d2d_factory(other.d2d_factory),
          d2d_device(other.d2d_device),
          d2d_context(other.d2d_context),
          wic_factory(other.wic_factory) {
    }
    mswr::ComPtr<ID3D11Device1> d3d_device;
    mswr::ComPtr<ID2D1Factory1> d2d_factory;
    mswr::ComPtr<ID2D1Device> d2d_device;
    mswr::ComPtr<ID2D1DeviceContext> d2d_context;
    mswr::ComPtr<IWICImagingFactory2> wic_factory;
  };

  // Construction / Initialization.
  explicit PrintDocumentSource();
  HRESULT RuntimeClassInitialize(const DirectXContext& directx_context,
                                 base::Lock* parent_lock);
  // Aborts any pending asynchronous operation.
  void Abort();

  // classic COM interface IPrintDocumentPageSource methods
  STDMETHOD(GetPreviewPageCollection) (
      IPrintDocumentPackageTarget* package_target,
      IPrintPreviewPageCollection** page_collection);
  STDMETHOD(MakeDocument)(IInspectable* options,
                          IPrintDocumentPackageTarget* package_target);

  // classic COM interface IPrintPreviewPageCollection methods
  STDMETHOD(Paginate)(uint32 page, IInspectable* options);
  STDMETHOD(MakePage)(uint32 desired_page, float width, float height);

  // If the screen DPI changes, we must be warned here.
  void ResetDpi(float dpi);

  // When the page count is known, this is called and we can setup our data.
  void SetPageCount(size_t page_count);

  // Every time a page is ready, this is called and we can read the data if
  // we were waiting for it, or store it for later use.
  void AddPage(size_t page_number, IStream* metafile_stream);

 private:
  // Print the page given in the metafile stream to the given print control.
  HRESULT PrintPage(ID2D1PrintControl* print_control,
                    ID2D1GdiMetafile* metafile_stream,
                    D2D1_SIZE_F pageSize);

  // Helper function to wait for the page count to be ready.
  // Returns 0 when aborted.
  size_t WaitAndGetPageCount();

  // Helper function to wait for a given page to be ready.
  // Returns S_FALSE when aborted.
  HRESULT WaitAndGetPage(size_t page_number,
                         ID2D1GdiMetafile** metafile_stream);

  DirectXContext directx_context_;

  // Once page data is available, it's added to this vector.
  std::vector<mswr::ComPtr<IStream>> pages_;

  // When page count is set, the size of this vector is set to that number.
  // Then, every time page data is added to pages_, the associated condition
  // variable in this vector is signaled. This is only filled when we receive
  // the page count, so we must wait on page_count_ready_ before accessing
  // the content of this vector.
  std::vector<scoped_ptr<base::ConditionVariable> > pages_ready_state_;

  // This event is signaled when we receive a page count from Chrome. We should
  // not receive any page data before the count, so we can check this event
  // while waiting for pages too, in case we ask for page data before we got
  // the count, so before we properly initialized pages_ready_state_.
  base::WaitableEvent page_count_ready_;

  // The preview target interface set from within GetPreviewPageCollection
  // but used from within MakePage.
  mswr::ComPtr<IPrintPreviewDxgiPackageTarget> dxgi_preview_target_;

  // Our parent's lock (to make sure it is initialized and destroyed early
  // enough), which we use to protect access to our data members.
  base::Lock* parent_lock_;

  // The width/height requested in Paginate and used in MakePage.
  // TODO(mad): Height is currently not used, and width is only used for
  // scaling. We need to add a way to specify width and height when we request
  // pages from Chrome.
  float height_;
  float width_;

  // The DPI is set by Windows and we need to give it to DirectX.
  float dpi_;

  // A flag identiying that we have been aborted. Needed to properly handle
  // asynchronous callbacks.
  bool aborted_;

  // TODO(mad): remove once we don't run mixed SDK/OS anymore.
  bool using_old_preview_interface_;
};

}  // namespace metro_driver

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_PRINT_DOCUMENT_SOURCE_H_
