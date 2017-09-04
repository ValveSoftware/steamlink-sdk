// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_LOCALE_H_
#define XFA_FXFA_PARSER_XFA_LOCALE_H_

#include <memory>

#include "xfa/fgas/localization/fgas_locale.h"
#include "xfa/fxfa/parser/xfa_object.h"

class CXFA_XMLLocale : public IFX_Locale {
 public:
  explicit CXFA_XMLLocale(std::unique_ptr<CXML_Element> pLocaleData);
  ~CXFA_XMLLocale() override;

  // IFX_Locale
  CFX_WideString GetName() const override;
  void GetNumbericSymbol(FX_LOCALENUMSYMBOL eType,
                         CFX_WideString& wsNumSymbol) const override;

  void GetDateTimeSymbols(CFX_WideString& wsDtSymbol) const override;
  void GetMonthName(int32_t nMonth,
                    CFX_WideString& wsMonthName,
                    bool bAbbr = true) const override;
  void GetDayName(int32_t nWeek,
                  CFX_WideString& wsDayName,
                  bool bAbbr = true) const override;
  void GetMeridiemName(CFX_WideString& wsMeridiemName,
                       bool bAM = true) const override;
  void GetTimeZone(FX_TIMEZONE* tz) const override;
  void GetEraName(CFX_WideString& wsEraName, bool bAD = true) const override;

  void GetDatePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                      CFX_WideString& wsPattern) const override;
  void GetTimePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                      CFX_WideString& wsPattern) const override;
  void GetNumPattern(FX_LOCALENUMSUBCATEGORY eType,
                     CFX_WideString& wsPattern) const override;

 protected:
  void GetPattern(CXML_Element* pElement,
                  const CFX_ByteStringC& bsTag,
                  const CFX_WideStringC& wsName,
                  CFX_WideString& wsPattern) const;
  CFX_WideString GetCalendarSymbol(const CFX_ByteStringC& symbol,
                                   int index,
                                   bool bAbbr) const;

 private:
  std::unique_ptr<CXML_Element> m_pLocaleData;
};

class CXFA_NodeLocale : public IFX_Locale {
 public:
  CXFA_NodeLocale(CXFA_Node* pLocale);
  ~CXFA_NodeLocale() override;

  // IFX_Locale
  CFX_WideString GetName() const override;
  void GetNumbericSymbol(FX_LOCALENUMSYMBOL eType,
                         CFX_WideString& wsNumSymbol) const override;

  void GetDateTimeSymbols(CFX_WideString& wsDtSymbol) const override;
  void GetMonthName(int32_t nMonth,
                    CFX_WideString& wsMonthName,
                    bool bAbbr = true) const override;
  void GetDayName(int32_t nWeek,
                  CFX_WideString& wsDayName,
                  bool bAbbr = true) const override;
  void GetMeridiemName(CFX_WideString& wsMeridiemName,
                       bool bAM = true) const override;
  void GetTimeZone(FX_TIMEZONE* tz) const override;
  void GetEraName(CFX_WideString& wsEraName, bool bAD = true) const override;

  void GetDatePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                      CFX_WideString& wsPattern) const override;
  void GetTimePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                      CFX_WideString& wsPattern) const override;
  void GetNumPattern(FX_LOCALENUMSUBCATEGORY eType,
                     CFX_WideString& wsPattern) const override;

 protected:
  CXFA_Node* GetNodeByName(CXFA_Node* pParent,
                           const CFX_WideStringC& wsName) const;
  CFX_WideString GetSymbol(XFA_Element eElement,
                           const CFX_WideStringC& symbol_type) const;
  CFX_WideString GetCalendarSymbol(XFA_Element eElement,
                                   int index,
                                   bool bAbbr) const;

  CXFA_Node* const m_pLocale;
};

#endif  // XFA_FXFA_PARSER_XFA_LOCALE_H_
