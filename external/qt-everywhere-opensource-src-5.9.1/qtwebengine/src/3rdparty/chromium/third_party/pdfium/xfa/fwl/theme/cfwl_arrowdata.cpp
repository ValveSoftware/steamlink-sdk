// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/theme/cfwl_arrowdata.h"

#include <algorithm>

namespace {

CFWL_ArrowData* g_pInstance = nullptr;

}  // namespace

CFWL_ArrowData* CFWL_ArrowData::GetInstance() {
  if (!g_pInstance)
    g_pInstance = new CFWL_ArrowData;
  return g_pInstance;
}

bool CFWL_ArrowData::HasInstance() {
  return !!g_pInstance;
}

void CFWL_ArrowData::DestroyInstance() {
  delete g_pInstance;
  g_pInstance = nullptr;
}

CFWL_ArrowData::~CFWL_ArrowData() {}

void CFWL_ArrowData::SetColorData(uint32_t dwID) {
  if (!m_pColorData)
    m_pColorData.reset(new CColorData);

  if (dwID) {
    m_pColorData->clrBorder[0] = ArgbEncode(255, 142, 153, 125);
    m_pColorData->clrBorder[1] = ArgbEncode(255, 157, 171, 119);
    m_pColorData->clrBorder[2] = ArgbEncode(255, 118, 131, 97);
    m_pColorData->clrBorder[3] = ArgbEncode(255, 172, 168, 153);
    m_pColorData->clrStart[0] = ArgbEncode(255, 203, 215, 186);
    m_pColorData->clrStart[1] = ArgbEncode(255, 218, 232, 185);
    m_pColorData->clrStart[2] = ArgbEncode(255, 203, 215, 186);
    m_pColorData->clrStart[3] = ArgbEncode(255, 254, 254, 251);
    m_pColorData->clrEnd[0] = ArgbEncode(255, 149, 167, 117);
    m_pColorData->clrEnd[1] = ArgbEncode(255, 198, 211, 155);
    m_pColorData->clrEnd[2] = ArgbEncode(255, 149, 167, 117);
    m_pColorData->clrEnd[3] = ArgbEncode(255, 243, 241, 236);
    m_pColorData->clrSign[0] = ArgbEncode(255, 255, 255, 255);
    m_pColorData->clrSign[1] = ArgbEncode(255, 255, 255, 255);
    m_pColorData->clrSign[2] = ArgbEncode(255, 255, 255, 255);
    m_pColorData->clrSign[3] = ArgbEncode(255, 128, 128, 128);
  } else {
    m_pColorData->clrBorder[0] = ArgbEncode(255, 202, 216, 249);
    m_pColorData->clrBorder[1] = ArgbEncode(255, 171, 190, 233);
    m_pColorData->clrBorder[2] = ArgbEncode(255, 135, 147, 219);
    m_pColorData->clrBorder[3] = ArgbEncode(255, 172, 168, 153);
    m_pColorData->clrStart[0] = ArgbEncode(255, 225, 234, 254);
    m_pColorData->clrStart[1] = ArgbEncode(255, 253, 255, 255);
    m_pColorData->clrStart[2] = ArgbEncode(255, 110, 142, 241);
    m_pColorData->clrStart[3] = ArgbEncode(255, 254, 254, 251);
    m_pColorData->clrEnd[0] = ArgbEncode(255, 175, 204, 251);
    m_pColorData->clrEnd[1] = ArgbEncode(255, 185, 218, 251);
    m_pColorData->clrEnd[2] = ArgbEncode(255, 210, 222, 235);
    m_pColorData->clrEnd[3] = ArgbEncode(255, 243, 241, 236);
    m_pColorData->clrSign[0] = ArgbEncode(255, 77, 97, 133);
    m_pColorData->clrSign[1] = ArgbEncode(255, 77, 97, 133);
    m_pColorData->clrSign[2] = ArgbEncode(255, 77, 97, 133);
    m_pColorData->clrSign[3] = ArgbEncode(255, 128, 128, 128);
  }
}
