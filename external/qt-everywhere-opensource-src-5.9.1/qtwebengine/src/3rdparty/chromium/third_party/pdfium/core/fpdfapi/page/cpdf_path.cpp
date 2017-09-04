// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/page/cpdf_path.h"

CPDF_Path::CPDF_Path() {}

CPDF_Path::CPDF_Path(const CPDF_Path& that) : m_Ref(that.m_Ref) {}

CPDF_Path::~CPDF_Path() {}

int CPDF_Path::GetPointCount() const {
  return m_Ref.GetObject()->GetPointCount();
}

void CPDF_Path::SetPointCount(int count) {
  m_Ref.GetPrivateCopy()->SetPointCount(count);
}

const FX_PATHPOINT* CPDF_Path::GetPoints() const {
  return m_Ref.GetObject()->GetPoints();
}

FX_PATHPOINT* CPDF_Path::GetMutablePoints() {
  return m_Ref.GetPrivateCopy()->GetPoints();
}

int CPDF_Path::GetFlag(int index) const {
  return m_Ref.GetObject()->GetFlag(index);
}

FX_FLOAT CPDF_Path::GetPointX(int index) const {
  return m_Ref.GetObject()->GetPointX(index);
}

FX_FLOAT CPDF_Path::GetPointY(int index) const {
  return m_Ref.GetObject()->GetPointY(index);
}

CFX_FloatRect CPDF_Path::GetBoundingBox() const {
  return m_Ref.GetObject()->GetBoundingBox();
}

CFX_FloatRect CPDF_Path::GetBoundingBox(FX_FLOAT line_width,
                                        FX_FLOAT miter_limit) const {
  return m_Ref.GetObject()->GetBoundingBox(line_width, miter_limit);
}

bool CPDF_Path::IsRect() const {
  return m_Ref.GetObject()->IsRect();
}

void CPDF_Path::Transform(const CFX_Matrix* pMatrix) {
  m_Ref.GetPrivateCopy()->Transform(pMatrix);
}

void CPDF_Path::Append(const CPDF_Path& other, const CFX_Matrix* pMatrix) {
  m_Ref.GetPrivateCopy()->Append(other.GetObject(), pMatrix);
}

void CPDF_Path::Append(const CFX_PathData* pData, const CFX_Matrix* pMatrix) {
  m_Ref.GetPrivateCopy()->Append(pData, pMatrix);
}

void CPDF_Path::AppendRect(FX_FLOAT left,
                           FX_FLOAT bottom,
                           FX_FLOAT right,
                           FX_FLOAT top) {
  m_Ref.GetPrivateCopy()->AppendRect(left, bottom, right, top);
}
