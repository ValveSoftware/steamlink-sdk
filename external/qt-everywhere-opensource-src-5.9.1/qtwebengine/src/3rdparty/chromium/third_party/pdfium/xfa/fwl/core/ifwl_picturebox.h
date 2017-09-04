// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_PICTUREBOX_H_
#define XFA_FWL_CORE_IFWL_PICTUREBOX_H_

#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/ifwl_dataprovider.h"
#include "xfa/fwl/core/ifwl_widget.h"

#define FWL_STYLEEXT_PTB_Left 0L << 0
#define FWL_STYLEEXT_PTB_Center 1L << 0
#define FWL_STYLEEXT_PTB_Right 2L << 0
#define FWL_STYLEEXT_PTB_Top 0L << 2
#define FWL_STYLEEXT_PTB_Vcenter 1L << 2
#define FWL_STYLEEXT_PTB_Bottom 2L << 2
#define FWL_STYLEEXT_PTB_Normal 0L << 4
#define FWL_STYLEEXT_PTB_AutoSize 1L << 4
#define FWL_STYLEEXT_PTB_StretchImage 2L << 4
#define FWL_STYLEEXT_PTB_StretchHImage 3L << 4
#define FWL_STYLEEXT_PTB_StretchVImage 4L << 4
#define FWL_STYLEEXT_PTB_HAlignMask 3L << 0
#define FWL_STYLEEXT_PTB_VAlignMask 3L << 2
#define FWL_STYLEEXT_PTB_StretchAlignMask 7L << 4

class CFX_DIBitmap;
class IFWL_Widget;

class IFWL_PictureBoxDP : public IFWL_DataProvider {};

class IFWL_PictureBox : public IFWL_Widget {
 public:
  explicit IFWL_PictureBox(const IFWL_App* app,
                           std::unique_ptr<CFWL_WidgetProperties> properties);
  ~IFWL_PictureBox() override;

  // IFWL_Widget
  FWL_Type GetClassID() const override;
  void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false) override;
  void Update() override;
  void DrawWidget(CFX_Graphics* pGraphics,
                  const CFX_Matrix* pMatrix = nullptr) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix) override;

 private:
  CFX_RectF m_rtClient;
  CFX_RectF m_rtImage;
  CFX_Matrix m_matrix;
};

#endif  // XFA_FWL_CORE_IFWL_PICTUREBOX_H_
