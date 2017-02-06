/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
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

#ifndef QQUICKDEFAULTSTYLE_P_H
#define QQUICKDEFAULTSTYLE_P_H

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

#include <QtCore/qobject.h>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QQuickDefaultStyle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor CONSTANT FINAL)
    Q_PROPERTY(QColor overlayModalColor READ overlayModalColor CONSTANT FINAL)
    Q_PROPERTY(QColor overlayDimColor READ overlayDimColor CONSTANT FINAL)
    Q_PROPERTY(QColor textColor READ textColor CONSTANT FINAL)
    Q_PROPERTY(QColor textDarkColor READ textDarkColor CONSTANT FINAL)
    Q_PROPERTY(QColor textLightColor READ textLightColor CONSTANT FINAL)
    Q_PROPERTY(QColor textLinkColor READ textLinkColor CONSTANT FINAL)
    Q_PROPERTY(QColor textSelectionColor READ textSelectionColor CONSTANT FINAL)
    Q_PROPERTY(QColor textDisabledColor READ textDisabledColor CONSTANT FINAL)
    Q_PROPERTY(QColor textDisabledLightColor READ textDisabledLightColor CONSTANT FINAL)
    Q_PROPERTY(QColor focusColor READ focusColor CONSTANT FINAL)
    Q_PROPERTY(QColor focusLightColor READ focusLightColor CONSTANT FINAL)
    Q_PROPERTY(QColor focusPressedColor READ focusPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor buttonColor READ buttonColor CONSTANT FINAL)
    Q_PROPERTY(QColor buttonPressedColor READ buttonPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor buttonCheckedColor READ buttonCheckedColor CONSTANT FINAL)
    Q_PROPERTY(QColor buttonCheckedPressedColor READ buttonCheckedPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor buttonCheckedFocusColor READ buttonCheckedFocusColor CONSTANT FINAL)
    Q_PROPERTY(QColor toolButtonColor READ toolButtonColor CONSTANT FINAL)
    Q_PROPERTY(QColor tabButtonColor READ tabButtonColor CONSTANT FINAL)
    Q_PROPERTY(QColor tabButtonPressedColor READ tabButtonPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor tabButtonCheckedPressedColor READ tabButtonCheckedPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor delegateColor READ delegateColor CONSTANT FINAL)
    Q_PROPERTY(QColor delegatePressedColor READ delegatePressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor delegateFocusColor READ delegateFocusColor CONSTANT FINAL)
    Q_PROPERTY(QColor indicatorPressedColor READ indicatorPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor indicatorDisabledColor READ indicatorDisabledColor CONSTANT FINAL)
    Q_PROPERTY(QColor indicatorFrameColor READ indicatorFrameColor CONSTANT FINAL)
    Q_PROPERTY(QColor indicatorFramePressedColor READ indicatorFramePressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor indicatorFrameDisabledColor READ indicatorFrameDisabledColor CONSTANT FINAL)
    Q_PROPERTY(QColor frameDarkColor READ frameDarkColor CONSTANT FINAL)
    Q_PROPERTY(QColor frameLightColor READ frameLightColor CONSTANT FINAL)
    Q_PROPERTY(QColor scrollBarColor READ scrollBarColor CONSTANT FINAL)
    Q_PROPERTY(QColor scrollBarPressedColor READ scrollBarPressedColor CONSTANT FINAL)
    Q_PROPERTY(QColor progressBarColor READ progressBarColor CONSTANT FINAL)
    Q_PROPERTY(QColor pageIndicatorColor READ pageIndicatorColor CONSTANT FINAL)
    Q_PROPERTY(QColor separatorColor READ separatorColor CONSTANT FINAL)
    Q_PROPERTY(QColor disabledDarkColor READ disabledDarkColor CONSTANT FINAL)
    Q_PROPERTY(QColor disabledLightColor READ disabledLightColor CONSTANT FINAL)

public:
    explicit QQuickDefaultStyle(QObject *parent = nullptr);

    QColor backgroundColor() const;
    QColor overlayModalColor() const;
    QColor overlayDimColor() const;
    QColor textColor() const;
    QColor textDarkColor() const;
    QColor textLightColor() const;
    QColor textLinkColor() const;
    QColor textSelectionColor() const;
    QColor textDisabledColor() const;
    QColor textDisabledLightColor() const;
    QColor focusColor() const;
    QColor focusLightColor() const;
    QColor focusPressedColor() const;
    QColor buttonColor() const;
    QColor buttonPressedColor() const;
    QColor buttonCheckedColor() const;
    QColor buttonCheckedPressedColor() const;
    QColor buttonCheckedFocusColor() const;
    QColor toolButtonColor() const;
    QColor tabButtonColor() const;
    QColor tabButtonPressedColor() const;
    QColor tabButtonCheckedPressedColor() const;
    QColor delegateColor() const;
    QColor delegatePressedColor() const;
    QColor delegateFocusColor() const;
    QColor indicatorPressedColor() const;
    QColor indicatorDisabledColor() const;
    QColor indicatorFrameColor() const;
    QColor indicatorFramePressedColor() const;
    QColor indicatorFrameDisabledColor() const;
    QColor frameDarkColor() const;
    QColor frameLightColor() const;
    QColor scrollBarColor() const;
    QColor scrollBarPressedColor() const;
    QColor progressBarColor() const;
    QColor pageIndicatorColor() const;
    QColor separatorColor() const;
    QColor disabledDarkColor() const;
    QColor disabledLightColor() const;
};

QT_END_NAMESPACE

#endif // QQUICKDEFAULTSTYLE_P_H
