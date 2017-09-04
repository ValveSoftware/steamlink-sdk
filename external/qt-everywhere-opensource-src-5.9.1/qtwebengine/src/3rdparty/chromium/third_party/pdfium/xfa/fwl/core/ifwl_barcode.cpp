// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/ifwl_barcode.h"

#include "third_party/base/ptr_util.h"
#include "xfa/fgas/font/fgas_gefont.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfx_barcode.h"
#include "xfa/fwl/core/fwl_noteimp.h"
#include "xfa/fwl/core/ifwl_themeprovider.h"

IFWL_Barcode::IFWL_Barcode(const IFWL_App* app,
                           std::unique_ptr<CFWL_WidgetProperties> properties)
    : IFWL_Edit(app, std::move(properties), nullptr),
      m_dwStatus(0),
      m_type(BC_UNKNOWN) {}

IFWL_Barcode::~IFWL_Barcode() {}

FWL_Type IFWL_Barcode::GetClassID() const {
  return FWL_Type::Barcode;
}

void IFWL_Barcode::Update() {
  if (IsLocked())
    return;

  IFWL_Edit::Update();
  GenerateBarcodeImageCache();
}

void IFWL_Barcode::DrawWidget(CFX_Graphics* pGraphics,
                              const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    return;
  if ((m_pProperties->m_dwStates & FWL_WGTSTATE_Focused) == 0) {
    GenerateBarcodeImageCache();
    if (!m_pBarcodeEngine || (m_dwStatus & XFA_BCS_EncodeSuccess) == 0)
      return;

    CFX_Matrix mt;
    mt.e = GetRTClient().left;
    mt.f = GetRTClient().top;
    if (pMatrix)
      mt.Concat(*pMatrix);

    int32_t errorCode = 0;
    m_pBarcodeEngine->RenderDevice(pGraphics->GetRenderDevice(), pMatrix,
                                   errorCode);
    return;
  }
  IFWL_Edit::DrawWidget(pGraphics, pMatrix);
}

void IFWL_Barcode::GenerateBarcodeImageCache() {
  if ((m_dwStatus & XFA_BCS_NeedUpdate) == 0)
    return;

  m_dwStatus = 0;
  CreateBarcodeEngine();
  IFWL_BarcodeDP* pData =
      static_cast<IFWL_BarcodeDP*>(m_pProperties->m_pDataProvider);
  if (!pData)
    return;
  if (!m_pBarcodeEngine)
    return;

  CFX_WideString wsText;
  GetText(wsText);

  CFWL_ThemePart part;
  part.m_pWidget = this;

  IFWL_ThemeProvider* pTheme = GetAvailableTheme();
  CFGAS_GEFont* pFont = static_cast<CFGAS_GEFont*>(
      pTheme->GetCapacity(&part, CFWL_WidgetCapacity::Font));
  CFX_Font* pCXFont = pFont ? pFont->GetDevFont() : nullptr;
  if (pCXFont)
    m_pBarcodeEngine->SetFont(pCXFont);

  FX_FLOAT* pFontSize = static_cast<FX_FLOAT*>(
      pTheme->GetCapacity(&part, CFWL_WidgetCapacity::FontSize));
  if (pFontSize)
    m_pBarcodeEngine->SetFontSize(*pFontSize);

  FX_ARGB* pFontColor = static_cast<FX_ARGB*>(
      pTheme->GetCapacity(&part, CFWL_WidgetCapacity::TextColor));
  if (pFontColor)
    m_pBarcodeEngine->SetFontColor(*pFontColor);

  m_pBarcodeEngine->SetHeight(int32_t(GetRTClient().height));
  m_pBarcodeEngine->SetWidth(int32_t(GetRTClient().width));
  uint32_t dwAttributeMask = pData->GetBarcodeAttributeMask();
  if (dwAttributeMask & FWL_BCDATTRIBUTE_CHARENCODING)
    m_pBarcodeEngine->SetCharEncoding(pData->GetCharEncoding());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_MODULEHEIGHT)
    m_pBarcodeEngine->SetModuleHeight(pData->GetModuleHeight());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_MODULEWIDTH)
    m_pBarcodeEngine->SetModuleWidth(pData->GetModuleWidth());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_DATALENGTH)
    m_pBarcodeEngine->SetDataLength(pData->GetDataLength());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_CALCHECKSUM)
    m_pBarcodeEngine->SetCalChecksum(pData->GetCalChecksum());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_PRINTCHECKSUM)
    m_pBarcodeEngine->SetPrintChecksum(pData->GetPrintChecksum());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_TEXTLOCATION)
    m_pBarcodeEngine->SetTextLocation(pData->GetTextLocation());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_WIDENARROWRATIO)
    m_pBarcodeEngine->SetWideNarrowRatio(pData->GetWideNarrowRatio());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_STARTCHAR)
    m_pBarcodeEngine->SetStartChar(pData->GetStartChar());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_ENDCHAR)
    m_pBarcodeEngine->SetEndChar(pData->GetEndChar());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_VERSION)
    m_pBarcodeEngine->SetVersion(pData->GetVersion());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_ECLEVEL)
    m_pBarcodeEngine->SetErrorCorrectionLevel(pData->GetErrorCorrectionLevel());
  if (dwAttributeMask & FWL_BCDATTRIBUTE_TRUNCATED)
    m_pBarcodeEngine->SetTruncated(pData->GetTruncated());

  int32_t errorCode = 0;
  m_dwStatus = m_pBarcodeEngine->Encode(wsText.AsStringC(), true, errorCode)
                   ? XFA_BCS_EncodeSuccess
                   : 0;
}

void IFWL_Barcode::CreateBarcodeEngine() {
  if (m_pBarcodeEngine || m_type == BC_UNKNOWN)
    return;

  std::unique_ptr<CFX_Barcode> pBarcode(new CFX_Barcode);
  if (pBarcode->Create(m_type))
    m_pBarcodeEngine = std::move(pBarcode);
}

void IFWL_Barcode::SetType(BC_TYPE type) {
  if (m_type == type)
    return;

  m_pBarcodeEngine.reset();
  m_type = type;
  m_dwStatus = XFA_BCS_NeedUpdate;
}

void IFWL_Barcode::SetText(const CFX_WideString& wsText) {
  m_pBarcodeEngine.reset();
  m_dwStatus = XFA_BCS_NeedUpdate;
  IFWL_Edit::SetText(wsText);
}

bool IFWL_Barcode::IsProtectedType() const {
  if (!m_pBarcodeEngine)
    return true;

  BC_TYPE tEngineType = m_pBarcodeEngine->GetType();
  if (tEngineType == BC_QR_CODE || tEngineType == BC_PDF417 ||
      tEngineType == BC_DATAMATRIX) {
    return true;
  }
  return false;
}

void IFWL_Barcode::OnProcessEvent(CFWL_Event* pEvent) {
  if (pEvent->GetClassID() == CFWL_EventType::TextChanged) {
    m_pBarcodeEngine.reset();
    m_dwStatus = XFA_BCS_NeedUpdate;
  }
  IFWL_Edit::OnProcessEvent(pEvent);
}
