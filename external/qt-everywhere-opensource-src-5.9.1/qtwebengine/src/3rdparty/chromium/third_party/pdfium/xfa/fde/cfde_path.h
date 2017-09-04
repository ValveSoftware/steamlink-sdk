// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_CFDE_PATH_H_
#define XFA_FDE_CFDE_PATH_H_

#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "xfa/fgas/crt/fgas_memory.h"

class CFDE_Path : public CFX_Target {
 public:
  bool StartFigure();
  bool CloseFigure();

  void AddBezier(const CFX_PointsF& points);
  void AddBeziers(const CFX_PointsF& points);
  void AddCurve(const CFX_PointsF& points,
                bool bClosed,
                FX_FLOAT fTension = 0.5f);
  void AddEllipse(const CFX_RectF& rect);
  void AddLines(const CFX_PointsF& points);
  void AddLine(const CFX_PointF& pt1, const CFX_PointF& pt2);
  void AddPath(const CFDE_Path* pSrc, bool bConnect);
  void AddPolygon(const CFX_PointsF& points);
  void AddRectangle(const CFX_RectF& rect);
  void GetBBox(CFX_RectF& bbox) const;
  void GetBBox(CFX_RectF& bbox,
               FX_FLOAT fLineWidth,
               FX_FLOAT fMiterLimit) const;
  FX_PATHPOINT* AddPoints(int32_t iCount);
  FX_PATHPOINT* GetLastPoint(int32_t iCount = 1) const;
  bool FigureClosed() const;
  void MoveTo(FX_FLOAT fx, FX_FLOAT fy);
  void LineTo(FX_FLOAT fx, FX_FLOAT fy);
  void BezierTo(const CFX_PointF& p1,
                const CFX_PointF& p2,
                const CFX_PointF& p3);
  void ArcTo(bool bStart,
             const CFX_RectF& rect,
             FX_FLOAT startAngle,
             FX_FLOAT endAngle);
  void MoveTo(const CFX_PointF& p0) { MoveTo(p0.x, p0.y); }
  void LineTo(const CFX_PointF& p1) { LineTo(p1.x, p1.y); }
  void GetCurveTangents(const CFX_PointsF& points,
                        CFX_PointsF& tangents,
                        bool bClosed,
                        FX_FLOAT fTension) const;
  CFX_PathData m_Path;
};

#endif  // XFA_FDE_CFDE_PATH_H_
