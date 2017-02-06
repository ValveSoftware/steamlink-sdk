/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qlayoutwidget_propertysheet.h"
#include "qlayout_widget_p.h"
#include "formwindow.h"
#include "formeditor.h"

#include <QtDesigner/QExtensionManager>

#include <QtWidgets/QLayout>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

QLayoutWidgetPropertySheet::QLayoutWidgetPropertySheet(QLayoutWidget *object, QObject *parent)
    : QDesignerPropertySheet(object, parent)
{
    clearFakeProperties();
}

QLayoutWidgetPropertySheet::~QLayoutWidgetPropertySheet()
{
}

bool QLayoutWidgetPropertySheet::isVisible(int index) const
{
    static const QString layoutPropertyGroup = QStringLiteral("Layout");
    if (propertyGroup(index) == layoutPropertyGroup)
        return QDesignerPropertySheet::isVisible(index);
    return false;
}

void QLayoutWidgetPropertySheet::setProperty(int index, const QVariant &value)
{
    QDesignerPropertySheet::setProperty(index, value);
}

bool QLayoutWidgetPropertySheet::dynamicPropertiesAllowed() const
{
    return false;
}

QT_END_NAMESPACE
