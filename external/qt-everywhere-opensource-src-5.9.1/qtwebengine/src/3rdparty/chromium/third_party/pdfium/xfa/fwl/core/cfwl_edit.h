// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EDIT_H_
#define XFA_FWL_CORE_CFWL_EDIT_H_

#include <vector>

#include "xfa/fwl/core/cfwl_widget.h"
#include "xfa/fwl/core/ifwl_edit.h"

class IFDE_TxtEdtDoRecord;

class CFWL_Edit : public CFWL_Widget {
 public:
  explicit CFWL_Edit(const IFWL_App*);
  ~CFWL_Edit() override;

  void Initialize();

  void SetText(const CFX_WideString& wsText);
  void GetText(CFX_WideString& wsText,
               int32_t nStart = 0,
               int32_t nCount = -1) const;

  int32_t CountSelRanges() const;
  int32_t GetSelRange(int32_t nIndex, int32_t& nStart) const;

  int32_t GetLimit() const;
  void SetLimit(int32_t nLimit);

  void SetAliasChar(FX_WCHAR wAlias);

  void SetScrollOffset(FX_FLOAT fScrollOffset);
};

#endif  // XFA_FWL_CORE_CFWL_EDIT_H_
