/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef BROWSER_ACCESSIBILITY_MANAGER_QT_H
#define BROWSER_ACCESSIBILITY_MANAGER_QT_H

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE
class QAccessibleInterface;
QT_END_NAMESPACE

namespace content {

class BrowserAccessibilityFactoryQt : public BrowserAccessibilityFactory
{
public:
    BrowserAccessibility* Create() Q_DECL_OVERRIDE;
};

class BrowserAccessibilityManagerQt : public BrowserAccessibilityManager
{
public:
    BrowserAccessibilityManagerQt(
        QObject* parentObject,
        const ui::AXTreeUpdate& initialTree,
        BrowserAccessibilityDelegate* delegate,
        BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactoryQt());

    void NotifyAccessibilityEvent(
        ui::AXEvent event_type,
        BrowserAccessibility* node) Q_DECL_OVERRIDE;

    QAccessibleInterface *rootParentAccessible();

private:
    Q_DISABLE_COPY(BrowserAccessibilityManagerQt)
    QObject *m_parentObject;
};

}

#endif
