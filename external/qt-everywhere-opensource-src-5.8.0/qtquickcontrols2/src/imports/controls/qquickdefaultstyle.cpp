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

#include "qquickdefaultstyle_p.h"

QT_BEGIN_NAMESPACE

QQuickDefaultStyle::QQuickDefaultStyle(QObject *parent) :
    QObject(parent)
{
}

QColor QQuickDefaultStyle::backgroundColor() const
{
    return QColor::fromRgba(0xFFFFFFFF);
}

QColor QQuickDefaultStyle::overlayModalColor() const
{
    return QColor::fromRgba(0x7F28282A);
}

QColor QQuickDefaultStyle::overlayDimColor() const
{
    return QColor::fromRgba(0x1F28282A);
}

QColor QQuickDefaultStyle::textColor() const
{
    return QColor::fromRgba(0xFF353637);
}

QColor QQuickDefaultStyle::textDarkColor() const
{
    return QColor::fromRgba(0xFF26282A);
}

QColor QQuickDefaultStyle::textLightColor() const
{
    return QColor::fromRgba(0xFFFFFFFF);
}

QColor QQuickDefaultStyle::textLinkColor() const
{
    return QColor::fromRgba(0xFF45A7D7);
}

QColor QQuickDefaultStyle::textSelectionColor() const
{
    return QColor::fromRgba(0xFFFDDD5C);
}

QColor QQuickDefaultStyle::textDisabledColor() const
{
    return QColor::fromRgba(0xFFBDBEBF);
}

QColor QQuickDefaultStyle::textDisabledLightColor() const
{
    return QColor::fromRgba(0xFFC2C2C2);
}

QColor QQuickDefaultStyle::focusColor() const
{
    return QColor::fromRgba(0xFF0066FF);
}

QColor QQuickDefaultStyle::focusLightColor() const
{
    return QColor::fromRgba(0xFFF0F6FF);
}

QColor QQuickDefaultStyle::focusPressedColor() const
{
    return QColor::fromRgba(0xFFCCE0FF);
}

QColor QQuickDefaultStyle::buttonColor() const
{
    return QColor::fromRgba(0xFFE0E0E0);
}

QColor QQuickDefaultStyle::buttonPressedColor() const
{
    return QColor::fromRgba(0xFFD0D0D0);
}

QColor QQuickDefaultStyle::buttonCheckedColor() const
{
    return QColor::fromRgba(0xFF353637);
}

QColor QQuickDefaultStyle::buttonCheckedPressedColor() const
{
    return QColor::fromRgba(0xFF585A5C);
}

QColor QQuickDefaultStyle::buttonCheckedFocusColor() const
{
    return QColor::fromRgba(0xFF599BFF);
}

QColor QQuickDefaultStyle::toolButtonColor() const
{
    return QColor::fromRgba(0x33333333);
}

QColor QQuickDefaultStyle::tabButtonColor() const
{
    return QColor::fromRgba(0xFF353637);
}

QColor QQuickDefaultStyle::tabButtonPressedColor() const
{
    return QColor::fromRgba(0xFF585A5C);
}

QColor QQuickDefaultStyle::tabButtonCheckedPressedColor() const
{
    return QColor::fromRgba(0xFFE4E4E4);
}

QColor QQuickDefaultStyle::delegateColor() const
{
    return QColor::fromRgba(0xFFEEEEEE);
}

QColor QQuickDefaultStyle::delegatePressedColor() const
{
    return QColor::fromRgba(0xFFBDBEBF);
}

QColor QQuickDefaultStyle::delegateFocusColor() const
{
    return QColor::fromRgba(0xFFE5EFFF);
}

QColor QQuickDefaultStyle::indicatorPressedColor() const
{
    return QColor::fromRgba(0xFFF6F6F6);
}

QColor QQuickDefaultStyle::indicatorDisabledColor() const
{
    return QColor::fromRgba(0xFFFDFDFD);
}

QColor QQuickDefaultStyle::indicatorFrameColor() const
{
    return QColor::fromRgba(0xFF909090);
}

QColor QQuickDefaultStyle::indicatorFramePressedColor() const
{
    return QColor::fromRgba(0xFF808080);
}

QColor QQuickDefaultStyle::indicatorFrameDisabledColor() const
{
    return QColor::fromRgba(0xFFD6D6D6);
}

QColor QQuickDefaultStyle::frameDarkColor() const
{
    return QColor::fromRgba(0xFF353637);
}

QColor QQuickDefaultStyle::frameLightColor() const
{
    return QColor::fromRgba(0xFFBDBEBF);
}

QColor QQuickDefaultStyle::scrollBarColor() const
{
    return QColor::fromRgba(0xFFBDBEBF);
}

QColor QQuickDefaultStyle::scrollBarPressedColor() const
{
    return QColor::fromRgba(0xFF28282A);
}

QColor QQuickDefaultStyle::progressBarColor() const
{
    return QColor::fromRgba(0xFFE4E4E4);
}

QColor QQuickDefaultStyle::pageIndicatorColor() const
{
    return QColor::fromRgba(0xFF28282A);
}

QColor QQuickDefaultStyle::separatorColor() const
{
    return QColor::fromRgba(0xFFCCCCCC);
}

QColor QQuickDefaultStyle::disabledDarkColor() const
{
    return QColor::fromRgba(0xFF353637);
}

QColor QQuickDefaultStyle::disabledLightColor() const
{
    return QColor::fromRgba(0xFFBDBEBF);
}

QT_END_NAMESPACE
