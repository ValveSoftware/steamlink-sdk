// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_JAVASCRIPT_PUBLICMETHODS_H_
#define FPDFSDK_JAVASCRIPT_PUBLICMETHODS_H_

#include <string>
#include <vector>

#include "fpdfsdk/javascript/JS_Define.h"

class CJS_PublicMethods : public CJS_Object {
 public:
  explicit CJS_PublicMethods(v8::Local<v8::Object> pObject)
      : CJS_Object(pObject) {}
  ~CJS_PublicMethods() override {}

  static bool AFNumber_Format(IJS_Context* cc,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError);
  static bool AFNumber_Keystroke(IJS_Context* cc,
                                 const std::vector<CJS_Value>& params,
                                 CJS_Value& vRet,
                                 CFX_WideString& sError);
  static bool AFPercent_Format(IJS_Context* cc,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError);
  static bool AFPercent_Keystroke(IJS_Context* cc,
                                  const std::vector<CJS_Value>& params,
                                  CJS_Value& vRet,
                                  CFX_WideString& sError);
  static bool AFDate_FormatEx(IJS_Context* cc,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError);
  static bool AFDate_KeystrokeEx(IJS_Context* cc,
                                 const std::vector<CJS_Value>& params,
                                 CJS_Value& vRet,
                                 CFX_WideString& sError);
  static bool AFDate_Format(IJS_Context* cc,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError);
  static bool AFDate_Keystroke(IJS_Context* cc,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError);
  static bool AFTime_FormatEx(IJS_Context* cc,
                              const std::vector<CJS_Value>& params,
                              CJS_Value& vRet,
                              CFX_WideString& sError);  //
  static bool AFTime_KeystrokeEx(IJS_Context* cc,
                                 const std::vector<CJS_Value>& params,
                                 CJS_Value& vRet,
                                 CFX_WideString& sError);
  static bool AFTime_Format(IJS_Context* cc,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError);
  static bool AFTime_Keystroke(IJS_Context* cc,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError);
  static bool AFSpecial_Format(IJS_Context* cc,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError);
  static bool AFSpecial_Keystroke(IJS_Context* cc,
                                  const std::vector<CJS_Value>& params,
                                  CJS_Value& vRet,
                                  CFX_WideString& sError);
  static bool AFSpecial_KeystrokeEx(IJS_Context* cc,
                                    const std::vector<CJS_Value>& params,
                                    CJS_Value& vRet,
                                    CFX_WideString& sError);  //
  static bool AFSimple(IJS_Context* cc,
                       const std::vector<CJS_Value>& params,
                       CJS_Value& vRet,
                       CFX_WideString& sError);
  static bool AFMakeNumber(IJS_Context* cc,
                           const std::vector<CJS_Value>& params,
                           CJS_Value& vRet,
                           CFX_WideString& sError);
  static bool AFSimple_Calculate(IJS_Context* cc,
                                 const std::vector<CJS_Value>& params,
                                 CJS_Value& vRet,
                                 CFX_WideString& sError);
  static bool AFRange_Validate(IJS_Context* cc,
                               const std::vector<CJS_Value>& params,
                               CJS_Value& vRet,
                               CFX_WideString& sError);
  static bool AFMergeChange(IJS_Context* cc,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError);
  static bool AFParseDateEx(IJS_Context* cc,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError);
  static bool AFExtractNums(IJS_Context* cc,
                            const std::vector<CJS_Value>& params,
                            CJS_Value& vRet,
                            CFX_WideString& sError);

  JS_STATIC_GLOBAL_FUN(AFNumber_Format);
  JS_STATIC_GLOBAL_FUN(AFNumber_Keystroke);
  JS_STATIC_GLOBAL_FUN(AFPercent_Format);
  JS_STATIC_GLOBAL_FUN(AFPercent_Keystroke);
  JS_STATIC_GLOBAL_FUN(AFDate_FormatEx);
  JS_STATIC_GLOBAL_FUN(AFDate_KeystrokeEx);
  JS_STATIC_GLOBAL_FUN(AFDate_Format);
  JS_STATIC_GLOBAL_FUN(AFDate_Keystroke);
  JS_STATIC_GLOBAL_FUN(AFTime_FormatEx);
  JS_STATIC_GLOBAL_FUN(AFTime_KeystrokeEx);
  JS_STATIC_GLOBAL_FUN(AFTime_Format);
  JS_STATIC_GLOBAL_FUN(AFTime_Keystroke);
  JS_STATIC_GLOBAL_FUN(AFSpecial_Format);
  JS_STATIC_GLOBAL_FUN(AFSpecial_Keystroke);
  JS_STATIC_GLOBAL_FUN(AFSpecial_KeystrokeEx);
  JS_STATIC_GLOBAL_FUN(AFSimple);
  JS_STATIC_GLOBAL_FUN(AFMakeNumber);
  JS_STATIC_GLOBAL_FUN(AFSimple_Calculate);
  JS_STATIC_GLOBAL_FUN(AFRange_Validate);
  JS_STATIC_GLOBAL_FUN(AFMergeChange);
  JS_STATIC_GLOBAL_FUN(AFParseDateEx);
  JS_STATIC_GLOBAL_FUN(AFExtractNums);

  JS_STATIC_DECLARE_GLOBAL_FUN();

  static int ParseStringInteger(const CFX_WideString& string,
                                int nStart,
                                int& nSkip,
                                int nMaxStep);
  static CFX_WideString ParseStringString(const CFX_WideString& string,
                                          int nStart,
                                          int& nSkip);
  static double MakeRegularDate(const CFX_WideString& value,
                                const CFX_WideString& format,
                                bool* bWrongFormat);
  static CFX_WideString MakeFormatDate(double dDate,
                                       const CFX_WideString& format);
  static double ParseNormalDate(const CFX_WideString& value,
                                bool* bWrongFormat);
  static double MakeInterDate(const CFX_WideString& value);

  static bool IsNumber(const CFX_WideString& str);

  static bool maskSatisfied(wchar_t c_Change, wchar_t c_Mask);
  static bool isReservedMaskChar(wchar_t ch);

  static double AF_Simple(const FX_WCHAR* sFuction,
                          double dValue1,
                          double dValue2);
  static CJS_Array AF_MakeArrayFromList(CJS_Runtime* pRuntime, CJS_Value val);
};

#endif  // FPDFSDK_JAVASCRIPT_PUBLICMETHODS_H_
