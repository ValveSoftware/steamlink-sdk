// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_BARCODE_H_
#define XFA_FWL_CORE_IFWL_BARCODE_H_

#include <memory>

#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_edit.h"
#include "xfa/fwl/core/ifwl_scrollbar.h"
#include "xfa/fxbarcode/BC_Library.h"

class CFWL_WidgetProperties;
class CFX_Barcode;
class IFWL_Widget;

#define XFA_BCS_NeedUpdate 0x0001
#define XFA_BCS_EncodeSuccess 0x0002

enum FWL_BCDAttribute {
  FWL_BCDATTRIBUTE_NONE = 0,
  FWL_BCDATTRIBUTE_CHARENCODING = 1 << 0,
  FWL_BCDATTRIBUTE_MODULEHEIGHT = 1 << 1,
  FWL_BCDATTRIBUTE_MODULEWIDTH = 1 << 2,
  FWL_BCDATTRIBUTE_DATALENGTH = 1 << 3,
  FWL_BCDATTRIBUTE_CALCHECKSUM = 1 << 4,
  FWL_BCDATTRIBUTE_PRINTCHECKSUM = 1 << 5,
  FWL_BCDATTRIBUTE_TEXTLOCATION = 1 << 6,
  FWL_BCDATTRIBUTE_WIDENARROWRATIO = 1 << 7,
  FWL_BCDATTRIBUTE_STARTCHAR = 1 << 8,
  FWL_BCDATTRIBUTE_ENDCHAR = 1 << 9,
  FWL_BCDATTRIBUTE_VERSION = 1 << 10,
  FWL_BCDATTRIBUTE_ECLEVEL = 1 << 11,
  FWL_BCDATTRIBUTE_TRUNCATED = 1 << 12
};

class IFWL_BarcodeDP : public IFWL_DataProvider {
 public:
  virtual BC_CHAR_ENCODING GetCharEncoding() const = 0;
  virtual int32_t GetModuleHeight() const = 0;
  virtual int32_t GetModuleWidth() const = 0;
  virtual int32_t GetDataLength() const = 0;
  virtual bool GetCalChecksum() const = 0;
  virtual bool GetPrintChecksum() const = 0;
  virtual BC_TEXT_LOC GetTextLocation() const = 0;
  virtual int32_t GetWideNarrowRatio() const = 0;
  virtual FX_CHAR GetStartChar() const = 0;
  virtual FX_CHAR GetEndChar() const = 0;
  virtual int32_t GetVersion() const = 0;
  virtual int32_t GetErrorCorrectionLevel() const = 0;
  virtual bool GetTruncated() const = 0;
  virtual uint32_t GetBarcodeAttributeMask() const = 0;
};

class IFWL_Barcode : public IFWL_Edit {
 public:
  IFWL_Barcode(const IFWL_App* app,
               std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_Barcode() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;

  // IFWL_Edit
  void SetText(const CFX_WideString& wsText) override;

  void SetType(BC_TYPE type);
  bool IsProtectedType() const;

 private:
  void GenerateBarcodeImageCache();
  void CreateBarcodeEngine();

  std::unique_ptr<CFX_Barcode> m_pBarcodeEngine;
  uint32_t m_dwStatus;
  BC_TYPE m_type;
};

#endif  // XFA_FWL_CORE_IFWL_BARCODE_H_
