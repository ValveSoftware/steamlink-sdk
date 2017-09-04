/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "browser_accessibility_manager_qt.h"

#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "browser_accessibility_qt.h"

using namespace blink;

namespace content {

BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
      const ui::AXTreeUpdate& initialTree,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory)
{
#ifndef QT_NO_ACCESSIBILITY
        return new BrowserAccessibilityManagerQt(0, initialTree, delegate);
#else
        return 0;
#endif // QT_NO_ACCESSIBILITY
}

BrowserAccessibility *BrowserAccessibilityFactoryQt::Create()
{
#ifndef QT_NO_ACCESSIBILITY
    return new BrowserAccessibilityQt();
#else
    return 0;
#endif // QT_NO_ACCESSIBILITY
}

#ifndef QT_NO_ACCESSIBILITY
BrowserAccessibilityManagerQt::BrowserAccessibilityManagerQt(
    QObject *parentObject, const ui::AXTreeUpdate &initialTree,
    BrowserAccessibilityDelegate* delegate, BrowserAccessibilityFactory* factory)
      : BrowserAccessibilityManager(delegate, factory)
      , m_parentObject(parentObject)
{
    Initialize(initialTree);
}

QAccessibleInterface *BrowserAccessibilityManagerQt::rootParentAccessible()
{
    return QAccessible::queryAccessibleInterface(m_parentObject);
}

void BrowserAccessibilityManagerQt::NotifyAccessibilityEvent(BrowserAccessibilityEvent::Source source,
                                                             ui::AXEvent event_type,
                                                             BrowserAccessibility* node)
{
    BrowserAccessibilityQt *iface = static_cast<BrowserAccessibilityQt*>(node);

    switch (event_type) {
    case ui::AX_EVENT_FOCUS: {
        QAccessibleEvent event(iface, QAccessible::Focus);
        QAccessible::updateAccessibility(&event);
        break;
    }
    case ui::AX_EVENT_CHECKED_STATE_CHANGED: {
        QAccessible::State change;
        change.checked = true;
        QAccessibleStateChangeEvent event(iface, change);
        QAccessible::updateAccessibility(&event);
        break;
    }
    case ui::AX_EVENT_VALUE_CHANGED: {
        QVariant value;
        if (QAccessibleValueInterface *valueIface = iface->valueInterface())
            value = valueIface->currentValue();
        QAccessibleValueChangeEvent event(iface, value);
        QAccessible::updateAccessibility(&event);
        break;
    }
    case ui::AX_EVENT_CHILDREN_CHANGED:
        break;
    case ui::AX_EVENT_LAYOUT_COMPLETE:
        break;
    case ui::AX_EVENT_LOAD_COMPLETE:
        break;
    case ui::AX_EVENT_TEXT_CHANGED: {
        QAccessibleTextUpdateEvent event(iface, -1, QString(), QString());
        QAccessible::updateAccessibility(&event);
        break;
    }
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED: {
        QAccessibleTextInterface *textIface = iface->textInterface();
        if (textIface) {
            int start = 0;
            int end = 0;
            textIface->selection(0, &start, &end);
            if (start == end) {
                QAccessibleTextCursorEvent event(iface, start);
                QAccessible::updateAccessibility(&event);
            } else {
                QAccessibleTextSelectionEvent event(iface, start, end);
                QAccessible::updateAccessibility(&event);
            }
        }
        break;
    }
    default:
        break;
    }
}
#endif // QT_NO_ACCESSIBILITY

}
