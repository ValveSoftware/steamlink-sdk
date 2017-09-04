// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFAPI_PAGE_CPDF_PATH_H_
#define CORE_FPDFAPI_PAGE_CPDF_PATH_H_

#include "core/fxcrt/cfx_shared_copy_on_write.h"
#include "core/fxcrt/fx_system.h"
#include "core/fxge/cfx_fxgedevice.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"

class CPDF_Path {
 public:
  CPDF_Path();
  CPDF_Path(const CPDF_Path& that);
  ~CPDF_Path();

  void Emplace() { m_Ref.Emplace(); }
  explicit operator bool() const { return !!m_Ref; }

  int GetPointCount() const;
  void SetPointCount(int count);
  const FX_PATHPOINT* GetPoints() const;
  FX_PATHPOINT* GetMutablePoints();

  int GetFlag(int index) const;
  FX_FLOAT GetPointX(int index) const;
  FX_FLOAT GetPointY(int index) const;
  CFX_FloatRect GetBoundingBox() const;
  CFX_FloatRect GetBoundingBox(FX_FLOAT line_width, FX_FLOAT miter_limit) const;

  bool IsRect() const;
  void Transform(const CFX_Matrix* pMatrix);

  void Append(const CPDF_Path& other, const CFX_Matrix* pMatrix);
  void Append(const CFX_PathData* pData, const CFX_Matrix* pMatrix);
  void AppendRect(FX_FLOAT left, FX_FLOAT bottom, FX_FLOAT right, FX_FLOAT top);

  // TODO(tsepez): Remove when all access thru this class.
  const CFX_PathData* GetObject() const { return m_Ref.GetObject(); }

 private:
  CFX_SharedCopyOnWrite<CFX_PathData> m_Ref;
};

#endif  // CORE_FPDFAPI_PAGE_CPDF_PATH_H_
