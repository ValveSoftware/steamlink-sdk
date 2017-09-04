// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXBARCODE_DATAMATRIX_BC_SYMBOLINFO_H_
#define XFA_FXBARCODE_DATAMATRIX_BC_SYMBOLINFO_H_

#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fxbarcode/datamatrix/BC_SymbolShapeHint.h"

class CBC_Dimension;

class CBC_SymbolInfo : public CBC_SymbolShapeHint {
 public:
  CBC_SymbolInfo(bool rectangular,
                 int32_t dataCapacity,
                 int32_t errorCodewords,
                 int32_t matrixWidth,
                 int32_t matrixHeight,
                 int32_t dataRegions);
  ~CBC_SymbolInfo() override;

  static void Initialize();
  static void Finalize();
  static void overrideSymbolSet(CBC_SymbolInfo* override);
  static CBC_SymbolInfo* lookup(int32_t dataCodewords, int32_t& e);
  static CBC_SymbolInfo* lookup(int32_t dataCodewords,
                                SymbolShapeHint shape,
                                int32_t& e);
  static CBC_SymbolInfo* lookup(int32_t dataCodewords,
                                bool allowRectangular,
                                bool fail,
                                int32_t& e);
  static CBC_SymbolInfo* lookup(int32_t dataCodewords,
                                SymbolShapeHint shape,
                                bool fail,
                                int32_t& e);
  static CBC_SymbolInfo* lookup(int32_t dataCodewords,
                                SymbolShapeHint shape,
                                CBC_Dimension* minSize,
                                CBC_Dimension* maxSize,
                                bool fail,
                                int32_t& e);
  int32_t getHorizontalDataRegions(int32_t& e);
  int32_t getVerticalDataRegions(int32_t& e);
  int32_t getSymbolDataWidth(int32_t& e);
  int32_t getSymbolDataHeight(int32_t& e);
  int32_t getSymbolWidth(int32_t& e);
  int32_t getSymbolHeight(int32_t& e);
  int32_t getCodewordCount();
  int32_t getInterleavedBlockCount();
  int32_t getDataLengthForInterleavedBlock(int32_t index);
  int32_t getErrorLengthForInterleavedBlock(int32_t index);

  int32_t m_dataCapacity;
  int32_t m_errorCodewords;
  int32_t m_matrixWidth;
  int32_t m_matrixHeight;
  int32_t m_rsBlockData;
  int32_t m_rsBlockError;

 private:
  CBC_SymbolInfo(bool rectangular,
                 int32_t dataCapacity,
                 int32_t errorCodewords,
                 int32_t matrixWidth,
                 int32_t matrixHeight,
                 int32_t dataRegions,
                 int32_t rsBlockData,
                 int32_t rsBlockError);

  bool m_rectangular;
  int32_t m_dataRegions;
};

#endif  // XFA_FXBARCODE_DATAMATRIX_BC_SYMBOLINFO_H_
