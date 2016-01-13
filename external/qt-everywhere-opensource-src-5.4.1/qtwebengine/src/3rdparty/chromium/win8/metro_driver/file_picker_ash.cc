// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "win8/metro_driver/file_picker_ash.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/metro.h"
#include "base/win/scoped_comptr.h"
#include "ui/metro_viewer/metro_viewer_messages.h"
#include "win8/metro_driver/chrome_app_view_ash.h"
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

}  // namespace

FilePickerSessionBase::FilePickerSessionBase(ChromeAppViewAsh* app_view,
                                             const base::string16& title,
                                             const base::string16& filter,
                                             const base::FilePath& default_path)
    : app_view_(app_view),
      title_(title),
      filter_(filter),
      default_path_(default_path),
      success_(false) {
}

bool FilePickerSessionBase::Run() {
  if (!DoFilePicker())
    return false;
  return success_;
}

bool FilePickerSessionBase::DoFilePicker() {
  // The file picker will fail if spawned from a snapped application,
  // so let's attempt to unsnap first if we're in that state.
  HRESULT hr = ChromeAppViewAsh::Unsnap();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to unsnap for file picker, error 0x" << hr;
    return false;
  }
  hr = StartFilePicker();
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to start file picker, error 0x"
               << std::hex << hr;
    return false;
  }
  return true;
}

OpenFilePickerSession::OpenFilePickerSession(
    ChromeAppViewAsh* app_view,
    const base::string16& title,
    const base::string16& filter,
    const base::FilePath& default_path,
    bool allow_multi_select)
    : FilePickerSessionBase(app_view, title, filter, default_path),
      allow_multi_select_(allow_multi_select) {
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

        result_ = path_str;
        success_ = true;
      }
    } else {
      LOG(ERROR) << "NULL IStorageItem";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }
  app_view_->OnOpenFileCompleted(this, success_);
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
        success_ = true;
        // The code below has been copied from the
        // SelectFileDialogImpl::RunOpenMultiFileDialog function in
        // select_file_dialog_win.cc.
        // TODO(ananta)
        // Consolidate this into a common place.
        const wchar_t* selection = result.c_str();
        std::vector<base::FilePath> files;

        while (*selection) {  // Empty string indicates end of list.
          files.push_back(base::FilePath(selection));
          // Skip over filename and null-terminator.
          selection += files.back().value().length() + 1;
        }
        if (files.empty()) {
          success_ = false;
        } else if (files.size() == 1) {
          // When there is one file, it contains the path and filename.
          filenames_ = files;
        } else if (files.size() > 1) {
          // Otherwise, the first string is the path, and the remainder are
          // filenames.
          std::vector<base::FilePath>::iterator path = files.begin();
          for (std::vector<base::FilePath>::iterator file = path + 1;
               file != files.end(); ++file) {
            filenames_.push_back(path->Append(*file));
          }
        }
      }
    } else {
      LOG(ERROR) << "NULL StorageFileVectorCollection";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }
  app_view_->OnOpenFileCompleted(this, success_);
  return S_OK;
}

HRESULT OpenFilePickerSession::StartFilePicker() {
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

  if (filter_.empty()) {
    hr = filter->Append(mswrw::HStringReference(L"*").Get());
    if (FAILED(hr))
      return hr;
  } else {
    // The filter is a concatenation of zero terminated string pairs,
    // where each pair is {description, extension}. The concatenation ends
    // with a zero length string - e.g. a double zero terminator.
    const wchar_t* walk = filter_.c_str();
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
  if (allow_multi_select_) {
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

SaveFilePickerSession::SaveFilePickerSession(
    ChromeAppViewAsh* app_view,
    const MetroViewerHostMsg_SaveAsDialogParams& params)
    : FilePickerSessionBase(app_view,
                            params.title,
                            params.filter,
                            params.suggested_name),
      filter_index_(params.filter_index) {
}

int SaveFilePickerSession::filter_index() const {
  // TODO(ananta)
  // Add support for returning the correct filter index. This does not work in
  // regular Chrome metro on trunk as well.
  // BUG: https://code.google.com/p/chromium/issues/detail?id=172704
  return filter_index_;
}

HRESULT SaveFilePickerSession::StartFilePicker() {
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

  if (!filter_.empty()) {
    // The filter is a concatenation of zero terminated string pairs,
    // where each pair is {description, extension list}. The concatenation ends
    // with a zero length string - e.g. a double zero terminator.
    const wchar_t* walk = filter_.c_str();
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

  if (!default_path_.empty()) {
    base::string16 file_part = default_path_.BaseName().value();
    // If the suggested_name is a root directory, then don't set it as the
    // suggested name.
    if (file_part.size() == 1 && file_part[0] == L'\\')
      file_part.clear();
    hr = picker->put_SuggestedFileName(
        mswrw::HStringReference(file_part.c_str()).Get());
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
        result_ = path_str;
        success_ = true;
      }
    } else {
      LOG(ERROR) << "NULL IStorageItem";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }
  app_view_->OnSaveFileCompleted(this, success_);
  return S_OK;
}

FolderPickerSession::FolderPickerSession(ChromeAppViewAsh* app_view,
                                         const base::string16& title)
    : FilePickerSessionBase(app_view, title, L"", base::FilePath()) {}

HRESULT FolderPickerSession::StartFilePicker() {
  mswrw::HStringReference class_name(
      RuntimeClass_Windows_Storage_Pickers_FolderPicker);

  // Create the folder picker.
  mswr::ComPtr<winstorage::Pickers::IFolderPicker> picker;
  HRESULT hr = ::Windows::Foundation::ActivateInstance(
      class_name.Get(), picker.GetAddressOf());
  CheckHR(hr);

  // Set the file type filter
  mswr::ComPtr<winfoundtn::Collections::IVector<HSTRING>> filter;
  hr = picker->get_FileTypeFilter(filter.GetAddressOf());
  if (FAILED(hr))
    return hr;

  hr = filter->Append(mswrw::HStringReference(L"*").Get());
  if (FAILED(hr))
    return hr;

  mswr::ComPtr<FolderPickerAsyncOp> completion;
  hr = picker->PickSingleFolderAsync(&completion);
  if (FAILED(hr))
    return hr;

  // Create the callback method.
  typedef winfoundtn::IAsyncOperationCompletedHandler<
      winstorage::StorageFolder*> HandlerDoneType;
  mswr::ComPtr<HandlerDoneType> handler(mswr::Callback<HandlerDoneType>(
      this, &FolderPickerSession::FolderPickerDone));
  DCHECK(handler.Get() != NULL);
  hr = completion->put_Completed(handler.Get());
  return hr;
}

HRESULT FolderPickerSession::FolderPickerDone(FolderPickerAsyncOp* async,
                                              AsyncStatus status) {
  if (status == Completed) {
    mswr::ComPtr<winstorage::IStorageFolder> folder;
    HRESULT hr = async->GetResults(folder.GetAddressOf());

    if (folder) {
      mswr::ComPtr<winstorage::IStorageItem> storage_item;
      if (SUCCEEDED(hr))
        hr = folder.As(&storage_item);

      mswrw::HString file_path;
      if (SUCCEEDED(hr))
        hr = storage_item->get_Path(file_path.GetAddressOf());

      if (SUCCEEDED(hr)) {
        base::string16 path_str = MakeStdWString(file_path.Get());
        result_ = path_str;
        success_ = true;
      }
    } else {
      LOG(ERROR) << "NULL IStorageItem";
    }
  } else {
    LOG(ERROR) << "Unexpected async status " << static_cast<int>(status);
  }
  app_view_->OnFolderPickerCompleted(this, success_);
  return S_OK;
}

