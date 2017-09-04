// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/global.h"

#include <vector>

#include "core/fxcrt/fx_ext.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_GlobalData.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_context.h"
#include "fpdfsdk/javascript/resource.h"

BEGIN_JS_STATIC_CONST(CJS_Global)
END_JS_STATIC_CONST()

BEGIN_JS_STATIC_PROP(CJS_Global)
END_JS_STATIC_PROP()

BEGIN_JS_STATIC_METHOD(CJS_Global)
JS_STATIC_METHOD_ENTRY(setPersistent)
END_JS_STATIC_METHOD()

IMPLEMENT_SPECIAL_JS_CLASS(CJS_Global, JSGlobalAlternate, global);

void CJS_Global::InitInstance(IJS_Runtime* pIRuntime) {
  CJS_Runtime* pRuntime = static_cast<CJS_Runtime*>(pIRuntime);
  JSGlobalAlternate* pGlobal =
      static_cast<JSGlobalAlternate*>(GetEmbedObject());
  pGlobal->Initial(pRuntime->GetFormFillEnv());
}

JSGlobalData::JSGlobalData()
    : nType(JS_GlobalDataType::NUMBER),
      dData(0),
      bData(false),
      sData(""),
      bPersistent(false),
      bDeleted(false) {}

JSGlobalData::~JSGlobalData() {
  pData.Reset();
}

JSGlobalAlternate::JSGlobalAlternate(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_pFormFillEnv(nullptr) {}

JSGlobalAlternate::~JSGlobalAlternate() {
  DestroyGlobalPersisitentVariables();
  m_pGlobalData->Release();
}

void JSGlobalAlternate::Initial(CPDFSDK_FormFillEnvironment* pFormFillEnv) {
  m_pFormFillEnv = pFormFillEnv;
  m_pGlobalData = CJS_GlobalData::GetRetainedInstance(pFormFillEnv);
  UpdateGlobalPersistentVariables();
}

bool JSGlobalAlternate::QueryProperty(const FX_WCHAR* propname) {
  return CFX_WideString(propname) != L"setPersistent";
}

bool JSGlobalAlternate::DelProperty(IJS_Context* cc,
                                    const FX_WCHAR* propname,
                                    CFX_WideString& sError) {
  auto it = m_mapGlobal.find(CFX_ByteString::FromUnicode(propname));
  if (it == m_mapGlobal.end())
    return false;

  it->second->bDeleted = true;
  return true;
}

bool JSGlobalAlternate::DoProperty(IJS_Context* cc,
                                   const FX_WCHAR* propname,
                                   CJS_PropValue& vp,
                                   CFX_WideString& sError) {
  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  if (vp.IsSetting()) {
    CFX_ByteString sPropName = CFX_ByteString::FromUnicode(propname);
    switch (vp.GetJSValue()->GetType()) {
      case CJS_Value::VT_number: {
        double dData;
        vp >> dData;
        return SetGlobalVariables(sPropName, JS_GlobalDataType::NUMBER, dData,
                                  false, "", v8::Local<v8::Object>(), false);
      }
      case CJS_Value::VT_boolean: {
        bool bData;
        vp >> bData;
        return SetGlobalVariables(sPropName, JS_GlobalDataType::BOOLEAN, 0,
                                  bData, "", v8::Local<v8::Object>(), false);
      }
      case CJS_Value::VT_string: {
        CFX_ByteString sData;
        vp >> sData;
        return SetGlobalVariables(sPropName, JS_GlobalDataType::STRING, 0,
                                  false, sData, v8::Local<v8::Object>(), false);
      }
      case CJS_Value::VT_object: {
        v8::Local<v8::Object> pData;
        vp >> pData;
        return SetGlobalVariables(sPropName, JS_GlobalDataType::OBJECT, 0,
                                  false, "", pData, false);
      }
      case CJS_Value::VT_null: {
        return SetGlobalVariables(sPropName, JS_GlobalDataType::NULLOBJ, 0,
                                  false, "", v8::Local<v8::Object>(), false);
      }
      case CJS_Value::VT_undefined: {
        DelProperty(cc, propname, sError);
        return true;
      }
      default:
        break;
    }
  } else {
    auto it = m_mapGlobal.find(CFX_ByteString::FromUnicode(propname));
    if (it == m_mapGlobal.end()) {
      vp.GetJSValue()->SetNull(pRuntime);
      return true;
    }
    JSGlobalData* pData = it->second;
    if (pData->bDeleted) {
      vp.GetJSValue()->SetNull(pRuntime);
      return true;
    }
    switch (pData->nType) {
      case JS_GlobalDataType::NUMBER:
        vp << pData->dData;
        return true;
      case JS_GlobalDataType::BOOLEAN:
        vp << pData->bData;
        return true;
      case JS_GlobalDataType::STRING:
        vp << pData->sData;
        return true;
      case JS_GlobalDataType::OBJECT: {
        v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(
            vp.GetJSRuntime()->GetIsolate(), pData->pData);
        vp << obj;
        return true;
      }
      case JS_GlobalDataType::NULLOBJ:
        vp.GetJSValue()->SetNull(pRuntime);
        return true;
      default:
        break;
    }
  }
  return false;
}

bool JSGlobalAlternate::setPersistent(IJS_Context* cc,
                                      const std::vector<CJS_Value>& params,
                                      CJS_Value& vRet,
                                      CFX_WideString& sError) {
  if (params.size() != 2) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  auto it = m_mapGlobal.find(params[0].ToCFXByteString(pRuntime));
  if (it != m_mapGlobal.end()) {
    JSGlobalData* pData = it->second;
    if (!pData->bDeleted) {
      pData->bPersistent = params[1].ToBool(pRuntime);
      return true;
    }
  }

  sError = JSGetStringFromID(IDS_STRING_JSNOGLOBAL);
  return false;
}

void JSGlobalAlternate::UpdateGlobalPersistentVariables() {
  CJS_Runtime* pRuntime =
      static_cast<CJS_Runtime*>(CFXJS_Engine::CurrentEngineFromIsolate(
          m_pJSObject->ToV8Object()->GetIsolate()));

  for (int i = 0, sz = m_pGlobalData->GetSize(); i < sz; i++) {
    CJS_GlobalData_Element* pData = m_pGlobalData->GetAt(i);
    switch (pData->data.nType) {
      case JS_GlobalDataType::NUMBER:
        SetGlobalVariables(pData->data.sKey, JS_GlobalDataType::NUMBER,
                           pData->data.dData, false, "",
                           v8::Local<v8::Object>(), pData->bPersistent == 1);
        pRuntime->PutObjectNumber(m_pJSObject->ToV8Object(),
                                  pData->data.sKey.UTF8Decode(),
                                  pData->data.dData);
        break;
      case JS_GlobalDataType::BOOLEAN:
        SetGlobalVariables(pData->data.sKey, JS_GlobalDataType::BOOLEAN, 0,
                           (bool)(pData->data.bData == 1), "",
                           v8::Local<v8::Object>(), pData->bPersistent == 1);
        pRuntime->PutObjectBoolean(m_pJSObject->ToV8Object(),
                                   pData->data.sKey.UTF8Decode(),
                                   (bool)(pData->data.bData == 1));
        break;
      case JS_GlobalDataType::STRING:
        SetGlobalVariables(pData->data.sKey, JS_GlobalDataType::STRING, 0,
                           false, pData->data.sData, v8::Local<v8::Object>(),
                           pData->bPersistent == 1);
        pRuntime->PutObjectString(m_pJSObject->ToV8Object(),
                                  pData->data.sKey.UTF8Decode(),
                                  pData->data.sData.UTF8Decode());
        break;
      case JS_GlobalDataType::OBJECT: {
        v8::Local<v8::Object> pObj = pRuntime->NewFxDynamicObj(-1);

        PutObjectProperty(pObj, &pData->data);
        SetGlobalVariables(pData->data.sKey, JS_GlobalDataType::OBJECT, 0,
                           false, "", pObj, pData->bPersistent == 1);
        pRuntime->PutObjectObject(m_pJSObject->ToV8Object(),
                                  pData->data.sKey.UTF8Decode(), pObj);
      } break;
      case JS_GlobalDataType::NULLOBJ:
        SetGlobalVariables(pData->data.sKey, JS_GlobalDataType::NULLOBJ, 0,
                           false, "", v8::Local<v8::Object>(),
                           pData->bPersistent == 1);
        pRuntime->PutObjectNull(m_pJSObject->ToV8Object(),
                                pData->data.sKey.UTF8Decode());
        break;
    }
  }
}

void JSGlobalAlternate::CommitGlobalPersisitentVariables(IJS_Context* cc) {
  for (auto it = m_mapGlobal.begin(); it != m_mapGlobal.end(); ++it) {
    CFX_ByteString name = it->first;
    JSGlobalData* pData = it->second;
    if (pData->bDeleted) {
      m_pGlobalData->DeleteGlobalVariable(name);
    } else {
      switch (pData->nType) {
        case JS_GlobalDataType::NUMBER:
          m_pGlobalData->SetGlobalVariableNumber(name, pData->dData);
          m_pGlobalData->SetGlobalVariablePersistent(name, pData->bPersistent);
          break;
        case JS_GlobalDataType::BOOLEAN:
          m_pGlobalData->SetGlobalVariableBoolean(name, pData->bData);
          m_pGlobalData->SetGlobalVariablePersistent(name, pData->bPersistent);
          break;
        case JS_GlobalDataType::STRING:
          m_pGlobalData->SetGlobalVariableString(name, pData->sData);
          m_pGlobalData->SetGlobalVariablePersistent(name, pData->bPersistent);
          break;
        case JS_GlobalDataType::OBJECT: {
          CJS_GlobalVariableArray array;
          v8::Local<v8::Object> obj = v8::Local<v8::Object>::New(
              GetJSObject()->GetIsolate(), pData->pData);
          ObjectToArray(cc, obj, array);
          m_pGlobalData->SetGlobalVariableObject(name, array);
          m_pGlobalData->SetGlobalVariablePersistent(name, pData->bPersistent);
        } break;
        case JS_GlobalDataType::NULLOBJ:
          m_pGlobalData->SetGlobalVariableNull(name);
          m_pGlobalData->SetGlobalVariablePersistent(name, pData->bPersistent);
          break;
      }
    }
  }
}

void JSGlobalAlternate::ObjectToArray(IJS_Context* cc,
                                      v8::Local<v8::Object> pObj,
                                      CJS_GlobalVariableArray& array) {
  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  std::vector<CFX_WideString> pKeyList = pRuntime->GetObjectPropertyNames(pObj);
  for (const auto& ws : pKeyList) {
    CFX_ByteString sKey = ws.UTF8Encode();
    v8::Local<v8::Value> v = pRuntime->GetObjectProperty(pObj, ws);
    switch (CJS_Value::GetValueType(v)) {
      case CJS_Value::VT_number: {
        CJS_KeyValue* pObjElement = new CJS_KeyValue;
        pObjElement->nType = JS_GlobalDataType::NUMBER;
        pObjElement->sKey = sKey;
        pObjElement->dData = pRuntime->ToNumber(v);
        array.Add(pObjElement);
      } break;
      case CJS_Value::VT_boolean: {
        CJS_KeyValue* pObjElement = new CJS_KeyValue;
        pObjElement->nType = JS_GlobalDataType::BOOLEAN;
        pObjElement->sKey = sKey;
        pObjElement->dData = pRuntime->ToBoolean(v);
        array.Add(pObjElement);
      } break;
      case CJS_Value::VT_string: {
        CFX_ByteString sValue =
            CJS_Value(pRuntime, v).ToCFXByteString(pRuntime);
        CJS_KeyValue* pObjElement = new CJS_KeyValue;
        pObjElement->nType = JS_GlobalDataType::STRING;
        pObjElement->sKey = sKey;
        pObjElement->sData = sValue;
        array.Add(pObjElement);
      } break;
      case CJS_Value::VT_object: {
        CJS_KeyValue* pObjElement = new CJS_KeyValue;
        pObjElement->nType = JS_GlobalDataType::OBJECT;
        pObjElement->sKey = sKey;
        ObjectToArray(cc, pRuntime->ToObject(v), pObjElement->objData);
        array.Add(pObjElement);
      } break;
      case CJS_Value::VT_null: {
        CJS_KeyValue* pObjElement = new CJS_KeyValue;
        pObjElement->nType = JS_GlobalDataType::NULLOBJ;
        pObjElement->sKey = sKey;
        array.Add(pObjElement);
      } break;
      default:
        break;
    }
  }
}

void JSGlobalAlternate::PutObjectProperty(v8::Local<v8::Object> pObj,
                                          CJS_KeyValue* pData) {
  CJS_Runtime* pRuntime = CJS_Runtime::CurrentRuntimeFromIsolate(
      m_pJSObject->ToV8Object()->GetIsolate());

  for (int i = 0, sz = pData->objData.Count(); i < sz; i++) {
    CJS_KeyValue* pObjData = pData->objData.GetAt(i);
    switch (pObjData->nType) {
      case JS_GlobalDataType::NUMBER:
        pRuntime->PutObjectNumber(pObj, pObjData->sKey.UTF8Decode(),
                                  pObjData->dData);
        break;
      case JS_GlobalDataType::BOOLEAN:
        pRuntime->PutObjectBoolean(pObj, pObjData->sKey.UTF8Decode(),
                                   pObjData->bData == 1);
        break;
      case JS_GlobalDataType::STRING:
        pRuntime->PutObjectString(pObj, pObjData->sKey.UTF8Decode(),
                                  pObjData->sData.UTF8Decode());
        break;
      case JS_GlobalDataType::OBJECT: {
        v8::Local<v8::Object> pNewObj = pRuntime->NewFxDynamicObj(-1);
        PutObjectProperty(pNewObj, pObjData);
        pRuntime->PutObjectObject(pObj, pObjData->sKey.UTF8Decode(), pNewObj);
      } break;
      case JS_GlobalDataType::NULLOBJ:
        pRuntime->PutObjectNull(pObj, pObjData->sKey.UTF8Decode());
        break;
    }
  }
}

void JSGlobalAlternate::DestroyGlobalPersisitentVariables() {
  for (const auto& pair : m_mapGlobal) {
    delete pair.second;
  }
  m_mapGlobal.clear();
}

bool JSGlobalAlternate::SetGlobalVariables(const CFX_ByteString& propname,
                                           JS_GlobalDataType nType,
                                           double dData,
                                           bool bData,
                                           const CFX_ByteString& sData,
                                           v8::Local<v8::Object> pData,
                                           bool bDefaultPersistent) {
  if (propname.IsEmpty())
    return false;

  auto it = m_mapGlobal.find(propname);
  if (it != m_mapGlobal.end()) {
    JSGlobalData* pTemp = it->second;
    if (pTemp->bDeleted || pTemp->nType != nType) {
      pTemp->dData = 0;
      pTemp->bData = 0;
      pTemp->sData = "";
      pTemp->nType = nType;
    }

    pTemp->bDeleted = false;
    switch (nType) {
      case JS_GlobalDataType::NUMBER: {
        pTemp->dData = dData;
      } break;
      case JS_GlobalDataType::BOOLEAN: {
        pTemp->bData = bData;
      } break;
      case JS_GlobalDataType::STRING: {
        pTemp->sData = sData;
      } break;
      case JS_GlobalDataType::OBJECT: {
        pTemp->pData.Reset(pData->GetIsolate(), pData);
      } break;
      case JS_GlobalDataType::NULLOBJ:
        break;
      default:
        return false;
    }
    return true;
  }

  JSGlobalData* pNewData = nullptr;

  switch (nType) {
    case JS_GlobalDataType::NUMBER: {
      pNewData = new JSGlobalData;
      pNewData->nType = JS_GlobalDataType::NUMBER;
      pNewData->dData = dData;
      pNewData->bPersistent = bDefaultPersistent;
    } break;
    case JS_GlobalDataType::BOOLEAN: {
      pNewData = new JSGlobalData;
      pNewData->nType = JS_GlobalDataType::BOOLEAN;
      pNewData->bData = bData;
      pNewData->bPersistent = bDefaultPersistent;
    } break;
    case JS_GlobalDataType::STRING: {
      pNewData = new JSGlobalData;
      pNewData->nType = JS_GlobalDataType::STRING;
      pNewData->sData = sData;
      pNewData->bPersistent = bDefaultPersistent;
    } break;
    case JS_GlobalDataType::OBJECT: {
      pNewData = new JSGlobalData;
      pNewData->nType = JS_GlobalDataType::OBJECT;
      pNewData->pData.Reset(pData->GetIsolate(), pData);
      pNewData->bPersistent = bDefaultPersistent;
    } break;
    case JS_GlobalDataType::NULLOBJ: {
      pNewData = new JSGlobalData;
      pNewData->nType = JS_GlobalDataType::NULLOBJ;
      pNewData->bPersistent = bDefaultPersistent;
    } break;
    default:
      return false;
  }

  m_mapGlobal[propname] = pNewData;
  return true;
}
