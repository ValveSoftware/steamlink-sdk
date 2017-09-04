// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_APP_H_
#define FPDFSDK_JAVASCRIPT_APP_H_

#include <memory>
#include <unordered_set>
#include <vector>

#include "fpdfsdk/javascript/JS_Define.h"

class CJS_Runtime;
class GlobalTimer;

class TimerObj : public CJS_EmbedObj {
 public:
  TimerObj(CJS_Object* pJSObject);
  ~TimerObj() override;

  void SetTimer(GlobalTimer* pTimer);
  int GetTimerID() const { return m_nTimerID; }

 private:
  int m_nTimerID;  // Weak reference to GlobalTimer through global map.
};

class CJS_TimerObj : public CJS_Object {
 public:
  CJS_TimerObj(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_TimerObj() override {}

  DECLARE_JS_CLASS();
};

class app : public CJS_EmbedObj {
 public:
  app(CJS_Object* pJSObject);
  ~app() override;

  bool activeDocs(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool calculate(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool formsVersion(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool fs(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool fullscreen(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool language(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool media(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool platform(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool runtimeHighlight(IJS_Context* cc,
                        CJS_PropValue& vp,
                        CFX_WideString& sError);
  bool viewerType(IJS_Context* cc, CJS_PropValue& vp, CFX_WideString& sError);
  bool viewerVariation(IJS_Context* cc,
                       CJS_PropValue& vp,
                       CFX_WideString& sError);
  bool viewerVersion(IJS_Context* cc,
                     CJS_PropValue& vp,
                     CFX_WideString& sError);

  bool alert(IJS_Context* cc,
             const std::vector<CJS_Value>& params,
             CJS_Value& vRet,
             CFX_WideString& sError);
  bool beep(IJS_Context* cc,
            const std::vector<CJS_Value>& params,
            CJS_Value& vRet,
            CFX_WideString& sError);
  bool browseForDoc(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  bool clearInterval(IJS_Context* cc,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError);
  bool clearTimeOut(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  bool execDialog(IJS_Context* cc,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError);
  bool execMenuItem(IJS_Context* cc,
                    const std::vector<CJS_Value>& params,
                    CJS_Value& vRet,
                    CFX_WideString& sError);
  bool findComponent(IJS_Context* cc,
                     const std::vector<CJS_Value>& params,
                     CJS_Value& vRet,
                     CFX_WideString& sError);
  bool goBack(IJS_Context* cc,
              const std::vector<CJS_Value>& params,
              CJS_Value& vRet,
              CFX_WideString& sError);
  bool goForward(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  bool launchURL(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  bool mailMsg(IJS_Context* cc,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError);
  bool newFDF(IJS_Context* cc,
              const std::vector<CJS_Value>& params,
              CJS_Value& vRet,
              CFX_WideString& sError);
  bool newDoc(IJS_Context* cc,
              const std::vector<CJS_Value>& params,
              CJS_Value& vRet,
              CFX_WideString& sError);
  bool openDoc(IJS_Context* cc,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError);
  bool openFDF(IJS_Context* cc,
               const std::vector<CJS_Value>& params,
               CJS_Value& vRet,
               CFX_WideString& sError);
  bool popUpMenuEx(IJS_Context* cc,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError);
  bool popUpMenu(IJS_Context* cc,
                 const std::vector<CJS_Value>& params,
                 CJS_Value& vRet,
                 CFX_WideString& sError);
  bool response(IJS_Context* cc,
                const std::vector<CJS_Value>& params,
                CJS_Value& vRet,
                CFX_WideString& sError);
  bool setInterval(IJS_Context* cc,
                   const std::vector<CJS_Value>& params,
                   CJS_Value& vRet,
                   CFX_WideString& sError);
  bool setTimeOut(IJS_Context* cc,
                  const std::vector<CJS_Value>& params,
                  CJS_Value& vRet,
                  CFX_WideString& sError);

  void TimerProc(GlobalTimer* pTimer);
  void CancelProc(GlobalTimer* pTimer);

  static CFX_WideString SysPathToPDFPath(const CFX_WideString& sOldPath);

 private:
  // CJS_EmbedObj
  void RunJsScript(CJS_Runtime* pRuntime, const CFX_WideString& wsScript);

  void ClearTimerCommon(CJS_Runtime* pRuntime, const CJS_Value& param);

  bool m_bCalculate;
  bool m_bRuntimeHighLight;
  std::unordered_set<std::unique_ptr<GlobalTimer>> m_Timers;
};

class CJS_App : public CJS_Object {
 public:
  explicit CJS_App(v8::Local<v8::Object> pObject) : CJS_Object(pObject) {}
  ~CJS_App() override {}

  DECLARE_JS_CLASS();

  JS_STATIC_PROP(activeDocs, app);
  JS_STATIC_PROP(calculate, app);
  JS_STATIC_PROP(formsVersion, app);
  JS_STATIC_PROP(fs, app);
  JS_STATIC_PROP(fullscreen, app);
  JS_STATIC_PROP(language, app);
  JS_STATIC_PROP(media, app);
  JS_STATIC_PROP(platform, app);
  JS_STATIC_PROP(runtimeHighlight, app);
  JS_STATIC_PROP(viewerType, app);
  JS_STATIC_PROP(viewerVariation, app);
  JS_STATIC_PROP(viewerVersion, app);

  JS_STATIC_METHOD(alert, app);
  JS_STATIC_METHOD(beep, app);
  JS_STATIC_METHOD(browseForDoc, app);
  JS_STATIC_METHOD(clearInterval, app);
  JS_STATIC_METHOD(clearTimeOut, app);
  JS_STATIC_METHOD(execDialog, app);
  JS_STATIC_METHOD(execMenuItem, app);
  JS_STATIC_METHOD(findComponent, app);
  JS_STATIC_METHOD(goBack, app);
  JS_STATIC_METHOD(goForward, app);
  JS_STATIC_METHOD(launchURL, app);
  JS_STATIC_METHOD(mailMsg, app);
  JS_STATIC_METHOD(newFDF, app);
  JS_STATIC_METHOD(newDoc, app);
  JS_STATIC_METHOD(openDoc, app);
  JS_STATIC_METHOD(openFDF, app);
  JS_STATIC_METHOD(popUpMenuEx, app);
  JS_STATIC_METHOD(popUpMenu, app);
  JS_STATIC_METHOD(response, app);
  JS_STATIC_METHOD(setInterval, app);
  JS_STATIC_METHOD(setTimeOut, app);
};

#endif  // FPDFSDK_JAVASCRIPT_APP_H_
