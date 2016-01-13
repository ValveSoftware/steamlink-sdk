// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/ime/input_source.h"

#include <atlbase.h>
#include <atlcom.h>
#include <msctf.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/win/scoped_comptr.h"
#include "ui/base/win/atl_module.h"
#include "win8/metro_driver/ime/input_source_observer.h"

namespace metro_driver {
namespace {

// An implementation of ITfLanguageProfileNotifySink interface, which will be
// used to receive notifications when the text input source is changed.
class ATL_NO_VTABLE InputSourceMonitor
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITfLanguageProfileNotifySink {
 public:
  InputSourceMonitor()
      : cookie_(TF_INVALID_COOKIE) {
  }

  BEGIN_COM_MAP(InputSourceMonitor)
    COM_INTERFACE_ENTRY(ITfLanguageProfileNotifySink)
  END_COM_MAP()

  bool Initialize(ITfSource* source) {
    DWORD cookie = TF_INVALID_COOKIE;
    HRESULT hr = source->AdviseSink(IID_ITfLanguageProfileNotifySink,
                                    this,
                                    &cookie);
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfSource::AdviseSink failed. hr = " << hr;
      return false;
    }
    cookie_ = cookie;
    source_ = source;
    return true;
  }

  void SetCallback(base::Closure on_language_chanaged) {
    on_language_chanaged_ = on_language_chanaged;
  }

  void Unadvise() {
    if (cookie_ == TF_INVALID_COOKIE || !source_)
      return;
    if (FAILED(source_->UnadviseSink(cookie_)))
      return;
    cookie_ = TF_INVALID_COOKIE;
    source_.Release();
  }

 private:
  // ITfLanguageProfileNotifySink overrides:
  STDMETHOD(OnLanguageChange)(LANGID langid, BOOL *accept) OVERRIDE {
    if (!accept)
      return E_INVALIDARG;
    *accept = TRUE;
    return S_OK;
  }

  STDMETHOD(OnLanguageChanged)() OVERRIDE {
    if (!on_language_chanaged_.is_null())
      on_language_chanaged_.Run();
    return S_OK;
  }

  base::Closure on_language_chanaged_;
  base::win::ScopedComPtr<ITfSource> source_;
  DWORD cookie_;

  DISALLOW_COPY_AND_ASSIGN(InputSourceMonitor);
};

class InputSourceImpl : public InputSource {
 public:
  InputSourceImpl(ITfInputProcessorProfileMgr* profile_manager,
                  InputSourceMonitor* monitor)
      : profile_manager_(profile_manager),
        monitor_(monitor) {
    monitor_->SetCallback(base::Bind(&InputSourceImpl::OnLanguageChanged,
                                     base::Unretained(this)));
  }
  virtual ~InputSourceImpl() {
    monitor_->SetCallback(base::Closure());
    monitor_->Unadvise();
  }

 private:
  // InputSource overrides.
  virtual bool GetActiveSource(LANGID* langid, bool* is_ime) OVERRIDE {
    TF_INPUTPROCESSORPROFILE profile = {};
    HRESULT hr = profile_manager_->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,
                                                    &profile);
    if (FAILED(hr)) {
      LOG(ERROR) << "ITfInputProcessorProfileMgr::GetActiveProfile failed."
                 << " hr = " << hr;
      return false;
    }
    *langid = profile.langid;
    *is_ime = profile.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR;
    return true;
  }
  virtual void AddObserver(InputSourceObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }
  virtual void RemoveObserver(InputSourceObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }
  void OnLanguageChanged() {
    FOR_EACH_OBSERVER(InputSourceObserver,
                      observer_list_,
                      OnInputSourceChanged());
  }

  base::win::ScopedComPtr<ITfInputProcessorProfileMgr> profile_manager_;
  scoped_refptr<InputSourceMonitor> monitor_;
  ObserverList<InputSourceObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(InputSourceImpl);
};

}  // namespace

// static
scoped_ptr<InputSource> InputSource::Create() {
  ui::win::CreateATLModuleIfNeeded();

  base::win::ScopedComPtr<ITfInputProcessorProfileMgr> profile_manager;
  HRESULT hr = profile_manager.CreateInstance(CLSID_TF_InputProcessorProfiles);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to instantiate CLSID_TF_InputProcessorProfiles."
               << " hr = " << hr;
    return scoped_ptr<InputSource>();
  }
  base::win::ScopedComPtr<ITfSource> profiles_source;
  hr = profiles_source.QueryFrom(profile_manager);
  if (FAILED(hr)) {
    LOG(ERROR) << "QueryFrom to ITfSource failed. hr = " << hr;
    return scoped_ptr<InputSource>();
  }

  CComObject<InputSourceMonitor>* monitor = NULL;
  hr = CComObject<InputSourceMonitor>::CreateInstance(&monitor);
  if (FAILED(hr)) {
    LOG(ERROR) << "CComObject<InputSourceMonitor>::CreateInstance failed."
               << " hr = " << hr;
    return scoped_ptr<InputSource>();
  }
  if (!monitor->Initialize(profiles_source)) {
    LOG(ERROR) << "Failed to initialize the monitor.";
    return scoped_ptr<InputSource>();
  }

  // Transfer the ownership.
  return scoped_ptr<InputSource>(new InputSourceImpl(profile_manager, monitor));
}

}  // namespace metro_driver
