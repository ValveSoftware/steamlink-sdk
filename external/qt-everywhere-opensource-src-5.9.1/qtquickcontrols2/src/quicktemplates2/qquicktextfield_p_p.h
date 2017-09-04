/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKTEXTFIELD_P_P_H
#define QQUICKTEXTFIELD_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuick/private/qquicktextinput_p_p.h>
#include <QtQuickTemplates2/private/qquickpresshandler_p_p.h>

#include "qquicktextfield_p.h"

#if QT_CONFIG(accessibility)
#include <QtGui/qaccessible.h>
#endif

QT_BEGIN_NAMESPACE

class QQuickAccessibleAttached;

class QQuickTextFieldPrivate : public QQuickTextInputPrivate
#if QT_CONFIG(accessibility)
    , public QAccessible::ActivationObserver
#endif
{
    Q_DECLARE_PUBLIC(QQuickTextField)

public:
    QQuickTextFieldPrivate();
    ~QQuickTextFieldPrivate();

    static QQuickTextFieldPrivate *get(QQuickTextField *item) {
        return static_cast<QQuickTextFieldPrivate *>(QObjectPrivate::get(item)); }

    void resizeBackground();
    void resolveFont();
    void inheritFont(const QFont &f);

#if QT_CONFIG(quicktemplates2_hover)
    void updateHoverEnabled(bool h, bool e);
#endif

    qreal getImplicitWidth() const override;
    qreal getImplicitHeight() const override;

    void implicitWidthChanged() override;
    void implicitHeightChanged() override;

    void readOnlyChanged(bool isReadOnly);
    void echoModeChanged(QQuickTextField::EchoMode echoMode);

#if QT_CONFIG(accessibility)
    void accessibilityActiveChanged(bool active) override;
    QAccessible::Role accessibleRole() const override;
#endif

#if QT_CONFIG(quicktemplates2_hover)
    bool hovered;
    bool explicitHoverEnabled;
#endif
    QFont font;
    QQuickItem *background;
    QString placeholder;
    Qt::FocusReason focusReason;
    QQuickPressHandler pressHandler;
    QQuickAccessibleAttached *accessibleAttached;
};

QT_END_NAMESPACE

#endif // QQUICKTEXTFIELD_P_P_H
