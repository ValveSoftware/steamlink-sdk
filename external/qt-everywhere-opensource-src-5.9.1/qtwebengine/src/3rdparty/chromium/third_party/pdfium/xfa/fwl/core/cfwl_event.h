// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_EVENT_H_
#define XFA_FWL_CORE_CFWL_EVENT_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/cfwl_msgkey.h"
#include "xfa/fwl/core/cfwl_msgmouse.h"
#include "xfa/fwl/core/fwl_error.h"

enum class CFWL_EventType {
  None = 0,

  CheckStateChanged,
  CheckWord,
  Click,
  Close,
  EditChanged,
  Key,
  KillFocus,
  Mouse,
  MouseWheel,
  PostDropDown,
  PreDropDown,
  Scroll,
  SelectChanged,
  SetFocus,
  SizeChanged,
  TextChanged,
  TextFull,
  Validate
};

enum FWLEventMask {
  FWL_EVENT_MOUSE_MASK = 1 << 0,
  FWL_EVENT_MOUSEWHEEL_MASK = 1 << 1,
  FWL_EVENT_KEY_MASK = 1 << 2,
  FWL_EVENT_FOCUSCHANGED_MASK = 1 << 3,
  FWL_EVENT_DRAW_MASK = 1 << 4,
  FWL_EVENT_CLOSE_MASK = 1 << 5,
  FWL_EVENT_SIZECHANGED_MASK = 1 << 6,
  FWL_EVENT_IDLE_MASK = 1 << 7,
  FWL_EVENT_CONTROL_MASK = 1 << 8,
  FWL_EVENT_ALL_MASK = 0xFF
};

class CFX_Graphics;
class IFWL_Widget;

class CFWL_Event {
 public:
  CFWL_Event();
  virtual ~CFWL_Event();

  virtual CFWL_EventType GetClassID() const;

  IFWL_Widget* m_pSrcTarget;
  IFWL_Widget* m_pDstTarget;
};

#define FWL_EVENT_DEF(classname, eventType, ...)                            \
  class classname : public CFWL_Event {                                     \
   public:                                                                  \
    classname();                                                            \
    ~classname() override;                                                  \
    CFWL_EventType GetClassID() const override;                             \
    __VA_ARGS__                                                             \
  };                                                                        \
  inline classname::classname() {}                                          \
  inline classname::~classname() {}                                         \
  inline CFWL_EventType classname::GetClassID() const { return eventType; }

#endif  // XFA_FWL_CORE_CFWL_EVENT_H_
