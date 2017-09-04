// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/javascript/app.h"

#include <memory>
#include <vector>

#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/cpdfsdk_interform.h"
#include "fpdfsdk/javascript/Document.h"
#include "fpdfsdk/javascript/JS_Define.h"
#include "fpdfsdk/javascript/JS_EventHandler.h"
#include "fpdfsdk/javascript/JS_Object.h"
#include "fpdfsdk/javascript/JS_Value.h"
#include "fpdfsdk/javascript/cjs_context.h"
#include "fpdfsdk/javascript/cjs_runtime.h"
#include "fpdfsdk/javascript/resource.h"
#include "third_party/base/stl_util.h"

class GlobalTimer {
 public:
  GlobalTimer(app* pObj,
              CPDFSDK_FormFillEnvironment* pFormFillEnv,
              CJS_Runtime* pRuntime,
              int nType,
              const CFX_WideString& script,
              uint32_t dwElapse,
              uint32_t dwTimeOut);
  ~GlobalTimer();

  static void Trigger(int nTimerID);
  static void Cancel(int nTimerID);

  bool IsOneShot() const { return m_nType == 1; }
  uint32_t GetTimeOut() const { return m_dwTimeOut; }
  int GetTimerID() const { return m_nTimerID; }
  CJS_Runtime* GetRuntime() const { return m_pRuntime.Get(); }
  CFX_WideString GetJScript() const { return m_swJScript; }

 private:
  using TimerMap = std::map<uint32_t, GlobalTimer*>;
  static TimerMap* GetGlobalTimerMap();

  uint32_t m_nTimerID;
  app* const m_pEmbedObj;
  bool m_bProcessing;

  // data
  const int m_nType;  // 0:Interval; 1:TimeOut
  const uint32_t m_dwTimeOut;
  const CFX_WideString m_swJScript;
  CJS_Runtime::ObservedPtr m_pRuntime;
  CPDFSDK_FormFillEnvironment* const m_pFormFillEnv;
};

GlobalTimer::GlobalTimer(app* pObj,
                         CPDFSDK_FormFillEnvironment* pFormFillEnv,
                         CJS_Runtime* pRuntime,
                         int nType,
                         const CFX_WideString& script,
                         uint32_t dwElapse,
                         uint32_t dwTimeOut)
    : m_nTimerID(0),
      m_pEmbedObj(pObj),
      m_bProcessing(false),
      m_nType(nType),
      m_dwTimeOut(dwTimeOut),
      m_swJScript(script),
      m_pRuntime(pRuntime),
      m_pFormFillEnv(pFormFillEnv) {
  CFX_SystemHandler* pHandler = m_pFormFillEnv->GetSysHandler();
  m_nTimerID = pHandler->SetTimer(dwElapse, Trigger);
  if (m_nTimerID)
    (*GetGlobalTimerMap())[m_nTimerID] = this;
}

GlobalTimer::~GlobalTimer() {
  if (!m_nTimerID)
    return;

  if (GetRuntime())
    m_pFormFillEnv->GetSysHandler()->KillTimer(m_nTimerID);

  GetGlobalTimerMap()->erase(m_nTimerID);
}

// static
void GlobalTimer::Trigger(int nTimerID) {
  auto it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  GlobalTimer* pTimer = it->second;
  if (pTimer->m_bProcessing)
    return;

  pTimer->m_bProcessing = true;
  if (pTimer->m_pEmbedObj)
    pTimer->m_pEmbedObj->TimerProc(pTimer);

  // Timer proc may have destroyed timer, find it again.
  it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  pTimer = it->second;
  pTimer->m_bProcessing = false;
  if (pTimer->IsOneShot())
    pTimer->m_pEmbedObj->CancelProc(pTimer);
}

// static
void GlobalTimer::Cancel(int nTimerID) {
  auto it = GetGlobalTimerMap()->find(nTimerID);
  if (it == GetGlobalTimerMap()->end())
    return;

  GlobalTimer* pTimer = it->second;
  pTimer->m_pEmbedObj->CancelProc(pTimer);
}

// static
GlobalTimer::TimerMap* GlobalTimer::GetGlobalTimerMap() {
  // Leak the timer array at shutdown.
  static auto* s_TimerMap = new TimerMap;
  return s_TimerMap;
}

BEGIN_JS_STATIC_CONST(CJS_TimerObj)
END_JS_STATIC_CONST()

BEGIN_JS_STATIC_PROP(CJS_TimerObj)
END_JS_STATIC_PROP()

BEGIN_JS_STATIC_METHOD(CJS_TimerObj)
END_JS_STATIC_METHOD()

IMPLEMENT_JS_CLASS(CJS_TimerObj, TimerObj)

TimerObj::TimerObj(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_nTimerID(0) {}

TimerObj::~TimerObj() {}

void TimerObj::SetTimer(GlobalTimer* pTimer) {
  m_nTimerID = pTimer->GetTimerID();
}

#define JS_STR_VIEWERTYPE L"pdfium"
#define JS_STR_VIEWERVARIATION L"Full"
#define JS_STR_PLATFORM L"WIN"
#define JS_STR_LANGUAGE L"ENU"
#define JS_NUM_VIEWERVERSION 8
#ifdef PDF_ENABLE_XFA
#define JS_NUM_VIEWERVERSION_XFA 11
#endif  // PDF_ENABLE_XFA
#define JS_NUM_FORMSVERSION 7

BEGIN_JS_STATIC_CONST(CJS_App)
END_JS_STATIC_CONST()

BEGIN_JS_STATIC_PROP(CJS_App)
JS_STATIC_PROP_ENTRY(activeDocs)
JS_STATIC_PROP_ENTRY(calculate)
JS_STATIC_PROP_ENTRY(formsVersion)
JS_STATIC_PROP_ENTRY(fs)
JS_STATIC_PROP_ENTRY(fullscreen)
JS_STATIC_PROP_ENTRY(language)
JS_STATIC_PROP_ENTRY(media)
JS_STATIC_PROP_ENTRY(platform)
JS_STATIC_PROP_ENTRY(runtimeHighlight)
JS_STATIC_PROP_ENTRY(viewerType)
JS_STATIC_PROP_ENTRY(viewerVariation)
JS_STATIC_PROP_ENTRY(viewerVersion)
END_JS_STATIC_PROP()

BEGIN_JS_STATIC_METHOD(CJS_App)
JS_STATIC_METHOD_ENTRY(alert)
JS_STATIC_METHOD_ENTRY(beep)
JS_STATIC_METHOD_ENTRY(browseForDoc)
JS_STATIC_METHOD_ENTRY(clearInterval)
JS_STATIC_METHOD_ENTRY(clearTimeOut)
JS_STATIC_METHOD_ENTRY(execDialog)
JS_STATIC_METHOD_ENTRY(execMenuItem)
JS_STATIC_METHOD_ENTRY(findComponent)
JS_STATIC_METHOD_ENTRY(goBack)
JS_STATIC_METHOD_ENTRY(goForward)
JS_STATIC_METHOD_ENTRY(launchURL)
JS_STATIC_METHOD_ENTRY(mailMsg)
JS_STATIC_METHOD_ENTRY(newFDF)
JS_STATIC_METHOD_ENTRY(newDoc)
JS_STATIC_METHOD_ENTRY(openDoc)
JS_STATIC_METHOD_ENTRY(openFDF)
JS_STATIC_METHOD_ENTRY(popUpMenuEx)
JS_STATIC_METHOD_ENTRY(popUpMenu)
JS_STATIC_METHOD_ENTRY(response)
JS_STATIC_METHOD_ENTRY(setInterval)
JS_STATIC_METHOD_ENTRY(setTimeOut)
END_JS_STATIC_METHOD()

IMPLEMENT_JS_CLASS(CJS_App, app)

app::app(CJS_Object* pJSObject)
    : CJS_EmbedObj(pJSObject), m_bCalculate(true), m_bRuntimeHighLight(false) {}

app::~app() {
}

bool app::activeDocs(IJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;

  CJS_Context* pContext = (CJS_Context*)cc;
  CJS_Runtime* pRuntime = pContext->GetJSRuntime();
  CJS_Document* pJSDocument = nullptr;
  v8::Local<v8::Object> pObj = pRuntime->GetThisObj();
  if (CFXJS_Engine::GetObjDefnID(pObj) == CJS_Document::g_nObjDefnID)
    pJSDocument = static_cast<CJS_Document*>(pRuntime->GetObjectPrivate(pObj));

  CJS_Array aDocs;
  aDocs.SetElement(pRuntime, 0, CJS_Value(pRuntime, pJSDocument));
  if (aDocs.GetLength(pRuntime) > 0)
    vp << aDocs;
  else
    vp.GetJSValue()->SetNull(pRuntime);

  return true;
}

bool app::calculate(IJS_Context* cc,
                    CJS_PropValue& vp,
                    CFX_WideString& sError) {
  if (vp.IsSetting()) {
    bool bVP;
    vp >> bVP;
    m_bCalculate = (bool)bVP;

    CJS_Context* pContext = (CJS_Context*)cc;
    pContext->GetFormFillEnv()->GetInterForm()->EnableCalculate(
        (bool)m_bCalculate);
  } else {
    vp << (bool)m_bCalculate;
  }
  return true;
}

bool app::formsVersion(IJS_Context* cc,
                       CJS_PropValue& vp,
                       CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_NUM_FORMSVERSION;
    return true;
  }

  return false;
}

bool app::viewerType(IJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_STR_VIEWERTYPE;
    return true;
  }

  return false;
}

bool app::viewerVariation(IJS_Context* cc,
                          CJS_PropValue& vp,
                          CFX_WideString& sError) {
  if (vp.IsGetting()) {
    vp << JS_STR_VIEWERVARIATION;
    return true;
  }

  return false;
}

bool app::viewerVersion(IJS_Context* cc,
                        CJS_PropValue& vp,
                        CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CJS_Context* pJSContext = static_cast<CJS_Context*>(cc);
  CPDFXFA_Context* pXFAContext = pJSContext->GetFormFillEnv()->GetXFAContext();
  if (pXFAContext->GetDocType() == 1 || pXFAContext->GetDocType() == 2) {
    vp << JS_NUM_VIEWERVERSION_XFA;
    return true;
  }
#endif  // PDF_ENABLE_XFA
  vp << JS_NUM_VIEWERVERSION;
  return true;
}

bool app::platform(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      static_cast<CJS_Context*>(cc)->GetJSRuntime()->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;
  CFX_WideString platfrom = pFormFillEnv->GetPlatform();
  if (!platfrom.IsEmpty()) {
    vp << platfrom;
    return true;
  }
#endif
  vp << JS_STR_PLATFORM;
  return true;
}

bool app::language(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError) {
  if (!vp.IsGetting())
    return false;
#ifdef PDF_ENABLE_XFA
  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      static_cast<CJS_Context*>(cc)->GetJSRuntime()->GetFormFillEnv();
  if (!pFormFillEnv)
    return false;
  CFX_WideString language = pFormFillEnv->GetLanguage();
  if (!language.IsEmpty()) {
    vp << language;
    return true;
  }
#endif
  vp << JS_STR_LANGUAGE;
  return true;
}

// creates a new fdf object that contains no data
// comment: need reader support
// note:
// CFDF_Document * CPDFSDK_FormFillEnvironment::NewFDF();
bool app::newFDF(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  return true;
}
// opens a specified pdf document and returns its document object
// comment:need reader support
// note: as defined in js reference, the proto of this function's fourth
// parmeters, how old an fdf document while do not show it.
// CFDF_Document * CPDFSDK_FormFillEnvironment::OpenFDF(string strPath,bool
// bUserConv);

bool app::openFDF(IJS_Context* cc,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  return true;
}

bool app::alert(IJS_Context* cc,
                const std::vector<CJS_Value>& params,
                CJS_Value& vRet,
                CFX_WideString& sError) {
  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  std::vector<CJS_Value> newParams = JS_ExpandKeywordParams(
      pRuntime, params, 4, L"cMsg", L"nIcon", L"nType", L"cTitle");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CPDFSDK_FormFillEnvironment* pFormFillEnv = pRuntime->GetFormFillEnv();
  if (!pFormFillEnv) {
    vRet = CJS_Value(pRuntime, 0);
    return true;
  }

  CFX_WideString swMsg;
  if (newParams[0].GetType() == CJS_Value::VT_object) {
    CJS_Array carray;
    if (newParams[0].ConvertToArray(pRuntime, carray)) {
      swMsg = L"[";
      CJS_Value element(pRuntime);
      for (int i = 0; i < carray.GetLength(pRuntime); ++i) {
        if (i)
          swMsg += L", ";
        carray.GetElement(pRuntime, i, element);
        swMsg += element.ToCFXWideString(pRuntime);
      }
      swMsg += L"]";
    } else {
      swMsg = newParams[0].ToCFXWideString(pRuntime);
    }
  } else {
    swMsg = newParams[0].ToCFXWideString(pRuntime);
  }

  int iIcon = 0;
  if (newParams[1].GetType() != CJS_Value::VT_unknown)
    iIcon = newParams[1].ToInt(pRuntime);

  int iType = 0;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    iType = newParams[2].ToInt(pRuntime);

  CFX_WideString swTitle;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    swTitle = newParams[3].ToCFXWideString(pRuntime);
  else
    swTitle = JSGetStringFromID(IDS_STRING_JSALERT);

  pRuntime->BeginBlock();
  pFormFillEnv->KillFocusAnnot(0);

  vRet = CJS_Value(pRuntime, pFormFillEnv->JS_appAlert(
                                 swMsg.c_str(), swTitle.c_str(), iType, iIcon));
  pRuntime->EndBlock();
  return true;
}

bool app::beep(IJS_Context* cc,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError) {
  if (params.size() == 1) {
    CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
    pRuntime->GetFormFillEnv()->JS_appBeep(params[0].ToInt(pRuntime));
    return true;
  }

  sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
  return false;
}

bool app::findComponent(IJS_Context* cc,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  return true;
}

bool app::popUpMenuEx(IJS_Context* cc,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  return false;
}

bool app::fs(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError) {
  return false;
}

bool app::setInterval(IJS_Context* cc,
                      const std::vector<CJS_Value>& params,
                      CJS_Value& vRet,
                      CFX_WideString& sError) {
  if (params.size() > 2 || params.size() == 0) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  CFX_WideString script =
      params.size() > 0 ? params[0].ToCFXWideString(pRuntime) : L"";
  if (script.IsEmpty()) {
    sError = JSGetStringFromID(IDS_STRING_JSAFNUMBER_KEYSTROKE);
    return true;
  }

  uint32_t dwInterval = params.size() > 1 ? params[1].ToInt(pRuntime) : 1000;

  GlobalTimer* timerRef = new GlobalTimer(this, pRuntime->GetFormFillEnv(),
                                          pRuntime, 0, script, dwInterval, 0);
  m_Timers.insert(std::unique_ptr<GlobalTimer>(timerRef));

  v8::Local<v8::Object> pRetObj =
      pRuntime->NewFxDynamicObj(CJS_TimerObj::g_nObjDefnID);
  if (pRetObj.IsEmpty())
    return false;

  CJS_TimerObj* pJS_TimerObj =
      static_cast<CJS_TimerObj*>(pRuntime->GetObjectPrivate(pRetObj));
  TimerObj* pTimerObj = static_cast<TimerObj*>(pJS_TimerObj->GetEmbedObject());
  pTimerObj->SetTimer(timerRef);

  vRet = CJS_Value(pRuntime, pRetObj);
  return true;
}

bool app::setTimeOut(IJS_Context* cc,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  if (params.size() > 2 || params.size() == 0) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  CFX_WideString script = params[0].ToCFXWideString(pRuntime);
  if (script.IsEmpty()) {
    sError = JSGetStringFromID(IDS_STRING_JSAFNUMBER_KEYSTROKE);
    return true;
  }

  uint32_t dwTimeOut = params.size() > 1 ? params[1].ToInt(pRuntime) : 1000;
  GlobalTimer* timerRef =
      new GlobalTimer(this, pRuntime->GetFormFillEnv(), pRuntime, 1, script,
                      dwTimeOut, dwTimeOut);
  m_Timers.insert(std::unique_ptr<GlobalTimer>(timerRef));

  v8::Local<v8::Object> pRetObj =
      pRuntime->NewFxDynamicObj(CJS_TimerObj::g_nObjDefnID);
  if (pRetObj.IsEmpty())
    return false;

  CJS_TimerObj* pJS_TimerObj =
      static_cast<CJS_TimerObj*>(pRuntime->GetObjectPrivate(pRetObj));
  TimerObj* pTimerObj = static_cast<TimerObj*>(pJS_TimerObj->GetEmbedObject());
  pTimerObj->SetTimer(timerRef);
  vRet = CJS_Value(pRuntime, pRetObj);
  return true;
}

bool app::clearTimeOut(IJS_Context* cc,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  app::ClearTimerCommon(CJS_Runtime::FromContext(cc), params[0]);
  return true;
}

bool app::clearInterval(IJS_Context* cc,
                        const std::vector<CJS_Value>& params,
                        CJS_Value& vRet,
                        CFX_WideString& sError) {
  if (params.size() != 1) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }

  app::ClearTimerCommon(CJS_Runtime::FromContext(cc), params[0]);
  return true;
}

void app::ClearTimerCommon(CJS_Runtime* pRuntime, const CJS_Value& param) {
  if (param.GetType() != CJS_Value::VT_object)
    return;

  v8::Local<v8::Object> pObj = param.ToV8Object(pRuntime);
  if (CFXJS_Engine::GetObjDefnID(pObj) != CJS_TimerObj::g_nObjDefnID)
    return;

  CJS_Object* pJSObj = param.ToCJSObject(pRuntime);
  if (!pJSObj)
    return;

  TimerObj* pTimerObj = static_cast<TimerObj*>(pJSObj->GetEmbedObject());
  if (!pTimerObj)
    return;

  GlobalTimer::Cancel(pTimerObj->GetTimerID());
}

bool app::execMenuItem(IJS_Context* cc,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  return false;
}

void app::TimerProc(GlobalTimer* pTimer) {
  CJS_Runtime* pRuntime = pTimer->GetRuntime();
  if (pRuntime && (!pTimer->IsOneShot() || pTimer->GetTimeOut() > 0))
    RunJsScript(pRuntime, pTimer->GetJScript());
}

void app::CancelProc(GlobalTimer* pTimer) {
  m_Timers.erase(pdfium::FakeUniquePtr<GlobalTimer>(pTimer));
}

void app::RunJsScript(CJS_Runtime* pRuntime, const CFX_WideString& wsScript) {
  if (!pRuntime->IsBlocking()) {
    IJS_Context* pContext = pRuntime->NewContext();
    pContext->OnExternal_Exec();
    CFX_WideString wtInfo;
    pContext->RunScript(wsScript, &wtInfo);
    pRuntime->ReleaseContext(pContext);
  }
}

bool app::goBack(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool app::goForward(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  // Not supported.
  return true;
}

bool app::mailMsg(IJS_Context* cc,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  std::vector<CJS_Value> newParams =
      JS_ExpandKeywordParams(pRuntime, params, 6, L"bUI", L"cTo", L"cCc",
                             L"cBcc", L"cSubject", L"cMsg");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  bool bUI = newParams[0].ToBool(pRuntime);

  CFX_WideString cTo;
  if (newParams[1].GetType() != CJS_Value::VT_unknown) {
    cTo = newParams[1].ToCFXWideString(pRuntime);
  } else {
    if (!bUI) {
      // cTo parameter required when UI not invoked.
      sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
      return false;
    }
  }

  CFX_WideString cCc;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    cCc = newParams[2].ToCFXWideString(pRuntime);

  CFX_WideString cBcc;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    cBcc = newParams[3].ToCFXWideString(pRuntime);

  CFX_WideString cSubject;
  if (newParams[4].GetType() != CJS_Value::VT_unknown)
    cSubject = newParams[4].ToCFXWideString(pRuntime);

  CFX_WideString cMsg;
  if (newParams[5].GetType() != CJS_Value::VT_unknown)
    cMsg = newParams[5].ToCFXWideString(pRuntime);

  pRuntime->BeginBlock();
  CJS_Context* pContext = static_cast<CJS_Context*>(cc);
  pContext->GetFormFillEnv()->JS_docmailForm(nullptr, 0, bUI, cTo.c_str(),
                                             cSubject.c_str(), cCc.c_str(),
                                             cBcc.c_str(), cMsg.c_str());
  pRuntime->EndBlock();
  return true;
}

bool app::launchURL(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

bool app::runtimeHighlight(IJS_Context* cc,
                           CJS_PropValue& vp,
                           CFX_WideString& sError) {
  if (vp.IsSetting()) {
    vp >> m_bRuntimeHighLight;
  } else {
    vp << m_bRuntimeHighLight;
  }
  return true;
}

bool app::fullscreen(IJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError) {
  return false;
}

bool app::popUpMenu(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError) {
  return false;
}

bool app::browseForDoc(IJS_Context* cc,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError) {
  // Unsafe, not supported.
  return true;
}

CFX_WideString app::SysPathToPDFPath(const CFX_WideString& sOldPath) {
  CFX_WideString sRet = L"/";

  for (int i = 0, sz = sOldPath.GetLength(); i < sz; i++) {
    wchar_t c = sOldPath.GetAt(i);
    if (c == L':') {
    } else {
      if (c == L'\\') {
        sRet += L"/";
      } else {
        sRet += c;
      }
    }
  }

  return sRet;
}

bool app::newDoc(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError) {
  return false;
}

bool app::openDoc(IJS_Context* cc,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError) {
  return false;
}

bool app::response(IJS_Context* cc,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError) {
  CJS_Runtime* pRuntime = CJS_Runtime::FromContext(cc);
  std::vector<CJS_Value> newParams =
      JS_ExpandKeywordParams(pRuntime, params, 5, L"cQuestion", L"cTitle",
                             L"cDefault", L"bPassword", L"cLabel");

  if (newParams[0].GetType() == CJS_Value::VT_unknown) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAMERROR);
    return false;
  }
  CFX_WideString swQuestion = newParams[0].ToCFXWideString(pRuntime);

  CFX_WideString swTitle = L"PDF";
  if (newParams[1].GetType() != CJS_Value::VT_unknown)
    swTitle = newParams[1].ToCFXWideString(pRuntime);

  CFX_WideString swDefault;
  if (newParams[2].GetType() != CJS_Value::VT_unknown)
    swDefault = newParams[2].ToCFXWideString(pRuntime);

  bool bPassword = false;
  if (newParams[3].GetType() != CJS_Value::VT_unknown)
    bPassword = newParams[3].ToBool(pRuntime);

  CFX_WideString swLabel;
  if (newParams[4].GetType() != CJS_Value::VT_unknown)
    swLabel = newParams[4].ToCFXWideString(pRuntime);

  const int MAX_INPUT_BYTES = 2048;
  std::unique_ptr<char[]> pBuff(new char[MAX_INPUT_BYTES + 2]);
  memset(pBuff.get(), 0, MAX_INPUT_BYTES + 2);

  CJS_Context* pContext = static_cast<CJS_Context*>(cc);
  int nLengthBytes = pContext->GetFormFillEnv()->JS_appResponse(
      swQuestion.c_str(), swTitle.c_str(), swDefault.c_str(), swLabel.c_str(),
      bPassword, pBuff.get(), MAX_INPUT_BYTES);

  if (nLengthBytes < 0 || nLengthBytes > MAX_INPUT_BYTES) {
    sError = JSGetStringFromID(IDS_STRING_JSPARAM_TOOLONG);
    return false;
  }

  vRet = CJS_Value(pRuntime, CFX_WideString::FromUTF16LE(
                                 reinterpret_cast<uint16_t*>(pBuff.get()),
                                 nLengthBytes / sizeof(uint16_t))
                                 .c_str());

  return true;
}

bool app::media(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError) {
  return false;
}

bool app::execDialog(IJS_Context* cc,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError) {
  return true;
}
