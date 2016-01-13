// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_

#include <objidl.h>
#include <shlobj.h>
#include <string>
#include <vector>

// Win8 SDK compatibility, see http://goo.gl/fufvl for more information.
// "Note: This interface has been renamed IDataObjectAsyncCapability."
// If we're building on pre-8 we define it to its old name. It's documented as
// being binary compatible.
#ifndef __IDataObjectAsyncCapability_FWD_DEFINED__
#define IDataObjectAsyncCapability IAsyncOperation
#endif

#include "base/memory/scoped_vector.h"
#include "base/win/scoped_comptr.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector2d.h"

namespace ui {

class DataObjectImpl : public DownloadFileObserver,
                       public IDataObject,
                       public IDataObjectAsyncCapability {
 public:
  class Observer {
   public:
    virtual void OnWaitForData() = 0;
    virtual void OnDataObjectDisposed() = 0;
   protected:
    virtual ~Observer() { }
  };

  DataObjectImpl();

  // Accessors.
  void set_observer(Observer* observer) { observer_ = observer; }
  void set_in_drag_loop(bool in_drag_loop) { in_drag_loop_ = in_drag_loop; }

  // Number of known formats.
  size_t size() const { return contents_.size(); }

  // DownloadFileObserver implementation:
  virtual void OnDownloadCompleted(const base::FilePath& file_path);
  virtual void OnDownloadAborted();

  // IDataObject implementation:
  HRESULT __stdcall GetData(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall GetDataHere(FORMATETC* format_etc, STGMEDIUM* medium);
  HRESULT __stdcall QueryGetData(FORMATETC* format_etc);
  HRESULT __stdcall GetCanonicalFormatEtc(
      FORMATETC* format_etc, FORMATETC* result);
  HRESULT __stdcall SetData(
      FORMATETC* format_etc, STGMEDIUM* medium, BOOL should_release);
  HRESULT __stdcall EnumFormatEtc(
      DWORD direction, IEnumFORMATETC** enumerator);
  HRESULT __stdcall DAdvise(FORMATETC* format_etc, DWORD advf,
                            IAdviseSink* sink, DWORD* connection);
  HRESULT __stdcall DUnadvise(DWORD connection);
  HRESULT __stdcall EnumDAdvise(IEnumSTATDATA** enumerator);

  // IDataObjectAsyncCapability implementation:
  HRESULT __stdcall EndOperation(
      HRESULT result, IBindCtx* reserved, DWORD effects);
  HRESULT __stdcall GetAsyncMode(BOOL* is_op_async);
  HRESULT __stdcall InOperation(BOOL* in_async_op);
  HRESULT __stdcall SetAsyncMode(BOOL do_op_async);
  HRESULT __stdcall StartOperation(IBindCtx* reserved);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 private:
  // FormatEtcEnumerator only likes us for our StoredDataMap typedef.
  friend class FormatEtcEnumerator;
  friend class OSExchangeDataProviderWin;

  virtual ~DataObjectImpl();

  void StopDownloads();

  // Removes from contents_ the first data that matches |format|.
  void RemoveData(const FORMATETC& format);

  // Our internal representation of stored data & type info.
  struct StoredDataInfo {
    FORMATETC format_etc;
    STGMEDIUM* medium;
    bool owns_medium;
    scoped_refptr<DownloadFileProvider> downloader;

    StoredDataInfo(const FORMATETC& format_etc, STGMEDIUM* medium)
        : format_etc(format_etc), medium(medium), owns_medium(true) {}

    ~StoredDataInfo() {
      if (owns_medium) {
        ReleaseStgMedium(medium);
        delete medium;
      }
      if (downloader.get())
        downloader->Stop();
    }
  };

  typedef ScopedVector<StoredDataInfo> StoredData;
  StoredData contents_;

  base::win::ScopedComPtr<IDataObject> source_object_;

  bool is_aborting_;
  bool in_drag_loop_;
  bool in_async_mode_;
  bool async_operation_started_;
  Observer* observer_;
};

class UI_BASE_EXPORT OSExchangeDataProviderWin
    : public OSExchangeData::Provider {
 public:
  // Returns true if source has plain text that is a valid url.
  static bool HasPlainTextURL(IDataObject* source);

  // Returns true if source has plain text that is a valid URL and sets url to
  // that url.
  static bool GetPlainTextURL(IDataObject* source, GURL* url);

  static DataObjectImpl* GetDataObjectImpl(const OSExchangeData& data);
  static IDataObject* GetIDataObject(const OSExchangeData& data);
  static IDataObjectAsyncCapability* GetIAsyncOperation(
      const OSExchangeData& data);

  explicit OSExchangeDataProviderWin(IDataObject* source);
  OSExchangeDataProviderWin();

  virtual ~OSExchangeDataProviderWin();

  IDataObject* data_object() const { return data_.get(); }
  IDataObjectAsyncCapability* async_operation() const { return data_.get(); }

  // OSExchangeData::Provider methods.
  virtual Provider* Clone() const;
  virtual void MarkOriginatedFromRenderer();
  virtual bool DidOriginateFromRenderer() const;
  virtual void SetString(const base::string16& data);
  virtual void SetURL(const GURL& url, const base::string16& title);
  virtual void SetFilename(const base::FilePath& path);
  virtual void SetFilenames(const std::vector<FileInfo>& filenames);
  virtual void SetPickledData(const OSExchangeData::CustomFormat& format,
                              const Pickle& data);
  virtual void SetFileContents(const base::FilePath& filename,
                               const std::string& file_contents);
  virtual void SetHtml(const base::string16& html, const GURL& base_url);

  virtual bool GetString(base::string16* data) const;
  virtual bool GetURLAndTitle(OSExchangeData::FilenameToURLPolicy policy,
                              GURL* url,
                              base::string16* title) const;
  virtual bool GetFilename(base::FilePath* path) const;
  virtual bool GetFilenames(std::vector<FileInfo>* filenames) const;
  virtual bool GetPickledData(const OSExchangeData::CustomFormat& format,
                              Pickle* data) const;
  virtual bool GetFileContents(base::FilePath* filename,
                               std::string* file_contents) const;
  virtual bool GetHtml(base::string16* html, GURL* base_url) const;
  virtual bool HasString() const;
  virtual bool HasURL(OSExchangeData::FilenameToURLPolicy policy) const;
  virtual bool HasFile() const;
  virtual bool HasFileContents() const;
  virtual bool HasHtml() const;
  virtual bool HasCustomFormat(
      const OSExchangeData::CustomFormat& format) const;
  virtual void SetDownloadFileInfo(
      const OSExchangeData::DownloadFileInfo& download_info);
  virtual void SetDragImage(const gfx::ImageSkia& image,
                            const gfx::Vector2d& cursor_offset) OVERRIDE;
  virtual const gfx::ImageSkia& GetDragImage() const OVERRIDE;
  virtual const gfx::Vector2d& GetDragImageOffset() const OVERRIDE;

 private:
  scoped_refptr<DataObjectImpl> data_;
  base::win::ScopedComPtr<IDataObject> source_object_;

  // Drag image and offset data. Only used for Ash.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d drag_image_offset_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeDataProviderWin);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_WIN_H_
