// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_PICTUREBOX_H_
#define XFA_FWL_CORE_CFWL_PICTUREBOX_H_

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_picturebox.h"

class CFWL_PictureBox : public CFWL_Widget, public IFWL_PictureBoxDP {
 public:
  explicit CFWL_PictureBox(const IFWL_App* pApp);
  ~CFWL_PictureBox() override;

  void Initialize();

  // IFWL_DataProvider
  void GetCaption(IFWL_Widget* pWidget, CFX_WideString& wsCaption) override;
};

#endif  // XFA_FWL_CORE_CFWL_PICTUREBOX_H_
