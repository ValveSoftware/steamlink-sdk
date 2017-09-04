// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_WIDGETDATA_H_
#define XFA_FXFA_PARSER_CXFA_WIDGETDATA_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxfa/parser/cxfa_assist.h"
#include "xfa/fxfa/parser/cxfa_bind.h"
#include "xfa/fxfa/parser/cxfa_border.h"
#include "xfa/fxfa/parser/cxfa_calculate.h"
#include "xfa/fxfa/parser/cxfa_caption.h"
#include "xfa/fxfa/parser/cxfa_data.h"
#include "xfa/fxfa/parser/cxfa_font.h"
#include "xfa/fxfa/parser/cxfa_margin.h"
#include "xfa/fxfa/parser/cxfa_para.h"
#include "xfa/fxfa/parser/cxfa_validate.h"
#include "xfa/fxfa/parser/xfa_object.h"

enum XFA_CHECKSTATE {
  XFA_CHECKSTATE_On = 0,
  XFA_CHECKSTATE_Off = 1,
  XFA_CHECKSTATE_Neutral = 2,
};

enum XFA_VALUEPICTURE {
  XFA_VALUEPICTURE_Raw = 0,
  XFA_VALUEPICTURE_Display,
  XFA_VALUEPICTURE_Edit,
  XFA_VALUEPICTURE_DataBind,
};

class CXFA_Node;
class IFX_Locale;

class CXFA_WidgetData : public CXFA_Data {
 public:
  explicit CXFA_WidgetData(CXFA_Node* pNode);

  CXFA_Node* GetUIChild();
  XFA_Element GetUIType();
  CFX_WideString GetRawValue();
  int32_t GetAccess(bool bTemplate = false);
  int32_t GetRotate();
  CXFA_Border GetBorder(bool bModified = false);
  CXFA_Caption GetCaption(bool bModified = false);
  CXFA_Font GetFont(bool bModified = false);
  CXFA_Margin GetMargin(bool bModified = false);
  CXFA_Para GetPara(bool bModified = false);
  void GetEventList(CXFA_NodeArray& events);
  int32_t GetEventByActivity(int32_t iActivity,
                             CXFA_NodeArray& events,
                             bool bIsFormReady = false);
  CXFA_Value GetDefaultValue(bool bModified = false);
  CXFA_Value GetFormValue(bool bModified = false);
  CXFA_Calculate GetCalculate(bool bModified = false);
  CXFA_Validate GetValidate(bool bModified = false);
  CXFA_Bind GetBind(bool bModified = false);
  CXFA_Assist GetAssist(bool bModified = false);
  bool GetWidth(FX_FLOAT& fWidth);
  bool GetHeight(FX_FLOAT& fHeight);
  bool GetMinWidth(FX_FLOAT& fMinWidth);
  bool GetMinHeight(FX_FLOAT& fMinHeight);
  bool GetMaxWidth(FX_FLOAT& fMaxWidth);
  bool GetMaxHeight(FX_FLOAT& fMaxHeight);
  CXFA_Border GetUIBorder(bool bModified = false);
  CXFA_Margin GetUIMargin(bool bModified = false);
  void GetUIMargin(CFX_RectF& rtUIMargin);
  int32_t GetButtonHighlight();
  bool GetButtonRollover(CFX_WideString& wsRollover, bool& bRichText);
  bool GetButtonDown(CFX_WideString& wsDown, bool& bRichText);
  int32_t GetCheckButtonShape();
  int32_t GetCheckButtonMark();
  FX_FLOAT GetCheckButtonSize();
  bool IsAllowNeutral();
  bool IsRadioButton();
  XFA_CHECKSTATE GetCheckState();
  void SetCheckState(XFA_CHECKSTATE eCheckState, bool bNotify);
  CXFA_Node* GetExclGroupNode();
  CXFA_Node* GetSelectedMember();
  CXFA_Node* SetSelectedMember(const CFX_WideStringC& wsName, bool bNotify);
  void SetSelectedMemberByValue(const CFX_WideStringC& wsValue,
                                bool bNotify,
                                bool bScriptModify,
                                bool bSyncData);
  CXFA_Node* GetExclGroupFirstMember();
  CXFA_Node* GetExclGroupNextMember(CXFA_Node* pNode);
  int32_t GetChoiceListCommitOn();
  bool IsChoiceListAllowTextEntry();
  int32_t GetChoiceListOpen();
  bool IsListBox();
  int32_t CountChoiceListItems(bool bSaveValue = false);
  bool GetChoiceListItem(CFX_WideString& wsText,
                         int32_t nIndex,
                         bool bSaveValue = false);
  void GetChoiceListItems(CFX_WideStringArray& wsTextArray,
                          bool bSaveValue = false);
  int32_t CountSelectedItems();
  int32_t GetSelectedItem(int32_t nIndex = 0);
  void GetSelectedItems(CFX_Int32Array& iSelArray);
  void GetSelectedItemsValue(CFX_WideStringArray& wsSelTextArray);
  bool GetItemState(int32_t nIndex);
  void SetItemState(int32_t nIndex,
                    bool bSelected,
                    bool bNotify,
                    bool bScriptModify,
                    bool bSyncData);
  void SetSelectedItems(CFX_Int32Array& iSelArray,
                        bool bNotify,
                        bool bScriptModify,
                        bool bSyncData);
  void ClearAllSelections();
  void InsertItem(const CFX_WideString& wsLabel,
                  const CFX_WideString& wsValue,
                  int32_t nIndex = -1,
                  bool bNotify = false);
  void GetItemLabel(const CFX_WideStringC& wsValue, CFX_WideString& wsLabel);
  void GetItemValue(const CFX_WideStringC& wsLabel, CFX_WideString& wsValue);
  bool DeleteItem(int32_t nIndex,
                  bool bNotify = false,
                  bool bScriptModify = false,
                  bool bSyncData = true);
  int32_t GetHorizontalScrollPolicy();
  int32_t GetNumberOfCells();
  bool SetValue(const CFX_WideString& wsValue, XFA_VALUEPICTURE eValueType);
  bool GetPictureContent(CFX_WideString& wsPicture, XFA_VALUEPICTURE ePicture);
  IFX_Locale* GetLocal();
  bool GetValue(CFX_WideString& wsValue, XFA_VALUEPICTURE eValueType);
  bool GetNormalizeDataValue(const CFX_WideString& wsValue,
                             CFX_WideString& wsNormalizeValue);
  bool GetFormatDataValue(const CFX_WideString& wsValue,
                          CFX_WideString& wsFormattedValue);
  void NormalizeNumStr(const CFX_WideString& wsValue, CFX_WideString& wsOutput);
  CFX_WideString GetBarcodeType();
  bool GetBarcodeAttribute_CharEncoding(int32_t& val);
  bool GetBarcodeAttribute_Checksum(bool& val);
  bool GetBarcodeAttribute_DataLength(int32_t& val);
  bool GetBarcodeAttribute_StartChar(FX_CHAR& val);
  bool GetBarcodeAttribute_EndChar(FX_CHAR& val);
  bool GetBarcodeAttribute_ECLevel(int32_t& val);
  bool GetBarcodeAttribute_ModuleWidth(int32_t& val);
  bool GetBarcodeAttribute_ModuleHeight(int32_t& val);
  bool GetBarcodeAttribute_PrintChecksum(bool& val);
  bool GetBarcodeAttribute_TextLocation(int32_t& val);
  bool GetBarcodeAttribute_Truncate(bool& val);
  bool GetBarcodeAttribute_WideNarrowRatio(FX_FLOAT& val);
  void GetPasswordChar(CFX_WideString& wsPassWord);
  bool IsMultiLine();
  int32_t GetVerticalScrollPolicy();
  int32_t GetMaxChars(XFA_Element& eType);
  bool GetFracDigits(int32_t& iFracDigits);
  bool GetLeadDigits(int32_t& iLeadDigits);

  CFX_WideString NumericLimit(const CFX_WideString& wsValue,
                              int32_t iLead,
                              int32_t iTread) const;

  bool m_bIsNull;
  bool m_bPreNull;

 protected:
  void SyncValue(const CFX_WideString& wsValue, bool bNotify);
  void InsertListTextItem(CXFA_Node* pItems,
                          const CFX_WideString& wsText,
                          int32_t nIndex = -1);
  void FormatNumStr(const CFX_WideString& wsValue,
                    IFX_Locale* pLocale,
                    CFX_WideString& wsOutput);

  CXFA_Node* m_pUiChildNode;
  XFA_Element m_eUIType;
};

#endif  // XFA_FXFA_PARSER_CXFA_WIDGETDATA_H_
