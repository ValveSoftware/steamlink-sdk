// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_FDE_GEDEVICE_H_
#define XFA_FDE_FDE_GEDEVICE_H_

#include "core/fxge/cfx_renderdevice.h"
#include "xfa/fgas/crt/fgas_memory.h"

class CFDE_Brush;
class CFDE_Path;
class CFDE_Pen;
class CFGAS_GEFont;
class CFX_GraphStateData;

class CFDE_RenderDevice : public CFX_Target {
 public:
  CFDE_RenderDevice(CFX_RenderDevice* pDevice, bool bOwnerDevice);
  ~CFDE_RenderDevice() override;

  int32_t GetWidth() const;
  int32_t GetHeight() const;
  void SaveState();
  void RestoreState();
  bool SetClipPath(const CFDE_Path* pClip);
  CFDE_Path* GetClipPath() const;
  bool SetClipRect(const CFX_RectF& rtClip);
  const CFX_RectF& GetClipRect();

  FX_FLOAT GetDpiX() const;
  FX_FLOAT GetDpiY() const;

  bool DrawImage(CFX_DIBSource* pDib,
                 const CFX_RectF* pSrcRect,
                 const CFX_RectF& dstRect,
                 const CFX_Matrix* pImgMatrix = nullptr,
                 const CFX_Matrix* pDevMatrix = nullptr);
  bool DrawString(CFDE_Brush* pBrush,
                  CFGAS_GEFont* pFont,
                  const FXTEXT_CHARPOS* pCharPos,
                  int32_t iCount,
                  FX_FLOAT fFontSize,
                  const CFX_Matrix* pMatrix = nullptr);
  bool DrawBezier(CFDE_Pen* pPen,
                  FX_FLOAT fPenWidth,
                  const CFX_PointF& pt1,
                  const CFX_PointF& pt2,
                  const CFX_PointF& pt3,
                  const CFX_PointF& pt4,
                  const CFX_Matrix* pMatrix = nullptr);
  bool DrawCurve(CFDE_Pen* pPen,
                 FX_FLOAT fPenWidth,
                 const CFX_PointsF& points,
                 bool bClosed,
                 FX_FLOAT fTension = 0.5f,
                 const CFX_Matrix* pMatrix = nullptr);
  bool DrawEllipse(CFDE_Pen* pPen,
                   FX_FLOAT fPenWidth,
                   const CFX_RectF& rect,
                   const CFX_Matrix* pMatrix = nullptr);
  bool DrawLines(CFDE_Pen* pPen,
                 FX_FLOAT fPenWidth,
                 const CFX_PointsF& points,
                 const CFX_Matrix* pMatrix = nullptr);
  bool DrawLine(CFDE_Pen* pPen,
                FX_FLOAT fPenWidth,
                const CFX_PointF& pt1,
                const CFX_PointF& pt2,
                const CFX_Matrix* pMatrix = nullptr);
  bool DrawPath(CFDE_Pen* pPen,
                FX_FLOAT fPenWidth,
                const CFDE_Path* pPath,
                const CFX_Matrix* pMatrix = nullptr);
  bool DrawPolygon(CFDE_Pen* pPen,
                   FX_FLOAT fPenWidth,
                   const CFX_PointsF& points,
                   const CFX_Matrix* pMatrix = nullptr);
  bool DrawRectangle(CFDE_Pen* pPen,
                     FX_FLOAT fPenWidth,
                     const CFX_RectF& rect,
                     const CFX_Matrix* pMatrix = nullptr);
  bool FillClosedCurve(CFDE_Brush* pBrush,
                       const CFX_PointsF& points,
                       FX_FLOAT fTension = 0.5f,
                       const CFX_Matrix* pMatrix = nullptr);
  bool FillEllipse(CFDE_Brush* pBrush,
                   const CFX_RectF& rect,
                   const CFX_Matrix* pMatrix = nullptr);
  bool FillPath(CFDE_Brush* pBrush,
                const CFDE_Path* pPath,
                const CFX_Matrix* pMatrix = nullptr);
  bool FillPolygon(CFDE_Brush* pBrush,
                   const CFX_PointsF& points,
                   const CFX_Matrix* pMatrix = nullptr);
  bool FillRectangle(CFDE_Brush* pBrush,
                     const CFX_RectF& rect,
                     const CFX_Matrix* pMatrix = nullptr);

  bool DrawSolidString(CFDE_Brush* pBrush,
                       CFGAS_GEFont* pFont,
                       const FXTEXT_CHARPOS* pCharPos,
                       int32_t iCount,
                       FX_FLOAT fFontSize,
                       const CFX_Matrix* pMatrix);
  bool DrawStringPath(CFDE_Brush* pBrush,
                      CFGAS_GEFont* pFont,
                      const FXTEXT_CHARPOS* pCharPos,
                      int32_t iCount,
                      FX_FLOAT fFontSize,
                      const CFX_Matrix* pMatrix);

 protected:
  bool CreatePen(CFDE_Pen* pPen,
                 FX_FLOAT fPenWidth,
                 CFX_GraphStateData& graphState);

  CFX_RenderDevice* const m_pDevice;
  CFX_RectF m_rtClip;
  bool m_bOwnerDevice;
  int32_t m_iCharCount;
};

#endif  // XFA_FDE_FDE_GEDEVICE_H_
