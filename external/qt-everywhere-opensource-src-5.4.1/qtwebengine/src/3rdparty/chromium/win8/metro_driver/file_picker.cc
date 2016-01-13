// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/file_picker.h"

#include <windows.storage.pickers.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/metro.h"
#include "base/win/scoped_comptr.h"
#include "win8/metro_driver/chrome_app_view.h"
#include "win8/metro_driver/winrt_utils.h"

namespace {

namespace winstorage = ABI::Windows::Storage;
typedef winfoundtn::Collections::IVector<HSTRING> StringVectorItf;

// TODO(siggi): Complete this implementation and move it to a common place.
class StringVectorImpl : public mswr::RuntimeClass<StringVectorItf> {
 public:
  ~StringVectorImpl() {
    std::for_each(strings_.begin(), strings_.end(), ::WindowsDeleteString);
  }

  HRESULT RuntimeClassInitialize(const std::vector<base::string16>& list) {
    for (size_t i = 0; i < list.size(); ++i)
      strings_.push_back(MakeHString(list[i]));

    return S_OK;
  }

  // IVector<HSTRING> implementation.
  STDMETHOD(GetAt)(unsigned index, HSTRING* item) {
    if (index >= strings_.size())
      return E_INVALIDARG;

    return ::WindowsDuplicateString(strings_[index], item);
  }
  STDMETHOD(get_Size)(unsigned *size) {
    if (strings_.size() > UINT_MAX)
      return E_UNEXPECTED;
    *size = static_cast<unsigned>(strings_.size());
    return S_OK;
  }
  STDMETHOD(GetView)(winfoundtn::Collections::IVectorView<HSTRING> **view) {
    return E_NOTIMPL;
  }
  STDMETHOD(IndexOf)(HSTRING value, unsigned *index, boolean *found) {
    return E_NOTIMPL;
  }

  // write methods
  STDMETHOD(SetAt)(unsigned index, HSTRING item) {
    return E_NOTIMPL;
  }
  STDMETHOD(InsertAt)(unsigned index, HSTRING item) {
    return E_NOTIMPL;
  }
  STDMETHOD(RemoveAt)(unsigned index) {
    return E_NOTIMPL;
  }
  STDMETHOD(Append)(HSTRING item) {
    return E_NOTIMPL;
  }
  STDMETHOD(RemoveAtEnd)() {
    return E_NOTIMPL;
  }
  STDMETHOD(Clear)() {
    return E_NOTIMPL;
  }

 private:
  std::vector<HSTRING> strings_;
};

class FilePickerSessionBase {
 public:
  // Creates a file picker for open_file_name.
  explicit FilePickerSessionBase(OPENFILENAME* open_file_name);

  // Runs the picker, returns true on success.
  bool Run();

 protected:
  // Creates, configures and starts a file picker.
  // If the HRESULT returned is a failure code the file picker has not started,
  // so no callbacks should be expected.
  virtual HRESULT StartFilePicker() = 0;

  // The parameters to our picker.
  OPENFILENAME* open_file_name_;
  // The event Run waits on.
  base::WaitableEvent event_;
  // True iff a file picker has successfully finished.
  bool success_;

 private:
  // Initiate a file picker, must be called on the metro dispatcher's thread.
  void DoFilePicker();

  DISALLOW_COPY_AND_ASSIGN(FilePickerSessionBase);
};

class OpenFilePickerSession : public FilePickerSessionBase {
 public:
  explicit OpenFilePickerSession(OPENFILENAME* open_file_name);

 private:
  virtual HRESULT StartFilePicker() OVERRIDE;

  typedef winfoundtn::IAsyncOperation<winstorage::StorageFile*>
      SingleFileAsyncOp;
  typedef winfoundtn::Collections::IVectorView<
      winstorage::StorageFile*> StorageFileVectorCollection;
  typedef winfoundtn::IAsyncOperation<StorageFileVectorCollection*>
      MultiFileAsyncOp;

  // Called asynchronously when a single file picker is done.
  HRESULT SinglePickerDone(SingleFileAsyncOp* async, AsyncStatus status);

  // Called asynchronously when a multi file picker is done.
  HRESULT MultiPickerDone(MultiFileAsyncOp* async, AsyncStatus status);

  // Composes a multi-file result string suitable for returning to a
  // from a storage file collection.
  static HRESULT ComposeMultiFileResult(StorageFileVectorCollection* files,
                                        base::string16* result);
 private:
  DISALLOW_COPY_AND_ASSIGN(OpenFilePickerSession);
};

class SaveFilePickerSession : public FilePickerSessionBase {
 public:
  explicit SaveFilePickerSession(OPENFILENAME* open_file_name);

 private:
  virtual HRESULT StartFilePicker() OVERRIDE;

  typedef winfoundtn::IAsyncOperation<winstorage::StorageFile*>
      SaveFileAsyncOp;

  // Called asynchronously when the save file picker is done.
  HRESULT FilePickerDone(SaveFileAsyncOp* async, AsyncStatus status);
};

FilePickerSessionBase::FilePickerSessionBase(OPENFILENAME* open_file_name)
    : open_file_name_(open_file_name),
      event_(true, false),
      success_(false) {
}

bool FilePickerSessionBase::Run() {
  DCHECK(globals.appview_msg_loop != NULL);

  // Post the picker request over to the metro thread.
  bool posted = globals.appview_msg_loop->PostTask(FROM_HERE,
      base::Bind(&FilePickerSessionBase::DoFilePicker, base::Unretained(this)));
  if (!posted)
    return false;

  // Wait for the file picker to complete.
  event_.Wait();

  return success_;
}

void FilePickerSessionBase::DoFilePicker() {
  // The file picker will fail if spawned from a snapped application,
  // so let's attempt to unsnap first if we're in that state.
  HRESULT hr = ChromeAppView::Unsnap();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to unsnap for file picker, error 0x" << hr;
  }

  if (SUCCEEDED(hr))
    hr = StartFilePicker();

  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start file picker, error 0x"
               << std::hex << hr;

    event_.Signal();
  }
}

OpenFilePickerSession::OpenFilePickerSession(OPENFILENAME* open_file_name)
    : FilePickerSessionBase(open_file_name) {
}

HRESULT OpenFilePickerSession::SinglePickerDone(SingleFileAsyncOp* async,
                                                AsyncStatus status) {
  if (status == Completed) {
    mswr::ComPtr<winstorage::IStorageFile> file;
    HRESULT hr = async->GetResults(file.GetAddressOf());

    if (file) {
      mswr::ComPtr<winstorage::IStorageItem> storage_item;
      if (SUCCEEDED(hr))
        hr = file.As(&storage_item);

      mswrw::HString file_path;
      if (SUCCEEDED(hr))
        hr = storage_item->get_Path(file_path.GetAddressOf());

      if (SUCCEEDED(hr)) {
        UINT32 path_len = 0;
        const wchar_t* path_str =
            ::WindowsGetStringRawBuffer(file_path.Get(), &path_len);

        // If the selected file name is longer than the supplied buffer,
        // we return false as per GetOpenFileName documentation.
        if (path_len < open_file_name_->nMaxFile) {
          base::wcslcpy(open_file_name_->lpstrFile,
                        path_str,
                        open_file_name_->nMaxFile);
          success_ = true;
        }
      }
    } else {
      LOG(ERROR) << "NULL IStorageItem";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }

  event_.Signal();

  return S_OK;
}

HRESULT OpenFilePickerSession::MultiPickerDone(MultiFileAsyncOp* async,
                                                 AsyncStatus status) {
  if (status == Completed) {
    mswr::ComPtr<StorageFileVectorCollection> files;
    HRESULT hr = async->GetResults(files.GetAddressOf());

    if (files) {
      base::string16 result;
      if (SUCCEEDED(hr))
        hr = ComposeMultiFileResult(files.Get(), &result);

      if (SUCCEEDED(hr)) {
        if (result.size() + 1 < open_file_name_->nMaxFile) {
          // Because the result has embedded nulls, we must memcpy.
          memcpy(open_file_name_->lpstrFile,
                 result.c_str(),
                 (result.size() + 1) * sizeof(result[0]));
          success_ = true;
        }
      }
    } else {
      LOG(ERROR) << "NULL StorageFileVectorCollection";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }

  event_.Signal();

  return S_OK;
}

HRESULT OpenFilePickerSession::StartFilePicker() {
  DCHECK(globals.appview_msg_loop->BelongsToCurrentThread());
  DCHECK(open_file_name_ != NULL);

  mswrw::HStringReference class_name(
      RuntimeClass_Windows_Storage_Pickers_FileOpenPicker);

  // Create the file picker.
  mswr::ComPtr<winstorage::Pickers::IFileOpenPicker> picker;
  HRESULT hr = ::Windows::Foundation::ActivateInstance(
      class_name.Get(), picker.GetAddressOf());
  CheckHR(hr);

  // Set the file type filter
  mswr::ComPtr<winfoundtn::Collections::IVector<HSTRING>> filter;
  hr = picker->get_FileTypeFilter(filter.GetAddressOf());
  if (FAILED(hr))
    return hr;

  if (open_file_name_->lpstrFilter == NULL) {
    hr = filter->Append(mswrw::HStringReference(L"*").Get());
    if (FAILED(hr))
      return hr;
  } else {
    // The filter is a concatenation of zero terminated string pairs,
    // where each pair is {description, extension}. The concatenation ends
    // with a zero length string - e.g. a double zero terminator.
    const wchar_t* walk = open_file_name_->lpstrFilter;
    while (*walk != L'\0') {
      // Walk past the description.
      walk += wcslen(walk) + 1;

      // We should have an extension, but bail on malformed filters.
      if (*walk == L'\0')
        break;

      // There can be a single extension, or a list of semicolon-separated ones.
      std::vector<base::string16> extensions_win32_style;
      size_t extension_count = Tokenize(walk, L";", &extensions_win32_style);
      DCHECK_EQ(extension_count, extensions_win32_style.size());

      // Metro wants suffixes only, not patterns.
      mswrw::HString extension;
      for (size_t i = 0; i < extensions_win32_style.size(); ++i) {
        if (extensions_win32_style[i] == L"*.*") {
          // The wildcard filter is "*" for Metro. The string "*.*" produces
          // an "invalid parameter" error.
          hr = extension.Set(L"*");
        } else {
          // Metro wants suffixes only, not patterns.
          base::string16 ext =
              base::FilePath(extensions_win32_style[i]).Extension();
          if ((ext.size() < 2) ||
              (ext.find_first_of(L"*?") != base::string16::npos)) {
            continue;
          }
          hr = extension.Set(ext.c_str());
        }
        if (SUCCEEDED(hr))
          hr = filter->Append(extension.Get());
        if (FAILED(hr))
          return hr;
      }

      // Walk past the extension.
      walk += wcslen(walk) + 1;
    }
  }

  // Spin up a single or multi picker as appropriate.
  if (open_file_name_->Flags & OFN_ALLOWMULTISELECT) {
    mswr::ComPtr<MultiFileAsyncOp> completion;
    hr = picker->PickMultipleFilesAsync(&completion);
    if (FAILED(hr))
      return hr;

    // Create the callback method.
    typedef winfoundtn::IAsyncOperationCompletedHandler<
        StorageFileVectorCollection*> HandlerDoneType;
    mswr::ComPtr<HandlerDoneType> handler(mswr::Callback<HandlerDoneType>(
        this, &OpenFilePickerSession::MultiPickerDone));
    DCHECK(handler.Get() != NULL);
    hr = completion->put_Completed(handler.Get());

    return hr;
  } else {
    mswr::ComPtr<SingleFileAsyncOp> completion;
    hr = picker->PickSingleFileAsync(&completion);
    if (FAILED(hr))
      return hr;

    // Create the callback method.
    typedef winfoundtn::IAsyncOperationCompletedHandler<
        winstorage::StorageFile*> HandlerDoneType;
    mswr::ComPtr<HandlerDoneType> handler(mswr::Callback<HandlerDoneType>(
        this, &OpenFilePickerSession::SinglePickerDone));
    DCHECK(handler.Get() != NULL);
    hr = completion->put_Completed(handler.Get());

    return hr;
  }
}

HRESULT OpenFilePickerSession::ComposeMultiFileResult(
    StorageFileVectorCollection* files, base::string16* result) {
  DCHECK(files != NULL);
  DCHECK(result != NULL);

  // Empty the output string.
  result->clear();

  unsigned int num_files = 0;
  HRESULT hr = files->get_Size(&num_files);
  if (FAILED(hr))
    return hr;

  // Make sure we return an error on an empty collection.
  if (num_files == 0) {
    DLOG(ERROR) << "Empty collection on input.";
    return E_UNEXPECTED;
  }

  // This stores the base path that should be the parent of all the files.
  base::FilePath base_path;

  // Iterate through the collection and append the file paths to the result.
  for (unsigned int i = 0; i < num_files; ++i) {
    mswr::ComPtr<winstorage::IStorageFile> file;
    hr = files->GetAt(i, file.GetAddressOf());
    if (FAILED(hr))
      return hr;

    mswr::ComPtr<winstorage::IStorageItem> storage_item;
    hr = file.As(&storage_item);
    if (FAILED(hr))
      return hr;

    mswrw::HString file_path_str;
    hr = storage_item->get_Path(file_path_str.GetAddressOf());
    if (FAILED(hr))
      return hr;

    base::FilePath file_path(MakeStdWString(file_path_str.Get()));
    if (base_path.empty()) {
      DCHECK(result->empty());
      base_path = file_path.DirName();

      // Append the path, including the terminating zero.
      // We do this only for the first file.
      result->append(base_path.value().c_str(), base_path.value().size() + 1);
    }
    DCHECK(!result->empty());
    DCHECK(!base_path.empty());
    DCHECK(base_path == file_path.DirName());

    // Append the base name, including the terminating zero.
    base::FilePath base_name = file_path.BaseName();
    result->append(base_name.value().c_str(), base_name.value().size() + 1);
  }

  DCHECK(!result->empty());

  return S_OK;
}

SaveFilePickerSession::SaveFilePickerSession(OPENFILENAME* open_file_name)
    : FilePickerSessionBase(open_file_name) {
}

HRESULT SaveFilePickerSession::StartFilePicker() {
  DCHECK(globals.appview_msg_loop->BelongsToCurrentThread());
  DCHECK(open_file_name_ != NULL);

  mswrw::HStringReference class_name(
      RuntimeClass_Windows_Storage_Pickers_FileSavePicker);

  // Create the file picker.
  mswr::ComPtr<winstorage::Pickers::IFileSavePicker> picker;
  HRESULT hr = ::Windows::Foundation::ActivateInstance(
      class_name.Get(), picker.GetAddressOf());
  CheckHR(hr);

  typedef winfoundtn::Collections::IMap<HSTRING, StringVectorItf*>
      StringVectorMap;
  mswr::ComPtr<StringVectorMap> choices;
  hr = picker->get_FileTypeChoices(choices.GetAddressOf());
  if (FAILED(hr))
    return hr;

  if (open_file_name_->lpstrFilter) {
    // The filter is a concatenation of zero terminated string pairs,
    // where each pair is {description, extension list}. The concatenation ends
    // with a zero length string - e.g. a double zero terminator.
    const wchar_t* walk = open_file_name_->lpstrFilter;
    while (*walk != L'\0') {
      mswrw::HString description;
      hr = description.Set(walk);
      if (FAILED(hr))
        return hr;

      // Walk past the description.
      walk += wcslen(walk) + 1;

      // We should have an extension, but bail on malformed filters.
      if (*walk == L'\0')
        break;

      // There can be a single extension, or a list of semicolon-separated ones.
      std::vector<base::string16> extensions_win32_style;
      size_t extension_count = Tokenize(walk, L";", &extensions_win32_style);
      DCHECK_EQ(extension_count, extensions_win32_style.size());

      // Metro wants suffixes only, not patterns.  Also, metro does not support
      // the all files ("*") pattern in the save picker.
      std::vector<base::string16> extensions;
      for (size_t i = 0; i < extensions_win32_style.size(); ++i) {
        base::string16 ext =
            base::FilePath(extensions_win32_style[i]).Extension();
        if ((ext.size() < 2) ||
            (ext.find_first_of(L"*?") != base::string16::npos))
          continue;
        extensions.push_back(ext);
      }

      if (!extensions.empty()) {
        // Convert to a Metro collection class.
        mswr::ComPtr<StringVectorItf> list;
        hr = mswr::MakeAndInitialize<StringVectorImpl>(
            list.GetAddressOf(), extensions);
        if (FAILED(hr))
          return hr;

        // Finally set the filter.
        boolean replaced = FALSE;
        hr = choices->Insert(description.Get(), list.Get(), &replaced);
        if (FAILED(hr))
          return hr;
        DCHECK_EQ(FALSE, replaced);
      }

      // Walk past the extension(s).
      walk += wcslen(walk) + 1;
    }
  }

  // The save picker requires at least one choice.  Callers are strongly advised
  // to provide sensible choices.  If none were given, fallback to .dat.
  uint32 num_choices = 0;
  hr = choices->get_Size(&num_choices);
  if (FAILED(hr))
    return hr;

  if (num_choices == 0) {
    mswrw::HString description;
    // TODO(grt): Get a properly translated string.  This can't be done from
    // within metro_driver.  Consider preprocessing the filter list in Chrome
    // land to ensure it has this entry if all others are patterns.  In that
    // case, this whole block of code can be removed.
    hr = description.Set(L"Data File");
    if (FAILED(hr))
      return hr;

    mswr::ComPtr<StringVectorItf> list;
    hr = mswr::MakeAndInitialize<StringVectorImpl>(
        list.GetAddressOf(), std::vector<base::string16>(1, L".dat"));
    if (FAILED(hr))
      return hr;

    boolean replaced = FALSE;
    hr = choices->Insert(description.Get(), list.Get(), &replaced);
    if (FAILED(hr))
      return hr;
    DCHECK_EQ(FALSE, replaced);
  }

  if (open_file_name_->lpstrFile != NULL) {
    hr = picker->put_SuggestedFileName(
        mswrw::HStringReference(
            const_cast<const wchar_t*>(open_file_name_->lpstrFile)).Get());
    if (FAILED(hr))
      return hr;
  }

  mswr::ComPtr<SaveFileAsyncOp> completion;
  hr = picker->PickSaveFileAsync(&completion);
  if (FAILED(hr))
    return hr;

  // Create the callback method.
  typedef winfoundtn::IAsyncOperationCompletedHandler<
      winstorage::StorageFile*> HandlerDoneType;
  mswr::ComPtr<HandlerDoneType> handler(mswr::Callback<HandlerDoneType>(
      this, &SaveFilePickerSession::FilePickerDone));
  DCHECK(handler.Get() != NULL);
  hr = completion->put_Completed(handler.Get());

  return hr;
}

HRESULT SaveFilePickerSession::FilePickerDone(SaveFileAsyncOp* async,
                                              AsyncStatus status) {
  if (status == Completed) {
    mswr::ComPtr<winstorage::IStorageFile> file;
    HRESULT hr = async->GetResults(file.GetAddressOf());

    if (file) {
      mswr::ComPtr<winstorage::IStorageItem> storage_item;
      if (SUCCEEDED(hr))
        hr = file.As(&storage_item);

      mswrw::HString file_path;
      if (SUCCEEDED(hr))
        hr = storage_item->get_Path(file_path.GetAddressOf());

      if (SUCCEEDED(hr)) {
        base::string16 path_str = MakeStdWString(file_path.Get());

        // If the selected file name is longer than the supplied buffer,
        // we return false as per GetOpenFileName documentation.
        if (path_str.size() < open_file_name_->nMaxFile) {
          base::wcslcpy(open_file_name_->lpstrFile,
                        path_str.c_str(),
                        open_file_name_->nMaxFile);
          success_ = true;
        }
      }
    } else {
      LOG(ERROR) << "NULL IStorageItem";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }

  event_.Signal();

  return S_OK;
}

}  // namespace

BOOL MetroGetOpenFileName(OPENFILENAME* open_file_name) {
  OpenFilePickerSession session(open_file_name);

  return session.Run();
}

BOOL MetroGetSaveFileName(OPENFILENAME* open_file_name) {
  SaveFilePickerSession session(open_file_name);

  return session.Run();
}
