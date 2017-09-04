// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FDE_FDE_RENDER_H_
#define XFA_FDE_FDE_RENDER_H_

#include <memory>
#include <vector>

#include "core/fxcrt/fx_coordinates.h"
#include "xfa/fde/fde_gedevice.h"
#include "xfa/fde/fde_iterator.h"
#include "xfa/fde/fde_visualset.h"

class CFDE_RenderDevice;

enum FDE_RENDERSTATUS {
  FDE_RENDERSTATUS_Reset = 0,
  FDE_RENDERSTATUS_Paused,
  FDE_RENDERSTATUS_Done,
  FDE_RENDERSTATUS_Failed,
};

class CFDE_RenderContext : public CFX_Target {
 public:
  CFDE_RenderContext();
  ~CFDE_RenderContext() override;

  bool StartRender(CFDE_RenderDevice* pRenderDevice,
                   IFDE_CanvasSet* pCanvasSet,
                   const CFX_Matrix& tmDoc2Device);
  FDE_RENDERSTATUS GetStatus() const { return m_eStatus; }
  FDE_RENDERSTATUS DoRender(IFX_Pause* pPause = nullptr);
  void StopRender();
  void RenderText(IFDE_TextSet* pTextSet, FDE_TEXTEDITPIECE* pText);

 protected:
  FDE_RENDERSTATUS m_eStatus;
  CFDE_RenderDevice* m_pRenderDevice;
  CFX_Matrix m_Transform;
  std::vector<FXTEXT_CHARPOS> m_CharPos;
  std::unique_ptr<CFDE_Brush> m_pBrush;
  std::unique_ptr<CFDE_VisualSetIterator> m_pIterator;
};

#endif  // XFA_FDE_FDE_RENDER_H_
