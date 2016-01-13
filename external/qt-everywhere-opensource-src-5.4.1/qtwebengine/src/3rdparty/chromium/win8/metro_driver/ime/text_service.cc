// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/ime/text_service.h"

#include <msctf.h>

#include "base/logging.h"
#include "base/win/scoped_variant.h"
#include "ui/metro_viewer/ime_types.h"
#include "win8/metro_driver/ime/text_service_delegate.h"
#include "win8/metro_driver/ime/text_store.h"
#include "win8/metro_driver/ime/text_store_delegate.h"

// Architecture overview of input method support on Ash mode:
//
// Overview:
// On Ash mode, the system keyboard focus is owned by the metro_driver process
// while most of event handling are still implemented in the browser process.
// Thus the metro_driver basically works as a proxy that simply forwards
// keyevents to the metro_driver process. IME support must be involved somewhere
// in this flow.
//
// In short, we need to interact with an IME in the metro_driver process since
// TSF (Text Services Framework) runtime wants to processes keyevents while
// (and only while) the attached UI thread owns keyboard focus.
//
// Due to this limitation, we need to split IME handling into two parts, one
// is in the metro_driver process and the other is in the browser process.
// The metro_driver process is responsible for implementing the primary data
// store for the composition text and wiring it up with an IME via TSF APIs.
// On the other hand, the browser process is responsible for calculating
// character position in the composition text whenever the composition text
// is updated.
//
// IPC overview:
// Fortunately, we don't need so many IPC messages to support IMEs. In fact,
// only 4 messages are required to enable basic IME functionality.
//
//   metro_driver process -> browser process
//     Message Type:
//       - MetroViewerHostMsg_ImeCompositionChanged
//       - MetroViewerHostMsg_ImeTextCommitted
//     Message Routing:
//       TextServiceImpl
//         -> ChromeAppViewAsh
//         -- (process boundary) --
//         -> RemoteWindowTreeHostWin
//         -> RemoteInputMethodWin
//
//   browser process -> metro_driver process
//     Message Type:
//       - MetroViewerHostMsg_ImeCancelComposition
//       - MetroViewerHostMsg_ImeTextInputClientUpdated
//     Message Routing:
//       RemoteInputMethodWin
//         -> RemoteWindowTreeHostWin
//         -- (process boundary) --
//         -> ChromeAppViewAsh
//         -> TextServiceImpl
//
// Note that a keyevent may be forwarded through a different path. When a
// keyevent is not handled by an IME, such keyevent and subsequent character
// events will be sent from the metro_driver process to the browser process as
// following IPC messages.
//  - MetroViewerHostMsg_KeyDown
//  - MetroViewerHostMsg_KeyUp
//  - MetroViewerHostMsg_Character
//
// How TextServiceImpl works:
// Here is the list of the major tasks that are handled in TextServiceImpl.
//  - Manages a session object obtained from TSF runtime. We need them to call
//    most of TSF APIs.
//  - Handles OnDocumentChanged event. Whenever the document type is changed,
//    TextServiceImpl destroyes the current document and initializes new one
//    according to the given |input_scopes|.
//  - Stores the |composition_character_bounds_| passed from OnDocumentChanged
//    event so that an IME or on-screen keyboard can query the character
//    position synchronously.
// The most complicated part is the OnDocumentChanged handler. Since some IMEs
// such as Japanese IMEs drastically change their behavior depending on
// properties exposed from the virtual document, we need to set up a lot
// properties carefully and correctly. See DocumentBinding class in this file
// about what will be involved in this multi-phase construction. See also
// text_store.cc and input_scope.cc for more underlying details.

namespace metro_driver {
namespace {

// Japanese IME expects the default value of this compartment is
// TF_SENTENCEMODE_PHRASEPREDICT to emulate IMM32 behavior. This value is
// managed per thread, thus setting this value at once is sufficient. This
// value never affects non-Japanese IMEs.
bool InitializeSentenceMode(ITfThreadMgr2* thread_manager,
                            TfClientId client_id) {
  base::win::ScopedComPtr<ITfCompartmentMgr> thread_compartment_manager;
  HRESULT hr = thread_compartment_manager.QueryFrom(thread_manager);
  if (FAILED(hr)) {
    LOG(ERROR) << "QueryFrom failed. hr = " << hr;
    return false;
  }
  base::win::ScopedComPtr<ITfCompartment> sentence_compartment;
  hr = thread_compartment_manager->GetCompartment(
      GUID_COMPARTMENT_KEYBOARD_INPUTMODE_SENTENCE,
      sentence_compartment.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::GetCompartment failed. hr = " << hr;
    return false;
  }

  base::win::ScopedVariant sentence_variant;
  sentence_variant.Set(TF_SENTENCEMODE_PHRASEPREDICT);
  hr = sentence_compartment->SetValue(client_id, &sentence_variant);
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::SetValue failed. hr = " << hr;
    return false;
  }
  return true;
}

// Initializes |context| as disabled context where IMEs will be disabled.
bool InitializeDisabledContext(ITfContext* context, TfClientId client_id) {
  base::win::ScopedComPtr<ITfCompartmentMgr> compartment_mgr;
  HRESULT hr = compartment_mgr.QueryFrom(context);
  if (FAILED(hr)) {
    LOG(ERROR) << "QueryFrom failed. hr = " << hr;
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> disabled_compartment;
  hr = compartment_mgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_DISABLED,
                                       disabled_compartment.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::GetCompartment failed. hr = " << hr;
    return false;
  }

  base::win::ScopedVariant variant;
  variant.Set(1);
  hr = disabled_compartment->SetValue(client_id, &variant);
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::SetValue failed. hr = " << hr;
    return false;
  }

  base::win::ScopedComPtr<ITfCompartment> empty_context;
  hr = compartment_mgr->GetCompartment(GUID_COMPARTMENT_EMPTYCONTEXT,
                                       empty_context.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::GetCompartment failed. hr = " << hr;
    return false;
  }

  base::win::ScopedVariant empty_context_variant;
  empty_context_variant.Set(static_cast<int32>(1));
  hr = empty_context->SetValue(client_id, &empty_context_variant);
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfCompartment::SetValue failed. hr = " << hr;
    return false;
  }

  return true;
}

bool IsPasswordField(const std::vector<InputScope>& input_scopes) {
  return std::find(input_scopes.begin(), input_scopes.end(), IS_PASSWORD) !=
      input_scopes.end();
}

// A class that manages the lifetime of the event callback registration. When
// this object is destroyed, corresponding event callback will be unregistered.
class EventSink {
 public:
  EventSink(DWORD cookie, base::win::ScopedComPtr<ITfSource> source)
      : cookie_(cookie),
        source_(source) {}
  ~EventSink() {
    if (!source_ || cookie_ != TF_INVALID_COOKIE)
      return;
    source_->UnadviseSink(cookie_);
    cookie_ = TF_INVALID_COOKIE;
    source_.Release();
  }

 private:
  DWORD cookie_;
  base::win::ScopedComPtr<ITfSource> source_;
  DISALLOW_COPY_AND_ASSIGN(EventSink);
};

scoped_ptr<EventSink> CreateTextEditSink(ITfContext* context,
                                         ITfTextEditSink* text_store) {
  DCHECK(text_store);
  base::win::ScopedComPtr<ITfSource> source;
  DWORD cookie = TF_INVALID_EDIT_COOKIE;
  HRESULT hr = source.QueryFrom(context);
  if (FAILED(hr)) {
    LOG(ERROR) << "QueryFrom failed, hr = " << hr;
    return scoped_ptr<EventSink>();
  }
  hr = source->AdviseSink(IID_ITfTextEditSink, text_store, &cookie);
  if (FAILED(hr)) {
    LOG(ERROR) << "AdviseSink failed, hr = " << hr;
    return scoped_ptr<EventSink>();
  }
  return scoped_ptr<EventSink>(new EventSink(cookie, source));
}

// A set of objects that should have the same lifetime. Following things
// are maintained.
//  - TextStore: a COM object that abstracts text buffer. This object is
//      actually implemented by us in text_store.cc
//  - ITfDocumentMgr: a focusable unit in TSF. This object is implemented by
//      TSF runtime and works as a container of TextStore.
//  - EventSink: an object that ensures that the event callback between
//      TSF runtime and TextStore is unregistered when this object is destroyed.
class DocumentBinding {
 public:
  ~DocumentBinding() {
    if (!document_manager_)
      return;
    document_manager_->Pop(TF_POPF_ALL);
  }

  static scoped_ptr<DocumentBinding> Create(
      ITfThreadMgr2* thread_manager,
      TfClientId client_id,
      const std::vector<InputScope>& input_scopes,
      HWND window_handle,
      TextStoreDelegate* delegate) {
    base::win::ScopedComPtr<ITfDocumentMgr> document_manager;
    HRESULT hr = thread_manager->CreateDocumentMgr(document_manager.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfThreadMgr2::CreateDocumentMgr failed. hr = " << hr;
      return scoped_ptr<DocumentBinding>();
    }

    // Note: In our IPC protocol, an empty |input_scopes| is used to indicate
    // that an IME must be disabled in this context. In such case, we need not
    // instantiate TextStore.
    const bool use_null_text_store = input_scopes.empty();

    scoped_refptr<TextStore> text_store;
    if (!use_null_text_store) {
      text_store = TextStore::Create(window_handle, input_scopes, delegate);
      if (!text_store) {
        LOG(ERROR) << "Failed to create TextStore.";
        return scoped_ptr<DocumentBinding>();
      }
    }

    base::win::ScopedComPtr<ITfContext> context;
    DWORD edit_cookie = TF_INVALID_EDIT_COOKIE;
    hr = document_manager->CreateContext(
        client_id,
        0,
        static_cast<ITextStoreACP*>(text_store.get()),
        context.Receive(),
        &edit_cookie);
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfDocumentMgr::CreateContext failed. hr = " << hr;
      return scoped_ptr<DocumentBinding>();
    }

    // If null-TextStore is used or |input_scopes| looks like a password field,
    // set special properties to tell IMEs to be disabled.
    if ((use_null_text_store || IsPasswordField(input_scopes)) &&
        !InitializeDisabledContext(context, client_id)) {
      LOG(ERROR) << "InitializeDisabledContext failed.";
      return scoped_ptr<DocumentBinding>();
    }

    scoped_ptr<EventSink> text_edit_sink;
    if (!use_null_text_store) {
      text_edit_sink = CreateTextEditSink(context, text_store);
      if (!text_edit_sink) {
        LOG(ERROR) << "CreateTextEditSink failed.";
        return scoped_ptr<DocumentBinding>();
      }
    }
    hr = document_manager->Push(context);
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfDocumentMgr::Push failed. hr = " << hr;
      return scoped_ptr<DocumentBinding>();
    }
    return scoped_ptr<DocumentBinding>(
        new DocumentBinding(text_store,
                            document_manager,
                            text_edit_sink.Pass()));
  }

  ITfDocumentMgr* document_manager() const {
    return document_manager_;
  }

  scoped_refptr<TextStore> text_store() const {
    return text_store_;
  }

 private:
  DocumentBinding(scoped_refptr<TextStore> text_store,
                  base::win::ScopedComPtr<ITfDocumentMgr> document_manager,
                  scoped_ptr<EventSink> text_edit_sink)
      : text_store_(text_store),
        document_manager_(document_manager),
        text_edit_sink_(text_edit_sink.Pass()) {}

  scoped_refptr<TextStore> text_store_;
  base::win::ScopedComPtr<ITfDocumentMgr> document_manager_;
  scoped_ptr<EventSink> text_edit_sink_;

  DISALLOW_COPY_AND_ASSIGN(DocumentBinding);
};

class TextServiceImpl : public TextService,
                        public TextStoreDelegate {
 public:
  TextServiceImpl(ITfThreadMgr2* thread_manager,
                  TfClientId client_id,
                  HWND window_handle,
                  TextServiceDelegate* delegate)
      : client_id_(client_id),
        window_handle_(window_handle),
        delegate_(delegate),
        thread_manager_(thread_manager) {
    DCHECK_NE(TF_CLIENTID_NULL, client_id);
    DCHECK(window_handle != NULL);
    DCHECK(thread_manager_);
  }
  virtual ~TextServiceImpl() {
    thread_manager_->Deactivate();
  }

 private:
  // TextService overrides:
  virtual void CancelComposition() OVERRIDE {
    if (!current_document_) {
      VLOG(0) << "|current_document_| is NULL due to the previous error.";
      return;
    }
    TextStore* text_store = current_document_->text_store();
    if (!text_store)
      return;
    text_store->CancelComposition();
  }

  virtual void OnDocumentChanged(
      const std::vector<int32>& input_scopes,
      const std::vector<metro_viewer::CharacterBounds>& character_bounds)
      OVERRIDE {
    bool document_type_changed = input_scopes_ != input_scopes;
    input_scopes_ = input_scopes;
    composition_character_bounds_ = character_bounds;
    if (document_type_changed)
      OnDocumentTypeChanged(input_scopes);
  }

  virtual void OnWindowActivated() OVERRIDE {
    if (!current_document_) {
      VLOG(0) << "|current_document_| is NULL due to the previous error.";
      return;
    }
    ITfDocumentMgr* document_manager = current_document_->document_manager();
    if (!document_manager) {
      VLOG(0) << "|document_manager| is NULL due to the previous error.";
      return;
    }
    HRESULT hr = thread_manager_->SetFocus(document_manager);
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfThreadMgr2::SetFocus failed. hr = " << hr;
      return;
    }
  }

  virtual void OnCompositionChanged(
      const base::string16& text,
      int32 selection_start,
      int32 selection_end,
      const std::vector<metro_viewer::UnderlineInfo>& underlines) OVERRIDE {
    if (!delegate_)
      return;
    delegate_->OnCompositionChanged(text,
                                    selection_start,
                                    selection_end,
                                    underlines);
  }

  virtual void OnTextCommitted(const base::string16& text) OVERRIDE {
    if (!delegate_)
      return;
    delegate_->OnTextCommitted(text);
  }

  virtual RECT GetCaretBounds() {
    if (composition_character_bounds_.empty()) {
      const RECT rect = {};
      return rect;
    }
    const metro_viewer::CharacterBounds& bounds =
        composition_character_bounds_[0];
    POINT left_top = { bounds.left, bounds.top };
    POINT right_bottom = { bounds.right, bounds.bottom };
    ClientToScreen(window_handle_, &left_top);
    ClientToScreen(window_handle_, &right_bottom);
    const RECT rect = {
      left_top.x,
      left_top.y,
      right_bottom.x,
      right_bottom.y,
    };
    return rect;
  }

  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             RECT* rect) OVERRIDE {
    if (index >= composition_character_bounds_.size()) {
      return false;
    }
    const metro_viewer::CharacterBounds& bounds =
        composition_character_bounds_[index];
    POINT left_top = { bounds.left, bounds.top };
    POINT right_bottom = { bounds.right, bounds.bottom };
    ClientToScreen(window_handle_, &left_top);
    ClientToScreen(window_handle_, &right_bottom);
    SetRect(rect, left_top.x, left_top.y, right_bottom.x, right_bottom.y);
    return true;
  }

  void OnDocumentTypeChanged(const std::vector<int32>& input_scopes) {
    std::vector<InputScope> native_input_scopes(input_scopes.size());
    for (size_t i = 0; i < input_scopes.size(); ++i)
      native_input_scopes[i] = static_cast<InputScope>(input_scopes[i]);
    scoped_ptr<DocumentBinding> new_document =
        DocumentBinding::Create(thread_manager_.get(),
                                client_id_,
                                native_input_scopes,
                                window_handle_,
                                this);
    LOG_IF(ERROR, !new_document) << "Failed to create a new document.";
    current_document_.swap(new_document);
    OnWindowActivated();
  }

  TfClientId client_id_;
  HWND window_handle_;
  TextServiceDelegate* delegate_;
  scoped_ptr<DocumentBinding> current_document_;
  base::win::ScopedComPtr<ITfThreadMgr2> thread_manager_;

  // A vector of InputScope enumeration, which represents the document type of
  // the focused text field. Note that in our IPC message protocol, an empty
  // |input_scopes_| has special meaning that IMEs must be disabled on this
  // document.
  std::vector<int32> input_scopes_;
  // Character bounds of the composition. When there is no composition but this
  // vector is not empty, the first element contains the caret bounds.
  std::vector<metro_viewer::CharacterBounds> composition_character_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TextServiceImpl);
};

}  // namespace

scoped_ptr<TextService>
CreateTextService(TextServiceDelegate* delegate, HWND window_handle) {
  if (!delegate)
    return scoped_ptr<TextService>();
  base::win::ScopedComPtr<ITfThreadMgr2> thread_manager;
  HRESULT hr = thread_manager.CreateInstance(CLSID_TF_ThreadMgr);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create instance of CLSID_TF_ThreadMgr. hr = "
               << hr;
    return scoped_ptr<TextService>();
  }
  TfClientId client_id = TF_CLIENTID_NULL;
  hr = thread_manager->ActivateEx(&client_id, 0);
  if (FAILED(hr)) {
    LOG(ERROR) << "ITfThreadMgr2::ActivateEx failed. hr = " << hr;
    return scoped_ptr<TextService>();
  }
  if (!InitializeSentenceMode(thread_manager, client_id)) {
    LOG(ERROR) << "InitializeSentenceMode failed.";
    thread_manager->Deactivate();
    return scoped_ptr<TextService>();
  }
  return scoped_ptr<TextService>(new TextServiceImpl(thread_manager,
                                                     client_id,
                                                     window_handle,
                                                     delegate));
}

}  // namespace metro_driver
