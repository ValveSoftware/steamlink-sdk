/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquicktooltip_p.h"
#include <qquickwindow.h>
#include <qquickitem.h>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <QtQuick/QQuickRenderControl>

#ifdef QT_WIDGETS_LIB
#include <qtooltip.h>
#endif

QT_BEGIN_NAMESPACE

QQuickTooltip1::QQuickTooltip1(QObject *parent)
    : QObject(parent)
{

}

void QQuickTooltip1::showText(QQuickItem *item, const QPointF &pos, const QString &str)
{
    if (!item || !item->window())
        return;
#ifdef QT_WIDGETS_LIB
    if (QGuiApplicationPrivate::platformIntegration()->
            hasCapability(QPlatformIntegration::MultipleWindows) &&
        QCoreApplication::instance()->inherits("QApplication")) {
        QPoint quickWidgetOffsetInTlw;
        QWindow *renderWindow = QQuickRenderControl::renderWindowFor(item->window(), &quickWidgetOffsetInTlw);
        QWindow *window = renderWindow ? renderWindow : item->window();
        const QPoint offsetInQuickWidget = item->mapToScene(pos).toPoint();
        const QPoint mappedPos = window->mapToGlobal(offsetInQuickWidget + quickWidgetOffsetInTlw);
        QToolTip::showText(mappedPos, str);
    }
#else
    Q_UNUSED(item);
    Q_UNUSED(pos);
    Q_UNUSED(str);
#endif
}

void QQuickTooltip1::hideText()
{
#ifdef QT_WIDGETS_LIB
    QToolTip::hideText();
#endif
}

QT_END_NAMESPACE
