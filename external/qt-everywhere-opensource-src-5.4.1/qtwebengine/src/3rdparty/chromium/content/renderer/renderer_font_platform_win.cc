// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_font_platform_win.h"

#include <dwrite.h>
#include <string>
#include <vector>
#include <wrl/implements.h>
#include <wrl/wrappers/corewrappers.h>

#include "base/debug/alias.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/win/iat_patch_function.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"

namespace {

namespace mswr = Microsoft::WRL;
namespace mswrw = Microsoft::WRL::Wrappers;

class FontCollectionLoader
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontCollectionLoader> {
 public:
  // IDWriteFontCollectionLoader methods.
  virtual HRESULT STDMETHODCALLTYPE
      CreateEnumeratorFromKey(IDWriteFactory* factory,
                              void const* key,
                              UINT32 key_size,
                              IDWriteFontFileEnumerator** file_enumerator);

  static HRESULT Initialize(IDWriteFactory* factory);

  UINT32 GetFontMapSize();

  std::wstring GetFontNameFromKey(UINT32 idx);

  bool LoadFontListFromRegistry();

  FontCollectionLoader() {};
  virtual ~FontCollectionLoader() {};

 private:
  mswr::ComPtr<IDWriteFontFileLoader> file_loader_;

  std::vector<std::wstring> reg_fonts_;
};

mswr::ComPtr<FontCollectionLoader> g_font_loader;

class FontFileStream
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileStream> {
 public:
  // IDWriteFontFileStream methods.
  virtual HRESULT STDMETHODCALLTYPE
  ReadFileFragment(void const** fragment_start,
                   UINT64 file_offset,
                   UINT64 fragment_size,
                   void** context) {
    if (!memory_.get() || !memory_->IsValid())
      return E_FAIL;

    *fragment_start = static_cast<BYTE const*>(memory_->data()) +
                      static_cast<size_t>(file_offset);
    *context = NULL;
    return S_OK;
  }

  virtual void STDMETHODCALLTYPE ReleaseFileFragment(void* context) {}

  virtual HRESULT STDMETHODCALLTYPE GetFileSize(UINT64* file_size) {
    if (!memory_.get() || !memory_->IsValid())
      return E_FAIL;

    *file_size = memory_->length();
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetLastWriteTime(UINT64* last_write_time) {
    if (!memory_.get() || !memory_->IsValid())
      return E_FAIL;

    // According to MSDN article http://goo.gl/rrSYzi the "last modified time"
    // is used by DirectWrite font selection algorithms to determine whether
    // one font resource is more up to date than another one.
    // So by returning 0 we are assuming that it will treat all fonts to be
    // equally up to date.
    // TODO(shrikant): We should further investigate this.
    *last_write_time = 0;
    return S_OK;
  }

  FontFileStream::FontFileStream() : font_key_(0) {
  };

  HRESULT RuntimeClassInitialize(UINT32 font_key) {
    base::FilePath path;
    PathService::Get(base::DIR_WINDOWS_FONTS, &path);
    path = path.Append(g_font_loader->GetFontNameFromKey(font_key).c_str());
    memory_.reset(new base::MemoryMappedFile());

    // Put some debug information on stack.
    WCHAR font_name[256];
    path.value().copy(font_name, arraysize(font_name));
    base::debug::Alias(font_name);

    if (!memory_->Initialize(path)) {
      memory_.reset();
      return E_FAIL;
    }

    font_key_ = font_key;
    return S_OK;
  }

  virtual ~FontFileStream() {}

  UINT32 font_key_;
  scoped_ptr<base::MemoryMappedFile> memory_;
};

class FontFileLoader
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileLoader> {
 public:
  // IDWriteFontFileLoader methods.
  virtual HRESULT STDMETHODCALLTYPE
  CreateStreamFromKey(void const* ref_key,
                      UINT32 ref_key_size,
                      IDWriteFontFileStream** stream) {
    if (ref_key_size != sizeof(UINT32))
      return E_FAIL;

    UINT32 font_key = *static_cast<const UINT32*>(ref_key);
    mswr::ComPtr<FontFileStream> font_stream;
    HRESULT hr = mswr::MakeAndInitialize<FontFileStream>(&font_stream,
                                                         font_key);
    if (SUCCEEDED(hr)) {
      *stream = font_stream.Detach();
      return S_OK;
    }
    return E_FAIL;
  }

  FontFileLoader() {}
  virtual ~FontFileLoader() {}
};

class FontFileEnumerator
    : public mswr::RuntimeClass<mswr::RuntimeClassFlags<mswr::ClassicCom>,
                                IDWriteFontFileEnumerator> {
 public:
  // IDWriteFontFileEnumerator methods.
  virtual HRESULT STDMETHODCALLTYPE MoveNext(BOOL* has_current_file) {
    *has_current_file = FALSE;

    if (current_file_)
      current_file_.ReleaseAndGetAddressOf();

    if (font_idx_ < g_font_loader->GetFontMapSize()) {
      HRESULT hr =
          factory_->CreateCustomFontFileReference(&font_idx_,
                                                  sizeof(UINT32),
                                                  file_loader_.Get(),
                                                  current_file_.GetAddressOf());
      DCHECK(SUCCEEDED(hr));
      *has_current_file = TRUE;
      font_idx_++;
    }
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE
  GetCurrentFontFile(IDWriteFontFile** font_file) {
    if (!current_file_) {
      *font_file = NULL;
      return E_FAIL;
    }

    *font_file = current_file_.Detach();
    return S_OK;
  }

  FontFileEnumerator(const void* keys,
                     UINT32 buffer_size,
                     IDWriteFactory* factory,
                     IDWriteFontFileLoader* file_loader)
      : factory_(factory), file_loader_(file_loader), font_idx_(0) {}

  virtual ~FontFileEnumerator() {}

  mswr::ComPtr<IDWriteFactory> factory_;
  mswr::ComPtr<IDWriteFontFile> current_file_;
  mswr::ComPtr<IDWriteFontFileLoader> file_loader_;
  UINT32 font_idx_;
};

// IDWriteFontCollectionLoader methods.
HRESULT STDMETHODCALLTYPE FontCollectionLoader::CreateEnumeratorFromKey(
    IDWriteFactory* factory,
    void const* key,
    UINT32 key_size,
    IDWriteFontFileEnumerator** file_enumerator) {
  *file_enumerator = mswr::Make<FontFileEnumerator>(
                         key, key_size, factory, file_loader_.Get()).Detach();
  return S_OK;
}

// static
HRESULT FontCollectionLoader::Initialize(IDWriteFactory* factory) {
  DCHECK(g_font_loader == NULL);

  g_font_loader = mswr::Make<FontCollectionLoader>();
  if (!g_font_loader) {
    DCHECK(FALSE);
    return E_FAIL;
  }

  CHECK(g_font_loader->LoadFontListFromRegistry());

  g_font_loader->file_loader_ = mswr::Make<FontFileLoader>().Detach();

  factory->RegisterFontFileLoader(g_font_loader->file_loader_.Get());
  factory->RegisterFontCollectionLoader(g_font_loader.Get());

  return S_OK;
}

UINT32 FontCollectionLoader::GetFontMapSize() {
  return reg_fonts_.size();
}

std::wstring FontCollectionLoader::GetFontNameFromKey(UINT32 idx) {
  DCHECK(idx < reg_fonts_.size());
  return reg_fonts_[idx];
}

bool FontCollectionLoader::LoadFontListFromRegistry() {
  const wchar_t kFontsRegistry[] =
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
  CHECK(reg_fonts_.empty());
  base::win::RegKey regkey;
  if (regkey.Open(HKEY_LOCAL_MACHINE, kFontsRegistry, KEY_READ) !=
      ERROR_SUCCESS) {
    return false;
  }

  std::wstring name;
  std::wstring value;
  for (DWORD idx = 0; idx < regkey.GetValueCount(); idx++) {
    if (regkey.GetValueNameAt(idx, &name) == ERROR_SUCCESS &&
        regkey.ReadValue(name.c_str(), &value) == ERROR_SUCCESS) {
      base::FilePath path(value.c_str());
      // We need to check if file name is the only component that exists,
      // we will ignore all other registry entries.
      std::vector<base::FilePath::StringType> components;
      path.GetComponents(&components);
      if (components.size() == 1) {
        reg_fonts_.push_back(value.c_str());
      }
    }
  }
  return true;
}

}  // namespace

namespace content {

mswr::ComPtr<IDWriteFontCollection> g_font_collection;

IDWriteFontCollection* GetCustomFontCollection(IDWriteFactory* factory) {
  if (g_font_collection.Get() != NULL)
    return g_font_collection.Get();

  base::TimeTicks start_tick = base::TimeTicks::Now();

  FontCollectionLoader::Initialize(factory);

  HRESULT hr = factory->CreateCustomFontCollection(
      g_font_loader.Get(), NULL, 0, g_font_collection.GetAddressOf());

  base::TimeDelta time_delta = base::TimeTicks::Now() - start_tick;
  int64 delta = time_delta.ToInternalValue();
  base::debug::Alias(&delta);
  UINT32 size = g_font_loader->GetFontMapSize();
  base::debug::Alias(&size);

  CHECK(SUCCEEDED(hr));
  CHECK(g_font_collection.Get() != NULL);

  return g_font_collection.Get();
}

}  // namespace content
