// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/ime/input_scope.h"

#include <atlbase.h>
#include <atlcom.h>

#include "base/logging.h"
#include "ui/base/win/atl_module.h"

namespace metro_driver {
namespace {

// An implementation of ITfInputScope interface.
// This implementation only covers ITfInputScope::GetInputScopes since built-in
// on-screen keyboard on Windows 8+ changes its layout depending on the returned
// value of this method.
// Although other advanced features of ITfInputScope such as phase list or
// regex support might be useful for IMEs or on-screen keyboards in future,
// no IME seems to be utilizing such features as of Windows 8.1.
class ATL_NO_VTABLE InputScopeImpl
    : public CComObjectRootEx<CComMultiThreadModel>,
      public ITfInputScope {
 public:
  InputScopeImpl() {}

  BEGIN_COM_MAP(InputScopeImpl)
    COM_INTERFACE_ENTRY(ITfInputScope)
  END_COM_MAP()

  void Initialize(const std::vector<InputScope>& input_scopes) {
    input_scopes_ = input_scopes;
  }

 private:
  // ITfInputScope overrides:
  STDMETHOD(GetInputScopes)(InputScope** input_scopes, UINT* count) OVERRIDE {
    if (!count || !input_scopes)
      return E_INVALIDARG;
    *input_scopes = static_cast<InputScope*>(
        CoTaskMemAlloc(sizeof(InputScope) * input_scopes_.size()));
    if (!input_scopes) {
      *count = 0;
      return E_OUTOFMEMORY;
    }
    std::copy(input_scopes_.begin(), input_scopes_.end(), *input_scopes);
    *count = static_cast<UINT>(input_scopes_.size());
    return S_OK;
  }
  STDMETHOD(GetPhrase)(BSTR** phrases, UINT* count) OVERRIDE {
    return E_NOTIMPL;
  }
  STDMETHOD(GetRegularExpression)(BSTR* regexp) OVERRIDE {
    return E_NOTIMPL;
  }
  STDMETHOD(GetSRGS)(BSTR* srgs) OVERRIDE {
    return E_NOTIMPL;
  }
  STDMETHOD(GetXML)(BSTR* xml) OVERRIDE {
    return E_NOTIMPL;
  }

  // Data which ITfInputScope::GetInputScopes should return.
  std::vector<InputScope> input_scopes_;

  DISALLOW_COPY_AND_ASSIGN(InputScopeImpl);
};

}  // namespace

base::win::ScopedComPtr<ITfInputScope>
CreteInputScope(const std::vector<InputScope>& input_scopes) {
  ui::win::CreateATLModuleIfNeeded();
  CComObject<InputScopeImpl>* object = NULL;
  HRESULT hr = CComObject<InputScopeImpl>::CreateInstance(&object);
  if (FAILED(hr)) {
    LOG(ERROR) << "CComObject<InputScopeImpl>::CreateInstance failed. hr = "
               << hr;
    return base::win::ScopedComPtr<ITfInputScope>();
  }
  object->Initialize(input_scopes);
  return base::win::ScopedComPtr<ITfInputScope>(object);
}

}  // namespace metro_driver
