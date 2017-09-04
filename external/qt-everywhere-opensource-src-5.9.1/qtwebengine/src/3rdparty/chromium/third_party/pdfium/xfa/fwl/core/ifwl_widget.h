// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_IFWL_WIDGET_H_
#define XFA_FWL_CORE_IFWL_WIDGET_H_

#include <memory>

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/cfwl_event.h"
#include "xfa/fwl/core/cfwl_themepart.h"
#include "xfa/fwl/core/cfwl_widgetmgr.h"
#include "xfa/fwl/core/cfwl_widgetproperties.h"
#include "xfa/fwl/core/fwl_widgethit.h"
#include "xfa/fwl/core/ifwl_widgetdelegate.h"
#include "xfa/fwl/theme/cfwl_widgettp.h"

// FWL contains two parallel inheritance hierarchies, which reference each
// other via pointers as follows:
//
//                  m_pAssociate
//                  <----------
//      CFWL_Widget ----------> IFWL_Widget
//           |       m_pIface        |
//           A                       A
//           |                       |
//      CFWL_...                IFWL_...
//
// TODO(tsepez): Collapse these into a single hierarchy.
//

enum class FWL_Type {
  Unknown = 0,

  Barcode,
  Caret,
  CheckBox,
  ComboBox,
  DateTimePicker,
  Edit,
  Form,
  FormProxy,
  ListBox,
  MonthCalendar,
  PictureBox,
  PushButton,
  ScrollBar,
  SpinButton,
  ToolTip
};

class CFWL_AppImp;
class CFWL_MsgKey;
class CFWL_Widget;
class CFWL_WidgetProperties;
class CFWL_WidgetMgr;
class IFWL_App;
class IFWL_DataProvider;
class IFWL_ThemeProvider;
class IFWL_Widget;
enum class FWL_Type;

class IFWL_Widget : public IFWL_WidgetDelegate {
 public:
  ~IFWL_Widget() override;

  virtual FWL_Type GetClassID() const = 0;
  virtual bool IsInstance(const CFX_WideStringC& wsClass) const;
  virtual void GetWidgetRect(CFX_RectF& rect, bool bAutoSize = false);
  virtual void GetClientRect(CFX_RectF& rect);
  virtual void ModifyStylesEx(uint32_t dwStylesExAdded,
                              uint32_t dwStylesExRemoved);
  virtual void SetStates(uint32_t dwStates, bool bSet = true);
  virtual void Update();
  virtual FWL_WidgetHit HitTest(FX_FLOAT fx, FX_FLOAT fy);
  virtual void DrawWidget(CFX_Graphics* pGraphics,
                          const CFX_Matrix* pMatrix = nullptr);
  virtual void SetThemeProvider(IFWL_ThemeProvider* pThemeProvider);

  // IFWL_WidgetDelegate.
  void OnProcessMessage(CFWL_Message* pMessage) override;
  void OnProcessEvent(CFWL_Event* pEvent) override;
  void OnDrawWidget(CFX_Graphics* pGraphics,
                    const CFX_Matrix* pMatrix = nullptr) override;

  void SetWidgetRect(const CFX_RectF& rect);

  void SetParent(IFWL_Widget* pParent);

  IFWL_Widget* GetOwner() { return m_pWidgetMgr->GetOwnerWidget(this); }
  IFWL_Widget* GetOuter() const { return m_pOuter; }

  uint32_t GetStyles() const { return m_pProperties->m_dwStyles; }
  void ModifyStyles(uint32_t dwStylesAdded, uint32_t dwStylesRemoved);
  uint32_t GetStylesEx() const { return m_pProperties->m_dwStyleExes; }
  uint32_t GetStates() const { return m_pProperties->m_dwStates; }

  void LockUpdate() { m_iLock++; }
  void UnlockUpdate() {
    if (IsLocked())
      m_iLock--;
  }

  void TransformTo(IFWL_Widget* pWidget, FX_FLOAT& fx, FX_FLOAT& fy);
  void GetMatrix(CFX_Matrix& matrix, bool bGlobal = false);
  IFWL_ThemeProvider* GetThemeProvider() const {
    return m_pProperties->m_pThemeProvider;
  }

  IFWL_DataProvider* GetDataProvider() const {
    return m_pProperties->m_pDataProvider;
  }

  void SetDelegate(IFWL_WidgetDelegate* delegate) { m_pDelegate = delegate; }
  IFWL_WidgetDelegate* GetDelegate() {
    return m_pDelegate ? m_pDelegate : this;
  }
  const IFWL_WidgetDelegate* GetDelegate() const {
    return m_pDelegate ? m_pDelegate : this;
  }

  const IFWL_App* GetOwnerApp() const { return m_pOwnerApp; }
  uint32_t GetEventKey() const { return m_nEventKey; }
  void SetEventKey(uint32_t key) { m_nEventKey = key; }

  void* GetLayoutItem() const { return m_pLayoutItem; }
  void SetLayoutItem(void* pItem) { m_pLayoutItem = pItem; }

  void SetAssociateWidget(CFWL_Widget* pAssociate) {
    m_pAssociate = pAssociate;
  }

  void SetFocus(bool bFocus);
  void Repaint(const CFX_RectF* pRect = nullptr);

 protected:
  IFWL_Widget(const IFWL_App* app,
              std::unique_ptr<CFWL_WidgetProperties> properties,
              IFWL_Widget* pOuter);

  bool IsEnabled() const {
    return (m_pProperties->m_dwStates & FWL_WGTSTATE_Disabled) == 0;
  }
  bool IsActive() const {
    return (m_pProperties->m_dwStates & FWL_WGTSTATE_Deactivated) == 0;
  }
  bool IsLocked() const { return m_iLock > 0; }
  bool HasBorder() const {
    return !!(m_pProperties->m_dwStyles & FWL_WGTSTYLE_Border);
  }
  bool HasEdge() const {
    return !!(m_pProperties->m_dwStyles & FWL_WGTSTYLE_EdgeMask);
  }
  void GetEdgeRect(CFX_RectF& rtEdge);
  FX_FLOAT GetBorderSize(bool bCX = true);
  FX_FLOAT GetEdgeWidth();
  void GetRelativeRect(CFX_RectF& rect);
  void* GetThemeCapacity(CFWL_WidgetCapacity dwCapacity);
  IFWL_ThemeProvider* GetAvailableTheme();
  CFX_SizeF CalcTextSize(const CFX_WideString& wsText,
                         IFWL_ThemeProvider* pTheme,
                         bool bMultiLine = false,
                         int32_t iLineWidth = -1);
  void CalcTextRect(const CFX_WideString& wsText,
                    IFWL_ThemeProvider* pTheme,
                    uint32_t dwTTOStyles,
                    int32_t iTTOAlign,
                    CFX_RectF& rect);
  void SetGrab(bool bSet);
  void GetPopupPos(FX_FLOAT fMinHeight,
                   FX_FLOAT fMaxHeight,
                   const CFX_RectF& rtAnchor,
                   CFX_RectF& rtPopup);
  void RegisterEventTarget(IFWL_Widget* pEventSource = nullptr,
                           uint32_t dwFilter = FWL_EVENT_ALL_MASK);
  void UnregisterEventTarget();
  void DispatchKeyEvent(CFWL_MsgKey* pNote);
  void DispatchEvent(CFWL_Event* pEvent);
  void DrawBorder(CFX_Graphics* pGraphics,
                  CFWL_Part iPartBorder,
                  IFWL_ThemeProvider* pTheme,
                  const CFX_Matrix* pMatrix = nullptr);
  void DrawEdge(CFX_Graphics* pGraphics,
                CFWL_Part iPartEdge,
                IFWL_ThemeProvider* pTheme,
                const CFX_Matrix* pMatrix = nullptr);

  const IFWL_App* const m_pOwnerApp;
  CFWL_WidgetMgr* const m_pWidgetMgr;
  std::unique_ptr<CFWL_WidgetProperties> m_pProperties;
  IFWL_Widget* m_pOuter;
  int32_t m_iLock;

 private:
  IFWL_Widget* GetParent() { return m_pWidgetMgr->GetParentWidget(this); }
  CFX_SizeF GetOffsetFromParent(IFWL_Widget* pParent);

  bool IsVisible() const {
    return (m_pProperties->m_dwStates & FWL_WGTSTATE_Invisible) == 0;
  }
  bool IsOverLapper() const {
    return (m_pProperties->m_dwStyles & FWL_WGTSTYLE_WindowTypeMask) ==
           FWL_WGTSTYLE_OverLapper;
  }
  bool IsPopup() const {
    return !!(m_pProperties->m_dwStyles & FWL_WGTSTYLE_Popup);
  }
  bool IsChild() const {
    return !!(m_pProperties->m_dwStyles & FWL_WGTSTYLE_Child);
  }
  bool IsOffscreen() const {
    return !!(m_pProperties->m_dwStyles & FWL_WGTSTYLE_Offscreen);
  }
  IFWL_Widget* GetRootOuter();
  bool GetPopupPosMenu(FX_FLOAT fMinHeight,
                       FX_FLOAT fMaxHeight,
                       const CFX_RectF& rtAnchor,
                       CFX_RectF& rtPopup);
  bool GetPopupPosComboBox(FX_FLOAT fMinHeight,
                           FX_FLOAT fMaxHeight,
                           const CFX_RectF& rtAnchor,
                           CFX_RectF& rtPopup);
  bool GetPopupPosGeneral(FX_FLOAT fMinHeight,
                          FX_FLOAT fMaxHeight,
                          const CFX_RectF& rtAnchor,
                          CFX_RectF& rtPopup);
  bool GetScreenSize(FX_FLOAT& fx, FX_FLOAT& fy);
  void DrawBackground(CFX_Graphics* pGraphics,
                      CFWL_Part iPartBk,
                      IFWL_ThemeProvider* pTheme,
                      const CFX_Matrix* pMatrix = nullptr);
  void NotifyDriver();
  bool IsParent(IFWL_Widget* pParent);

  void* m_pLayoutItem;
  CFWL_Widget* m_pAssociate;
  uint32_t m_nEventKey;
  IFWL_WidgetDelegate* m_pDelegate;  // Not owned.
};

#endif  // XFA_FWL_CORE_IFWL_WIDGET_H_
