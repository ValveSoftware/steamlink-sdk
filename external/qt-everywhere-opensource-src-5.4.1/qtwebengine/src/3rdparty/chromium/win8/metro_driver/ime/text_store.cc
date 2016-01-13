// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/ime/text_store.h"

#include <algorithm>

#include "base/win/scoped_variant.h"
#include "ui/base/win/atl_module.h"
#include "win8/metro_driver/ime/input_scope.h"
#include "win8/metro_driver/ime/text_store_delegate.h"

namespace metro_driver {
namespace {

// We support only one view.
const TsViewCookie kViewCookie = 1;

}  // namespace

TextStore::TextStore()
    : text_store_acp_sink_mask_(0),
      window_handle_(NULL),
      delegate_(NULL),
      committed_size_(0),
      selection_start_(0),
      selection_end_(0),
      edit_flag_(false),
      current_lock_type_(0),
      category_manager_(NULL),
      display_attribute_manager_(NULL),
      input_scope_(NULL) {
}

TextStore::~TextStore() {
}

// static
scoped_refptr<TextStore> TextStore::Create(
    HWND window_handle,
    const std::vector<InputScope>& input_scopes,
    TextStoreDelegate* delegate) {
  if (!delegate) {
    LOG(ERROR) << "|delegate| must be non-NULL.";
    return scoped_refptr<TextStore>();
  }
  base::win::ScopedComPtr<ITfCategoryMgr> category_manager;
  HRESULT hr = category_manager.CreateInstance(CLSID_TF_CategoryMgr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to initialize CategoryMgr. hr = " << hr;
    return scoped_refptr<TextStore>();
  }
  base::win::ScopedComPtr<ITfDisplayAttributeMgr> display_attribute_manager;
  hr = display_attribute_manager.CreateInstance(CLSID_TF_DisplayAttributeMgr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to initialize DisplayAttributeMgr. hr = " << hr;
    return scoped_refptr<TextStore>();
  }
  base::win::ScopedComPtr<ITfInputScope> input_scope =
      CreteInputScope(input_scopes);
  if (!input_scope) {
    LOG(ERROR) << "Failed to initialize InputScope.";
    return scoped_refptr<TextStore>();
  }

  ui::win::CreateATLModuleIfNeeded();
  CComObject<TextStore>* object = NULL;
  hr = CComObject<TextStore>::CreateInstance(&object);
  if (FAILED(hr)) {
    LOG(ERROR) << "CComObject<TextStore>::CreateInstance failed. hr = "
               << hr;
    return scoped_refptr<TextStore>();
  }
  object->Initialize(window_handle,
                     category_manager,
                     display_attribute_manager,
                     input_scope,
                     delegate);
  return scoped_refptr<TextStore>(object);
}

void TextStore::Initialize(HWND window_handle,
                           ITfCategoryMgr* category_manager,
                           ITfDisplayAttributeMgr* display_attribute_manager,
                           ITfInputScope* input_scope,
                           TextStoreDelegate* delegate) {
  window_handle_ = window_handle;
  category_manager_ = category_manager;
  display_attribute_manager_ = display_attribute_manager;
  input_scope_ = input_scope;
  delegate_ = delegate;
}


STDMETHODIMP TextStore::AdviseSink(REFIID iid,
                                   IUnknown* unknown,
                                   DWORD mask) {
  if (!IsEqualGUID(iid, IID_ITextStoreACPSink))
    return E_INVALIDARG;
  if (text_store_acp_sink_) {
    if (text_store_acp_sink_.IsSameObject(unknown)) {
      text_store_acp_sink_mask_ = mask;
      return S_OK;
    } else {
      return CONNECT_E_ADVISELIMIT;
    }
  }
  if (FAILED(text_store_acp_sink_.QueryFrom(unknown)))
    return E_UNEXPECTED;
  text_store_acp_sink_mask_ = mask;

  return S_OK;
}

STDMETHODIMP TextStore::FindNextAttrTransition(
    LONG acp_start,
    LONG acp_halt,
    ULONG num_filter_attributes,
    const TS_ATTRID* filter_attributes,
    DWORD flags,
    LONG* acp_next,
    BOOL* found,
    LONG* found_offset) {
  if (!acp_next || !found || !found_offset)
    return E_INVALIDARG;
  // We don't support any attributes.
  // So we always return "not found".
  *acp_next = 0;
  *found = FALSE;
  *found_offset = 0;
  return S_OK;
}

STDMETHODIMP TextStore::GetACPFromPoint(TsViewCookie view_cookie,
                                        const POINT* point,
                                        DWORD flags,
                                        LONG* acp) {
  NOTIMPLEMENTED();
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  return E_NOTIMPL;
}

STDMETHODIMP TextStore::GetActiveView(TsViewCookie* view_cookie) {
  if (!view_cookie)
    return E_INVALIDARG;
  // We support only one view.
  *view_cookie = kViewCookie;
  return S_OK;
}

STDMETHODIMP TextStore::GetEmbedded(LONG acp_pos,
                                    REFGUID service,
                                    REFIID iid,
                                    IUnknown** unknown) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  if (!unknown)
    return E_INVALIDARG;
  *unknown = NULL;
  return E_NOTIMPL;
}

STDMETHODIMP TextStore::GetEndACP(LONG* acp) {
  if (!acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *acp = static_cast<LONG>(string_buffer_.size());
  return S_OK;
}

STDMETHODIMP TextStore::GetFormattedText(LONG acp_start,
                                         LONG acp_end,
                                         IDataObject** data_object) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TextStore::GetScreenExt(TsViewCookie view_cookie, RECT* rect) {
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!rect)
    return E_INVALIDARG;

  // {0, 0, 0, 0} means that the document rect is not currently displayed.
  SetRect(rect, 0, 0, 0, 0);

  RECT client_rect = {};
  if (!GetClientRect(window_handle_, &client_rect))
    return E_FAIL;
  POINT left_top = {client_rect.left, client_rect.top};
  POINT right_bottom = {client_rect.right, client_rect.bottom};
  if (!ClientToScreen(window_handle_, &left_top))
    return E_FAIL;
  if (!ClientToScreen(window_handle_, &right_bottom))
    return E_FAIL;

  rect->left = left_top.x;
  rect->top = left_top.y;
  rect->right = right_bottom.x;
  rect->bottom = right_bottom.y;
  return S_OK;
}

STDMETHODIMP TextStore::GetSelection(ULONG selection_index,
                                     ULONG selection_buffer_size,
                                     TS_SELECTION_ACP* selection_buffer,
                                     ULONG* fetched_count) {
  if (!selection_buffer)
    return E_INVALIDARG;
  if (!fetched_count)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  *fetched_count = 0;
  if ((selection_buffer_size > 0) &&
      ((selection_index == 0) || (selection_index == TS_DEFAULT_SELECTION))) {
    selection_buffer[0].acpStart = selection_start_;
    selection_buffer[0].acpEnd = selection_end_;
    selection_buffer[0].style.ase = TS_AE_END;
    selection_buffer[0].style.fInterimChar = FALSE;
    *fetched_count = 1;
  }
  return S_OK;
}

STDMETHODIMP TextStore::GetStatus(TS_STATUS* status) {
  if (!status)
    return E_INVALIDARG;

  status->dwDynamicFlags = 0;
  // We use transitory contexts and we don't support hidden text.
  status->dwStaticFlags = TS_SS_TRANSITORY | TS_SS_NOHIDDENTEXT;

  return S_OK;
}

STDMETHODIMP TextStore::GetText(LONG acp_start,
                                LONG acp_end,
                                wchar_t* text_buffer,
                                ULONG text_buffer_size,
                                ULONG* text_buffer_copied,
                                TS_RUNINFO* run_info_buffer,
                                ULONG run_info_buffer_size,
                                ULONG* run_info_buffer_copied,
                                LONG* next_acp) {
  if (!text_buffer_copied || !run_info_buffer_copied)
    return E_INVALIDARG;
  if (!text_buffer && text_buffer_size != 0)
    return E_INVALIDARG;
  if (!run_info_buffer && run_info_buffer_size != 0)
    return E_INVALIDARG;
  if (!next_acp)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TF_E_NOLOCK;
  const LONG string_buffer_size = static_cast<LONG>(string_buffer_.size());
  if (acp_end == -1)
    acp_end = string_buffer_size;
  if (!((0 <= acp_start) &&
        (acp_start <= acp_end) &&
        (acp_end <= string_buffer_size))) {
    return TF_E_INVALIDPOS;
  }
  acp_end = std::min(acp_end, acp_start + static_cast<LONG>(text_buffer_size));
  *text_buffer_copied = acp_end - acp_start;

  const base::string16& result =
      string_buffer_.substr(acp_start, *text_buffer_copied);
  for (size_t i = 0; i < result.size(); ++i)
    text_buffer[i] = result[i];

  if (run_info_buffer_size) {
    run_info_buffer[0].uCount = *text_buffer_copied;
    run_info_buffer[0].type = TS_RT_PLAIN;
    *run_info_buffer_copied = 1;
  }

  *next_acp = acp_end;
  return S_OK;
}

STDMETHODIMP TextStore::GetTextExt(TsViewCookie view_cookie,
                                   LONG acp_start,
                                   LONG acp_end,
                                   RECT* rect,
                                   BOOL* clipped) {
  if (!rect || !clipped)
    return E_INVALIDARG;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  if (!HasReadLock())
    return TS_E_NOLOCK;
  if (!((static_cast<LONG>(committed_size_) <= acp_start) &&
       (acp_start <= acp_end) &&
       (acp_end <= static_cast<LONG>(string_buffer_.size())))) {
    return TS_E_INVALIDPOS;
  }

  // According to a behavior of notepad.exe and wordpad.exe, top left corner of
  // rect indicates a first character's one, and bottom right corner of rect
  // indicates a last character's one.
  // We use RECT instead of gfx::Rect since left position may be bigger than
  // right position when composition has multiple lines.
  RECT result;
  RECT tmp_rect;
  const uint32 start_pos = acp_start - committed_size_;
  const uint32 end_pos = acp_end - committed_size_;

  if (start_pos == end_pos) {
    // According to MSDN document, if |acp_start| and |acp_end| are equal it is
    // OK to just return E_INVALIDARG.
    // http://msdn.microsoft.com/en-us/library/ms538435
    // But when using Pinin IME of Windows 8, this method is called with the
    // equal values of |acp_start| and |acp_end|. So we handle this condition.
    if (start_pos == 0) {
      if (delegate_->GetCompositionCharacterBounds(0, &tmp_rect)) {
        tmp_rect.right = tmp_rect.right;
        result = tmp_rect;
      } else if (string_buffer_.size() == committed_size_) {
        result = delegate_->GetCaretBounds();
      } else {
        return TS_E_NOLAYOUT;
      }
    } else if (delegate_->GetCompositionCharacterBounds(start_pos - 1,
                                                        &tmp_rect)) {
      tmp_rect.left = tmp_rect.right;
      result = tmp_rect;
    } else {
      return TS_E_NOLAYOUT;
    }
  } else {
    if (delegate_->GetCompositionCharacterBounds(start_pos, &tmp_rect)) {
      result = tmp_rect;
      if (delegate_->GetCompositionCharacterBounds(end_pos - 1, &tmp_rect)) {
        result.right = tmp_rect.right;
        result.bottom = tmp_rect.bottom;
      } else {
        // We may not be able to get the last character bounds, so we use the
        // first character bounds instead of returning TS_E_NOLAYOUT.
      }
    } else {
      // Hack for PPAPI flash. PPAPI flash does not support GetCaretBounds, so
      // it's better to return previous caret rectangle instead.
      // TODO(nona, kinaba): Remove this hack.
      if (start_pos == 0)
        result = delegate_->GetCaretBounds();
      else
        return TS_E_NOLAYOUT;
    }
  }

  *rect =  result;
  *clipped = FALSE;
  return S_OK;
}

STDMETHODIMP TextStore::GetWnd(TsViewCookie view_cookie,
                               HWND* window_handle) {
  if (!window_handle)
    return E_INVALIDARG;
  if (view_cookie != kViewCookie)
    return E_INVALIDARG;
  *window_handle = window_handle_;
  return S_OK;
}

STDMETHODIMP TextStore::InsertEmbedded(DWORD flags,
                                       LONG acp_start,
                                       LONG acp_end,
                                       IDataObject* data_object,
                                       TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TextStore::InsertEmbeddedAtSelection(DWORD flags,
                                                  IDataObject* data_object,
                                                  LONG* acp_start,
                                                  LONG* acp_end,
                                                  TS_TEXTCHANGE* change) {
  // We don't support any embedded objects.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP TextStore::InsertTextAtSelection(DWORD flags,
                                              const wchar_t* text_buffer,
                                              ULONG text_buffer_size,
                                              LONG* acp_start,
                                              LONG* acp_end,
                                              TS_TEXTCHANGE* text_change) {
  const LONG start_pos = selection_start_;
  const LONG end_pos = selection_end_;
  const LONG new_end_pos = start_pos + text_buffer_size;

  if (flags & TS_IAS_QUERYONLY) {
    if (!HasReadLock())
      return TS_E_NOLOCK;
    if (acp_start)
      *acp_start = start_pos;
    if (acp_end)
      *acp_end = end_pos;
    return S_OK;
  }

  if (!HasReadWriteLock())
    return TS_E_NOLOCK;
  if (!text_buffer)
    return E_INVALIDARG;

  DCHECK_LE(start_pos, end_pos);
  string_buffer_ = string_buffer_.substr(0, start_pos) +
                   base::string16(text_buffer, text_buffer + text_buffer_size) +
                   string_buffer_.substr(end_pos);
  if (acp_start)
    *acp_start = start_pos;
  if (acp_end)
    *acp_end = new_end_pos;
  if (text_change) {
    text_change->acpStart = start_pos;
    text_change->acpOldEnd = end_pos;
    text_change->acpNewEnd = new_end_pos;
  }
  selection_start_ = start_pos;
  selection_end_ = new_end_pos;
  return S_OK;
}

STDMETHODIMP TextStore::QueryInsert(
    LONG acp_test_start,
    LONG acp_test_end,
    ULONG text_size,
    LONG* acp_result_start,
    LONG* acp_result_end) {
  if (!acp_result_start || !acp_result_end || acp_test_start > acp_test_end)
    return E_INVALIDARG;
  const LONG committed_size = static_cast<LONG>(committed_size_);
  const LONG buffer_size = static_cast<LONG>(string_buffer_.size());
  *acp_result_start = std::min(std::max(committed_size, acp_test_start),
                               buffer_size);
  *acp_result_end = std::min(std::max(committed_size, acp_test_end),
                             buffer_size);
  return S_OK;
}

STDMETHODIMP TextStore::QueryInsertEmbedded(const GUID* service,
                                               const FORMATETC* format,
                                               BOOL* insertable) {
  if (!format)
    return E_INVALIDARG;
  // We don't support any embedded objects.
  if (insertable)
    *insertable = FALSE;
  return S_OK;
}

STDMETHODIMP TextStore::RequestAttrsAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

STDMETHODIMP TextStore::RequestAttrsTransitioningAtPosition(
    LONG acp_pos,
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer,
    DWORD flags) {
  // We don't support any document attributes.
  // This method just returns S_OK, and the subsequently called
  // RetrieveRequestedAttrs() returns 0 as the number of supported attributes.
  return S_OK;
}

STDMETHODIMP TextStore::RequestLock(DWORD lock_flags, HRESULT* result) {
  if (!text_store_acp_sink_.get())
    return E_FAIL;
  if (!result)
    return E_INVALIDARG;

  if (current_lock_type_ != 0) {
    if (lock_flags & TS_LF_SYNC) {
      // Can't lock synchronously.
      *result = TS_E_SYNCHRONOUS;
      return S_OK;
    }
    // Queue the lock request.
    lock_queue_.push_back(lock_flags & TS_LF_READWRITE);
    *result = TS_S_ASYNC;
    return S_OK;
  }

  // Lock
  current_lock_type_ = (lock_flags & TS_LF_READWRITE);

  edit_flag_ = false;
  const uint32 last_committed_size = committed_size_;

  // Grant the lock.
  *result = text_store_acp_sink_->OnLockGranted(current_lock_type_);

  // Unlock
  current_lock_type_ = 0;

  // Handles the pending lock requests.
  while (!lock_queue_.empty()) {
    current_lock_type_ = lock_queue_.front();
    lock_queue_.pop_front();
    text_store_acp_sink_->OnLockGranted(current_lock_type_);
    current_lock_type_ = 0;
  }

  if (!edit_flag_)
    return S_OK;

  // If the text store is edited in OnLockGranted(), we may need to call
  // TextStoreDelegate::ConfirmComposition() or
  // TextStoreDelegate::SetComposition().
  const uint32 new_committed_size = committed_size_;
  const base::string16& new_committed_string =
      string_buffer_.substr(last_committed_size,
                            new_committed_size - last_committed_size);
  const base::string16& composition_string =
      string_buffer_.substr(new_committed_size);

  // If there is new committed string, calls
  // TextStoreDelegate::ConfirmComposition().
  if ((!new_committed_string.empty()))
    delegate_->OnTextCommitted(new_committed_string);

  // Calls TextInputClient::SetCompositionText().
  std::vector<metro_viewer::UnderlineInfo> underlines = underlines_;
  // Adjusts the offset.
  for (size_t i = 0; i < underlines_.size(); ++i) {
    underlines[i].start_offset -= new_committed_size;
    underlines[i].end_offset -= new_committed_size;
  }
  int32 selection_start = 0;
  int32 selection_end = 0;
  if (selection_start_ >= new_committed_size)
    selection_start = selection_start_ - new_committed_size;
  if (selection_end_ >= new_committed_size)
    selection_end = selection_end_ - new_committed_size;
  delegate_->OnCompositionChanged(
        composition_string, selection_start, selection_end, underlines);

  // If there is no composition string, clear the text store status.
  // And call OnSelectionChange(), OnLayoutChange(), and OnTextChange().
  if ((composition_string.empty()) && (new_committed_size != 0)) {
    string_buffer_.clear();
    committed_size_ = 0;
    selection_start_ = 0;
    selection_end_ = 0;
    if (text_store_acp_sink_mask_ & TS_AS_SEL_CHANGE)
      text_store_acp_sink_->OnSelectionChange();
    if (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE)
      text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
    if (text_store_acp_sink_mask_ & TS_AS_TEXT_CHANGE) {
      TS_TEXTCHANGE textChange;
      textChange.acpStart = 0;
      textChange.acpOldEnd = new_committed_size;
      textChange.acpNewEnd = 0;
      text_store_acp_sink_->OnTextChange(0, &textChange);
    }
  }

  return S_OK;
}

STDMETHODIMP TextStore::RequestSupportedAttrs(
    DWORD /* flags */,  // Seems that we should ignore this.
    ULONG attribute_buffer_size,
    const TS_ATTRID* attribute_buffer) {
  if (!attribute_buffer)
    return E_INVALIDARG;
  if (!input_scope_)
    return E_FAIL;
  // We support only input scope attribute.
  for (size_t i = 0; i < attribute_buffer_size; ++i) {
    if (IsEqualGUID(GUID_PROP_INPUTSCOPE, attribute_buffer[i]))
      return S_OK;
  }
  return E_FAIL;
}

STDMETHODIMP TextStore::RetrieveRequestedAttrs(
    ULONG attribute_buffer_size,
    TS_ATTRVAL* attribute_buffer,
    ULONG* attribute_buffer_copied) {
  if (!attribute_buffer_copied)
    return E_INVALIDARG;
  *attribute_buffer_copied = 0;
  if (!attribute_buffer)
    return E_INVALIDARG;
  if (!input_scope_)
    return E_UNEXPECTED;
  // We support only input scope attribute.
  *attribute_buffer_copied = 0;
  if (attribute_buffer_size == 0)
    return S_OK;

  attribute_buffer[0].dwOverlapId = 0;
  attribute_buffer[0].idAttr = GUID_PROP_INPUTSCOPE;
  attribute_buffer[0].varValue.vt = VT_UNKNOWN;
  attribute_buffer[0].varValue.punkVal = input_scope_.get();
  attribute_buffer[0].varValue.punkVal->AddRef();
  *attribute_buffer_copied = 1;
  return S_OK;
}

STDMETHODIMP TextStore::SetSelection(
    ULONG selection_buffer_size,
    const TS_SELECTION_ACP* selection_buffer) {
  if (!HasReadWriteLock())
    return TF_E_NOLOCK;
  if (selection_buffer_size > 0) {
    const LONG start_pos = selection_buffer[0].acpStart;
    const LONG end_pos = selection_buffer[0].acpEnd;
    if (!((static_cast<LONG>(committed_size_) <= start_pos) &&
          (start_pos <= end_pos) &&
          (end_pos <= static_cast<LONG>(string_buffer_.size())))) {
      return TF_E_INVALIDPOS;
    }
    selection_start_ = start_pos;
    selection_end_ = end_pos;
  }
  return S_OK;
}

STDMETHODIMP TextStore::SetText(DWORD flags,
                                LONG acp_start,
                                LONG acp_end,
                                const wchar_t* text_buffer,
                                ULONG text_buffer_size,
                                TS_TEXTCHANGE* text_change) {
  if (!HasReadWriteLock())
    return TS_E_NOLOCK;
  if (!((static_cast<LONG>(committed_size_) <= acp_start) &&
        (acp_start <= acp_end) &&
        (acp_end <= static_cast<LONG>(string_buffer_.size())))) {
    return TS_E_INVALIDPOS;
  }

  TS_SELECTION_ACP selection;
  selection.acpStart = acp_start;
  selection.acpEnd = acp_end;
  selection.style.ase = TS_AE_NONE;
  selection.style.fInterimChar = 0;

  HRESULT ret;
  ret = SetSelection(1, &selection);
  if (ret != S_OK)
    return ret;

  TS_TEXTCHANGE change;
  ret = InsertTextAtSelection(0, text_buffer, text_buffer_size,
                              &acp_start, &acp_end, &change);
  if (ret != S_OK)
    return ret;

  if (text_change)
    *text_change = change;

  return S_OK;
}

STDMETHODIMP TextStore::UnadviseSink(IUnknown* unknown) {
  if (!text_store_acp_sink_.IsSameObject(unknown))
    return CONNECT_E_NOCONNECTION;
  text_store_acp_sink_.Release();
  text_store_acp_sink_mask_ = 0;
  return S_OK;
}

STDMETHODIMP TextStore::OnStartComposition(
    ITfCompositionView* composition_view,
    BOOL* ok) {
  if (ok)
    *ok = TRUE;
  return S_OK;
}

STDMETHODIMP TextStore::OnUpdateComposition(
    ITfCompositionView* composition_view,
    ITfRange* range) {
  return S_OK;
}

STDMETHODIMP TextStore::OnEndComposition(
    ITfCompositionView* composition_view) {
  return S_OK;
}

STDMETHODIMP TextStore::OnEndEdit(ITfContext* context,
                                  TfEditCookie read_only_edit_cookie,
                                  ITfEditRecord* edit_record) {
  if (!context || !edit_record)
    return E_INVALIDARG;
  if (!GetCompositionStatus(context, read_only_edit_cookie, &committed_size_,
                            &underlines_)) {
    return S_OK;
  }
  edit_flag_ = true;
  return S_OK;
}

bool TextStore::GetDisplayAttribute(TfGuidAtom guid_atom,
                                    TF_DISPLAYATTRIBUTE* attribute) {
  GUID guid;
  if (FAILED(category_manager_->GetGUID(guid_atom, &guid)))
    return false;

  base::win::ScopedComPtr<ITfDisplayAttributeInfo> display_attribute_info;
  if (FAILED(display_attribute_manager_->GetDisplayAttributeInfo(
          guid, display_attribute_info.Receive(), NULL))) {
    return false;
  }
  return SUCCEEDED(display_attribute_info->GetAttributeInfo(attribute));
}

bool TextStore::GetCompositionStatus(
    ITfContext* context,
    const TfEditCookie read_only_edit_cookie,
    uint32* committed_size,
    std::vector<metro_viewer::UnderlineInfo>* undelines) {
  DCHECK(context);
  DCHECK(committed_size);
  DCHECK(undelines);
  const GUID* rgGuids[2] = {&GUID_PROP_COMPOSING, &GUID_PROP_ATTRIBUTE};
  base::win::ScopedComPtr<ITfReadOnlyProperty> track_property;
  if (FAILED(context->TrackProperties(rgGuids, 2, NULL, 0,
                                      track_property.Receive()))) {
    return false;
  }

  *committed_size = 0;
  undelines->clear();
  base::win::ScopedComPtr<ITfRange> start_to_end_range;
  base::win::ScopedComPtr<ITfRange> end_range;
  if (FAILED(context->GetStart(read_only_edit_cookie,
                               start_to_end_range.Receive()))) {
    return false;
  }
  if (FAILED(context->GetEnd(read_only_edit_cookie, end_range.Receive())))
    return false;
  if (FAILED(start_to_end_range->ShiftEndToRange(read_only_edit_cookie,
                                                 end_range, TF_ANCHOR_END))) {
    return false;
  }

  base::win::ScopedComPtr<IEnumTfRanges> ranges;
  if (FAILED(track_property->EnumRanges(read_only_edit_cookie, ranges.Receive(),
                                        start_to_end_range))) {
    return false;
  }

  while (true) {
    base::win::ScopedComPtr<ITfRange> range;
    if (ranges->Next(1, range.Receive(), NULL) != S_OK)
      break;
    base::win::ScopedVariant value;
    base::win::ScopedComPtr<IEnumTfPropertyValue> enum_prop_value;
    if (FAILED(track_property->GetValue(read_only_edit_cookie, range,
                                        value.Receive()))) {
      return false;
    }
    if (FAILED(enum_prop_value.QueryFrom(value.AsInput()->punkVal)))
      return false;

    TF_PROPERTYVAL property_value;
    bool is_composition = false;
    bool has_display_attribute = false;
    TF_DISPLAYATTRIBUTE display_attribute;
    while (enum_prop_value->Next(1, &property_value, NULL) == S_OK) {
      if (IsEqualGUID(property_value.guidId, GUID_PROP_COMPOSING)) {
        is_composition = (property_value.varValue.lVal == TRUE);
      } else if (IsEqualGUID(property_value.guidId, GUID_PROP_ATTRIBUTE)) {
        TfGuidAtom guid_atom =
            static_cast<TfGuidAtom>(property_value.varValue.lVal);
        if (GetDisplayAttribute(guid_atom, &display_attribute))
          has_display_attribute = true;
      }
      VariantClear(&property_value.varValue);
    }

    base::win::ScopedComPtr<ITfRangeACP> range_acp;
    range_acp.QueryFrom(range);
    LONG start_pos, length;
    range_acp->GetExtent(&start_pos, &length);
    if (!is_composition) {
      if (*committed_size < static_cast<size_t>(start_pos + length))
        *committed_size = start_pos + length;
    } else {
      metro_viewer::UnderlineInfo underline;
      underline.start_offset = start_pos;
      underline.end_offset = start_pos + length;
      underline.thick = !!display_attribute.fBoldLine;
      undelines->push_back(underline);
    }
  }
  return true;
}

bool TextStore::CancelComposition() {
  // If there is an on-going document lock, we must not edit the text.
  if (edit_flag_)
    return false;

  if (string_buffer_.empty())
    return true;

  // Unlike ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL, 0) in IMM32, TSF does
  // not have a dedicated method to cancel composition. However, CUAS actually
  // has a protocol conversion from CPS_CANCEL into TSF operations. According
  // to the observations on Windows 7, TIPs are expected to cancel composition
  // when an on-going composition text is replaced with an empty string. So
  // we use the same operation to cancel composition here to minimize the risk
  // of potential compatibility issues.

  const uint32 previous_buffer_size =
      static_cast<uint32>(string_buffer_.size());
  string_buffer_.clear();
  committed_size_ = 0;
  selection_start_ = 0;
  selection_end_ = 0;
  if (text_store_acp_sink_mask_ & TS_AS_SEL_CHANGE)
    text_store_acp_sink_->OnSelectionChange();
  if (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE)
    text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
  if (text_store_acp_sink_mask_ & TS_AS_TEXT_CHANGE) {
    TS_TEXTCHANGE textChange = {};
    textChange.acpStart = 0;
    textChange.acpOldEnd = previous_buffer_size;
    textChange.acpNewEnd = 0;
    text_store_acp_sink_->OnTextChange(0, &textChange);
  }
  return true;
}

bool TextStore::ConfirmComposition() {
  // If there is an on-going document lock, we must not edit the text.
  if (edit_flag_)
    return false;

  if (string_buffer_.empty())
    return true;

  // See the comment in TextStore::CancelComposition.
  // This logic is based on the observation about how to emulate
  // ImmNotifyIME(NI_COMPOSITIONSTR, CPS_COMPLETE, 0) by CUAS.

  const base::string16& composition_text =
      string_buffer_.substr(committed_size_);
  if (!composition_text.empty())
    delegate_->OnTextCommitted(composition_text);

  const uint32 previous_buffer_size =
      static_cast<uint32>(string_buffer_.size());
  string_buffer_.clear();
  committed_size_ = 0;
  selection_start_ = 0;
  selection_end_ = 0;
  if (text_store_acp_sink_mask_ & TS_AS_SEL_CHANGE)
    text_store_acp_sink_->OnSelectionChange();
  if (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE)
    text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
  if (text_store_acp_sink_mask_ & TS_AS_TEXT_CHANGE) {
    TS_TEXTCHANGE textChange = {};
    textChange.acpStart = 0;
    textChange.acpOldEnd = previous_buffer_size;
    textChange.acpNewEnd = 0;
    text_store_acp_sink_->OnTextChange(0, &textChange);
  }
  return true;
}

void TextStore::SendOnLayoutChange() {
  if (text_store_acp_sink_ && (text_store_acp_sink_mask_ & TS_AS_LAYOUT_CHANGE))
    text_store_acp_sink_->OnLayoutChange(TS_LC_CHANGE, 0);
}

bool TextStore::HasReadLock() const {
  return (current_lock_type_ & TS_LF_READ) == TS_LF_READ;
}

bool TextStore::HasReadWriteLock() const {
  return (current_lock_type_ & TS_LF_READWRITE) == TS_LF_READWRITE;
}

}  // namespace metro_driver
