// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CORE_CFWL_WIDGETPROPERTIES_H_
#define XFA_FWL_CORE_CFWL_WIDGETPROPERTIES_H_

#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_system.h"
#include "xfa/fwl/core/fwl_widgetdef.h"

class IFWL_DataProvider;
class IFWL_ThemeProvider;
class IFWL_Widget;

class CFWL_WidgetProperties {
 public:
  CFWL_WidgetProperties();
  CFWL_WidgetProperties(IFWL_DataProvider* dataProvider);
  ~CFWL_WidgetProperties();

  CFX_RectF m_rtWidget;
  uint32_t m_dwStyles;
  uint32_t m_dwStyleExes;
  uint32_t m_dwStates;
  IFWL_ThemeProvider* m_pThemeProvider;
  IFWL_DataProvider* m_pDataProvider;
  IFWL_Widget* m_pParent;
  IFWL_Widget* m_pOwner;
};

inline CFWL_WidgetProperties::CFWL_WidgetProperties()
    : CFWL_WidgetProperties(nullptr) {}

inline CFWL_WidgetProperties::CFWL_WidgetProperties(
    IFWL_DataProvider* dataProvider)
    : m_dwStyles(FWL_WGTSTYLE_Child),
      m_dwStyleExes(0),
      m_dwStates(0),
      m_pThemeProvider(nullptr),
      m_pDataProvider(dataProvider),
      m_pParent(nullptr),
      m_pOwner(nullptr) {
  m_rtWidget.Set(0, 0, 0, 0);
}

inline CFWL_WidgetProperties::~CFWL_WidgetProperties() {}

#endif  // XFA_FWL_CORE_CFWL_WIDGETPROPERTIES_H_
