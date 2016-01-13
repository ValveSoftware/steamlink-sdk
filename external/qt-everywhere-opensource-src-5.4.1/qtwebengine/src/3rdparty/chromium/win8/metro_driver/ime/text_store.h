// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_TEXT_STORE_H_
#define WIN8_METRO_DRIVER_IME_TEXT_STORE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <initguid.h>
#include <inputscope.h>
#include <msctf.h>

#include <deque>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"
#include "ui/metro_viewer/ime_types.h"

namespace metro_driver {

class TextStoreDelegate;

// TextStore is used to interact with the input method via TSF manager.
// TextStore have a string buffer which is manipulated by TSF manager through
// ITextStoreACP interface methods such as SetText().
// When the input method updates the composition, TextStore calls
// TextInputClient::SetCompositionText(). And when the input method finishes the
// composition, TextStore calls TextInputClient::InsertText() and clears the
// buffer.
//
// How TextStore works:
//  - The user enters "a".
//    - The input method set composition as "a".
//    - TSF manager calls TextStore::RequestLock().
//    - TextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TextStore::OnStartComposition()
//      - TextStore::SetText()
//        The string buffer is set as "a".
//      - TextStore::OnUpdateComposition()
//      - TextStore::OnEndEdit()
//        TextStore can get the composition information such as underlines.
//   - TextStore calls TextInputClient::SetCompositionText().
//     "a" is shown with an underline as composition string.
// - The user enters <space>.
//    - The input method set composition as "A".
//    - TSF manager calls TextStore::RequestLock().
//    - TextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TextStore::SetText()
//        The string buffer is set as "A".
//      - TextStore::OnUpdateComposition()
//      - TextStore::OnEndEdit()
//   - TextStore calls TextInputClient::SetCompositionText().
//     "A" is shown with an underline as composition string.
// - The user enters <enter>.
//    - The input method commits "A".
//    - TSF manager calls TextStore::RequestLock().
//    - TextStore callbacks ITextStoreACPSink::OnLockGranted().
//    - In OnLockGranted(), TSF manager calls
//      - TextStore::OnEndComposition()
//      - TextStore::OnEndEdit()
//        TextStore knows "A" is committed.
//   - TextStore calls TextInputClient::InsertText().
//     "A" is shown as committed string.
//   - TextStore clears the string buffer.
//   - TextStore calls OnSelectionChange(), OnLayoutChange() and
//     OnTextChange() of ITextStoreACPSink to let TSF manager know that the
//     string buffer has been changed.
//
// About the locking scheme:
// When TSF manager manipulates the string buffer it calls RequestLock() to get
// the lock of the document. If TextStore can grant the lock request, it
// callbacks ITextStoreACPSink::OnLockGranted().
// RequestLock() is called from only one thread, but called recursively in
// OnLockGranted() or OnSelectionChange() or OnLayoutChange() or OnTextChange().
// If the document is locked and the lock request is asynchronous, TextStore
// queues the request. The queued requests will be handled after the current
// lock is removed.
// More information about document locks can be found here:
//   http://msdn.microsoft.com/en-us/library/ms538064
//
// More information about TSF can be found here:
//   http://msdn.microsoft.com/en-us/library/ms629032
class ATL_NO_VTABLE TextStore
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITextStoreACP,
      public ITfContextOwnerCompositionSink,
      public ITfTextEditSink {
 public:
  virtual ~TextStore();

  BEGIN_COM_MAP(TextStore)
    COM_INTERFACE_ENTRY(ITextStoreACP)
    COM_INTERFACE_ENTRY(ITfContextOwnerCompositionSink)
    COM_INTERFACE_ENTRY(ITfTextEditSink)
  END_COM_MAP()

  // ITextStoreACP:
  STDMETHOD(AdviseSink)(REFIID iid, IUnknown* unknown, DWORD mask) OVERRIDE;
  STDMETHOD(FindNextAttrTransition)(LONG acp_start,
                                    LONG acp_halt,
                                    ULONG num_filter_attributes,
                                    const TS_ATTRID* filter_attributes,
                                    DWORD flags,
                                    LONG* acp_next,
                                    BOOL* found,
                                    LONG* found_offset) OVERRIDE;
  STDMETHOD(GetACPFromPoint)(TsViewCookie view_cookie,
                             const POINT* point,
                             DWORD flags,
                             LONG* acp) OVERRIDE;
  STDMETHOD(GetActiveView)(TsViewCookie* view_cookie) OVERRIDE;
  STDMETHOD(GetEmbedded)(LONG acp_pos,
                         REFGUID service,
                         REFIID iid,
                         IUnknown** unknown) OVERRIDE;
  STDMETHOD(GetEndACP)(LONG* acp) OVERRIDE;
  STDMETHOD(GetFormattedText)(LONG acp_start,
                              LONG acp_end,
                              IDataObject** data_object) OVERRIDE;
  STDMETHOD(GetScreenExt)(TsViewCookie view_cookie, RECT* rect) OVERRIDE;
  STDMETHOD(GetSelection)(ULONG selection_index,
                          ULONG selection_buffer_size,
                          TS_SELECTION_ACP* selection_buffer,
                          ULONG* fetched_count) OVERRIDE;
  STDMETHOD(GetStatus)(TS_STATUS* pdcs) OVERRIDE;
  STDMETHOD(GetText)(LONG acp_start,
                     LONG acp_end,
                     wchar_t* text_buffer,
                     ULONG text_buffer_size,
                     ULONG* text_buffer_copied,
                     TS_RUNINFO* run_info_buffer,
                     ULONG run_info_buffer_size,
                     ULONG* run_info_buffer_copied,
                     LONG* next_acp) OVERRIDE;
  STDMETHOD(GetTextExt)(TsViewCookie view_cookie,
                        LONG acp_start,
                        LONG acp_end,
                        RECT* rect,
                        BOOL* clipped) OVERRIDE;
  STDMETHOD(GetWnd)(TsViewCookie view_cookie, HWND* window_handle) OVERRIDE;
  STDMETHOD(InsertEmbedded)(DWORD flags,
                            LONG acp_start,
                            LONG acp_end,
                            IDataObject* data_object,
                            TS_TEXTCHANGE* change) OVERRIDE;
  STDMETHOD(InsertEmbeddedAtSelection)(DWORD flags,
                                       IDataObject* data_object,
                                       LONG* acp_start,
                                       LONG* acp_end,
                                       TS_TEXTCHANGE* change) OVERRIDE;
  STDMETHOD(InsertTextAtSelection)(DWORD flags,
                                   const wchar_t* text_buffer,
                                   ULONG text_buffer_size,
                                   LONG* acp_start,
                                   LONG* acp_end,
                                   TS_TEXTCHANGE* text_change) OVERRIDE;
  STDMETHOD(QueryInsert)(LONG acp_test_start,
                         LONG acp_test_end,
                         ULONG text_size,
                         LONG* acp_result_start,
                         LONG* acp_result_end) OVERRIDE;
  STDMETHOD(QueryInsertEmbedded)(const GUID* service,
                                 const FORMATETC* format,
                                 BOOL* insertable) OVERRIDE;
  STDMETHOD(RequestAttrsAtPosition)(LONG acp_pos,
                                    ULONG attribute_buffer_size,
                                    const TS_ATTRID* attribute_buffer,
                                    DWORD flags) OVERRIDE;
  STDMETHOD(RequestAttrsTransitioningAtPosition)(
      LONG acp_pos,
      ULONG attribute_buffer_size,
      const TS_ATTRID* attribute_buffer,
      DWORD flags) OVERRIDE;
  STDMETHOD(RequestLock)(DWORD lock_flags, HRESULT* result) OVERRIDE;
  STDMETHOD(RequestSupportedAttrs)(DWORD flags,
                                   ULONG attribute_buffer_size,
                                   const TS_ATTRID* attribute_buffer) OVERRIDE;
  STDMETHOD(RetrieveRequestedAttrs)(ULONG attribute_buffer_size,
                                    TS_ATTRVAL* attribute_buffer,
                                    ULONG* attribute_buffer_copied) OVERRIDE;
  STDMETHOD(SetSelection)(ULONG selection_buffer_size,
                          const TS_SELECTION_ACP* selection_buffer) OVERRIDE;
  STDMETHOD(SetText)(DWORD flags,
                     LONG acp_start,
                     LONG acp_end,
                     const wchar_t* text_buffer,
                     ULONG text_buffer_size,
                     TS_TEXTCHANGE* text_change) OVERRIDE;
  STDMETHOD(UnadviseSink)(IUnknown* unknown) OVERRIDE;

  // ITfContextOwnerCompositionSink:
  STDMETHOD(OnStartComposition)(ITfCompositionView* composition_view,
                                BOOL* ok) OVERRIDE;
  STDMETHOD(OnUpdateComposition)(ITfCompositionView* composition_view,
                                 ITfRange* range) OVERRIDE;
  STDMETHOD(OnEndComposition)(ITfCompositionView* composition_view) OVERRIDE;

  // ITfTextEditSink:
  STDMETHOD(OnEndEdit)(ITfContext* context, TfEditCookie read_only_edit_cookie,
                       ITfEditRecord* edit_record) OVERRIDE;

  // Cancels the ongoing composition if exists.
  bool CancelComposition();

  // Confirms the ongoing composition if exists.
  bool ConfirmComposition();

  // Sends OnLayoutChange() via |text_store_acp_sink_|.
  void SendOnLayoutChange();

  // Creates an instance of TextStore. Returns NULL if fails.
  static scoped_refptr<TextStore> Create(
      HWND window_handle,
      const std::vector<InputScope>& input_scopes,
      TextStoreDelegate* delegate);

 private:
  friend CComObject<TextStore>;
  TextStore();

  void Initialize(HWND window_handle,
                  ITfCategoryMgr* category_manager,
                  ITfDisplayAttributeMgr* display_attribute_manager,
                  ITfInputScope* input_scope,
                  TextStoreDelegate* delegate);

  // Checks if the document has a read-only lock.
  bool HasReadLock() const;

  // Checks if the document has a read and write lock.
  bool HasReadWriteLock() const;

  // Gets the display attribute structure.
  bool GetDisplayAttribute(TfGuidAtom guid_atom,
                           TF_DISPLAYATTRIBUTE* attribute);

  // Gets the committed string size and underline information of the context.
  bool GetCompositionStatus(
      ITfContext* context,
      const TfEditCookie read_only_edit_cookie,
      uint32* committed_size,
      std::vector<metro_viewer::UnderlineInfo>* undelines);

  // A pointer of ITextStoreACPSink, this instance is given in AdviseSink.
  base::win::ScopedComPtr<ITextStoreACPSink> text_store_acp_sink_;

  // The current mask of |text_store_acp_sink_|.
  DWORD text_store_acp_sink_mask_;

  // HWND of the attached window.
  HWND window_handle_;

  //  |string_buffer_| contains committed string and composition string.
  //  Example: "aoi" is committed, and "umi" is under composition.
  //    |string_buffer_|: "aoiumi"
  //    |committed_size_|: 3
  base::string16 string_buffer_;
  uint32 committed_size_;

  //  |selection_start_| and |selection_end_| indicates the selection range.
  //  Example: "iue" is selected
  //    |string_buffer_|: "aiueo"
  //    |selection_start_|: 1
  //    |selection_end_|: 4
  uint32 selection_start_;
  uint32 selection_end_;

  //  |start_offset| and |end_offset| of |composition_undelines_| indicates
  //  the offsets in |string_buffer_|.
  //  Example: "aoi" is committed. There are two underlines in "umi" and "no".
  //    |string_buffer_|: "aoiumino"
  //    |committed_size_|: 3
  //    underlines_[0].start_offset: 3
  //    underlines_[0].end_offset: 6
  //    underlines_[1].start_offset: 6
  //    underlines_[1].end_offset: 8
  std::vector<metro_viewer::UnderlineInfo> underlines_;

  // |edit_flag_| indicates that the status is edited during
  // ITextStoreACPSink::OnLockGranted().
  bool edit_flag_;

  // The type of current lock.
  //   0: No lock.
  //   TS_LF_READ: read-only lock.
  //   TS_LF_READWRITE: read/write lock.
  DWORD current_lock_type_;

  // Queue of the lock request used in RequestLock().
  std::deque<DWORD> lock_queue_;

  // Category manager and Display attribute manager are used to obtain the
  // attributes of the composition string.
  base::win::ScopedComPtr<ITfCategoryMgr> category_manager_;
  base::win::ScopedComPtr<ITfDisplayAttributeMgr> display_attribute_manager_;

  // Represents the context information of this text.
  base::win::ScopedComPtr<ITfInputScope> input_scope_;

  // The delegate attached to this text store.
  TextStoreDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TextStore);
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_TEXT_STORE_H_
