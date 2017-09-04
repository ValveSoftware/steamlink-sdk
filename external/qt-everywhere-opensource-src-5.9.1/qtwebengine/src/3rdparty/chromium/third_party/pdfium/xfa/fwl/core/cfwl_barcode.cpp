// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/core/cfwl_barcode.h"

#include <memory>

#include "third_party/base/ptr_util.h"

namespace {

IFWL_Barcode* ToBarcode(IFWL_Widget* widget) {
  return static_cast<IFWL_Barcode*>(widget);
}

}  // namespace

CFWL_Barcode::CFWL_Barcode(const IFWL_App* app)
    : CFWL_Edit(app), m_dwAttributeMask(FWL_BCDATTRIBUTE_NONE) {}

CFWL_Barcode::~CFWL_Barcode() {}

void CFWL_Barcode::Initialize() {
  ASSERT(!m_pIface);

  m_pIface = pdfium::MakeUnique<IFWL_Barcode>(
      m_pApp, pdfium::MakeUnique<CFWL_WidgetProperties>(this));

  CFWL_Widget::Initialize();
}

void CFWL_Barcode::SetCharEncoding(BC_CHAR_ENCODING encoding) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_CHARENCODING;
  m_eCharEncoding = encoding;
}

void CFWL_Barcode::SetModuleHeight(int32_t height) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_MODULEHEIGHT;
  m_nModuleHeight = height;
}

void CFWL_Barcode::SetModuleWidth(int32_t width) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_MODULEWIDTH;
  m_nModuleWidth = width;
}

void CFWL_Barcode::SetDataLength(int32_t dataLength) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_DATALENGTH;
  m_nDataLength = dataLength;
  ToBarcode(GetWidget())->SetLimit(dataLength);
}

void CFWL_Barcode::SetCalChecksum(bool calChecksum) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_CALCHECKSUM;
  m_bCalChecksum = calChecksum;
}

void CFWL_Barcode::SetPrintChecksum(bool printChecksum) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_PRINTCHECKSUM;
  m_bPrintChecksum = printChecksum;
}

void CFWL_Barcode::SetTextLocation(BC_TEXT_LOC location) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_TEXTLOCATION;
  m_eTextLocation = location;
}

void CFWL_Barcode::SetWideNarrowRatio(int32_t ratio) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_WIDENARROWRATIO;
  m_nWideNarrowRatio = ratio;
}

void CFWL_Barcode::SetStartChar(FX_CHAR startChar) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_STARTCHAR;
  m_cStartChar = startChar;
}

void CFWL_Barcode::SetEndChar(FX_CHAR endChar) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_ENDCHAR;
  m_cEndChar = endChar;
}

void CFWL_Barcode::SetErrorCorrectionLevel(int32_t ecLevel) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_ECLEVEL;
  m_nECLevel = ecLevel;
}

void CFWL_Barcode::SetTruncated(bool truncated) {
  m_dwAttributeMask |= FWL_BCDATTRIBUTE_TRUNCATED;
  m_bTruncated = truncated;
}

void CFWL_Barcode::SetType(BC_TYPE type) {
  if (GetWidget())
    ToBarcode(GetWidget())->SetType(type);
}

bool CFWL_Barcode::IsProtectedType() {
  return GetWidget() ? ToBarcode(GetWidget())->IsProtectedType() : false;
}

void CFWL_Barcode::GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) {
}

BC_CHAR_ENCODING CFWL_Barcode::GetCharEncoding() const {
  return m_eCharEncoding;
}

int32_t CFWL_Barcode::GetModuleHeight() const {
  return m_nModuleHeight;
}

int32_t CFWL_Barcode::GetModuleWidth() const {
  return m_nModuleWidth;
}

int32_t CFWL_Barcode::GetDataLength() const {
  return m_nDataLength;
}

bool CFWL_Barcode::GetCalChecksum() const {
  return m_bCalChecksum;
}

bool CFWL_Barcode::GetPrintChecksum() const {
  return m_bPrintChecksum;
}

BC_TEXT_LOC CFWL_Barcode::GetTextLocation() const {
  return m_eTextLocation;
}

int32_t CFWL_Barcode::GetWideNarrowRatio() const {
  return m_nWideNarrowRatio;
}

FX_CHAR CFWL_Barcode::GetStartChar() const {
  return m_cStartChar;
}

FX_CHAR CFWL_Barcode::GetEndChar() const {
  return m_cEndChar;
}

int32_t CFWL_Barcode::GetVersion() const {
  return 0;
}

int32_t CFWL_Barcode::GetErrorCorrectionLevel() const {
  return m_nECLevel;
}

bool CFWL_Barcode::GetTruncated() const {
  return m_bTruncated;
}

uint32_t CFWL_Barcode::GetBarcodeAttributeMask() const {
  return m_dwAttributeMask;
}
