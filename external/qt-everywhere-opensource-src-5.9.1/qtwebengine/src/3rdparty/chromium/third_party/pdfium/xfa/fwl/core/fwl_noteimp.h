// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_FWL_NOTEIMP_H_
#define XFA_FWL_CORE_FWL_NOTEIMP_H_

#include <deque>
#include <memory>
#include <unordered_map>

#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/ifwl_tooltip.h"
#include "xfa/fwl/core/ifwl_widget.h"
#include "xfa/fxgraphics/cfx_graphics.h"

class CFWL_EventTarget;
class CFWL_TargetImp;
class IFWL_ToolTip;
class IFWL_Widget;

class CFWL_NoteLoop {
 public:
  CFWL_NoteLoop();
  ~CFWL_NoteLoop() {}

  IFWL_Widget* GetForm() const { return m_pForm; }
  bool ContinueModal() const { return m_bContinueModal; }
  void EndModalLoop() { m_bContinueModal = false; }
  void SetMainForm(IFWL_Widget* pForm) { m_pForm = pForm; }

 private:
  IFWL_Widget* m_pForm;
  bool m_bContinueModal;
};

class CFWL_NoteDriver {
 public:
  CFWL_NoteDriver();
  ~CFWL_NoteDriver();

  void SendEvent(CFWL_Event* pNote);

  void RegisterEventTarget(IFWL_Widget* pListener,
                           IFWL_Widget* pEventSource = nullptr,
                           uint32_t dwFilter = FWL_EVENT_ALL_MASK);
  void UnregisterEventTarget(IFWL_Widget* pListener);
  void ClearEventTargets(bool bRemoveAll);

  CFWL_NoteLoop* GetTopLoop() const;
  void PushNoteLoop(CFWL_NoteLoop* pNoteLoop);
  CFWL_NoteLoop* PopNoteLoop();

  IFWL_Widget* GetFocus() const { return m_pFocus; }
  bool SetFocus(IFWL_Widget* pFocus, bool bNotify = false);
  void SetGrab(IFWL_Widget* pGrab, bool bSet) {
    m_pGrab = bSet ? pGrab : nullptr;
  }

  void Run();

  void NotifyTargetHide(IFWL_Widget* pNoteTarget);
  void NotifyTargetDestroy(IFWL_Widget* pNoteTarget);

  void RegisterForm(IFWL_Widget* pForm);
  void UnRegisterForm(IFWL_Widget* pForm);

  void QueueMessage(std::unique_ptr<CFWL_Message> pMessage);
  void UnqueueMessage(CFWL_NoteLoop* pNoteLoop);
  void ProcessMessage(CFWL_Message* pMessage);

 private:
  bool DispatchMessage(CFWL_Message* pMessage, IFWL_Widget* pMessageForm);
  bool DoSetFocus(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  bool DoKillFocus(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  bool DoKey(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  bool DoMouse(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  bool DoWheel(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  bool DoMouseEx(CFWL_Message* pMsg, IFWL_Widget* pMessageForm);
  void MouseSecondary(CFWL_Message* pMsg);
  bool IsValidMessage(CFWL_Message* pMessage);
  IFWL_Widget* GetMessageForm(IFWL_Widget* pDstTarget);

  CFX_ArrayTemplate<IFWL_Widget*> m_forms;
  std::deque<std::unique_ptr<CFWL_Message>> m_noteQueue;
  CFX_ArrayTemplate<CFWL_NoteLoop*> m_noteLoopQueue;
  std::unordered_map<uint32_t, CFWL_EventTarget*> m_eventTargets;
  IFWL_Widget* m_pHover;
  IFWL_Widget* m_pFocus;
  IFWL_Widget* m_pGrab;
  std::unique_ptr<CFWL_NoteLoop> m_pNoteLoop;
};

class CFWL_EventTarget {
 public:
  CFWL_EventTarget(IFWL_Widget* pListener);
  ~CFWL_EventTarget();

  int32_t SetEventSource(IFWL_Widget* pSource,
                         uint32_t dwFilter = FWL_EVENT_ALL_MASK);
  bool ProcessEvent(CFWL_Event* pEvent);

  bool IsInvalid() const { return m_bInvalid; }
  void FlagInvalid() { m_bInvalid = true; }

 private:
  bool IsFilterEvent(CFWL_Event* pEvent, uint32_t dwFilter) const;

  CFX_MapPtrTemplate<void*, uint32_t> m_eventSources;
  IFWL_Widget* m_pListener;
  bool m_bInvalid;
};

#endif  // XFA_FWL_CORE_FWL_NOTEIMP_H_
