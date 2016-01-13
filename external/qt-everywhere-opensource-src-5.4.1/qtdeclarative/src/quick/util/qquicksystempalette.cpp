/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquicksystempalette_p.h"

#include <QGuiApplication>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQuickSystemPalettePrivate : public QObjectPrivate
{
public:
    QPalette::ColorGroup group;
};



/*!
    \qmltype SystemPalette
    \instantiates QQuickSystemPalette
    \inqmlmodule QtQuick
    \ingroup qtquick-visual-utility
    \brief Provides access to the Qt palettes

    The SystemPalette type provides access to the Qt application
    palettes. This provides information about the standard colors used
    for application windows, buttons and other features. These colors
    are grouped into three \e {color groups}: \c Active, \c Inactive,
    and \c Disabled.  See the QPalette documentation for details about
    color groups and the properties provided by SystemPalette.

    This can be used to color items in a way that provides a more
    native look and feel.

    The following example creates a palette from the \c Active color
    group and uses this to color the window and text items
    appropriately:

    \snippet qml/systempalette.qml 0

    \sa QPalette
*/
QQuickSystemPalette::QQuickSystemPalette(QObject *parent)
    : QObject(*(new QQuickSystemPalettePrivate), parent)
{
    Q_D(QQuickSystemPalette);
    d->group = QPalette::Active;
    connect(qApp, SIGNAL(paletteChanged(QPalette)), this, SIGNAL(paletteChanged()));
}

QQuickSystemPalette::~QQuickSystemPalette()
{
}

/*!
    \qmlproperty color QtQuick::SystemPalette::window
    The window (general background) color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::window() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Window);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::windowText
    The window text (general foreground) color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::windowText() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::WindowText);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::base
    The base color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::base() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Base);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::text
    The text color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::text() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Text);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::alternateBase
    The alternate base color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::alternateBase() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::AlternateBase);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::button
    The button color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::button() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Button);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::buttonText
    The button text foreground color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::buttonText() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::ButtonText);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::light
    The light color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::light() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Light);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::midlight
    The midlight color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::midlight() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Midlight);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::dark
    The dark color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::dark() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Dark);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::mid
    The mid color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::mid() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Mid);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::shadow
    The shadow color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::shadow() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Shadow);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::highlight
    The highlight color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::highlight() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::Highlight);
}

/*!
    \qmlproperty color QtQuick::SystemPalette::highlightedText
    The highlighted text color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QQuickSystemPalette::highlightedText() const
{
    Q_D(const QQuickSystemPalette);
    return QGuiApplication::palette().color(d->group, QPalette::HighlightedText);
}

/*!
    \qmlproperty enumeration QtQuick::SystemPalette::colorGroup

    The color group of the palette. This can be one of:

    \list
    \li SystemPalette.Active (default)
    \li SystemPalette.Inactive
    \li SystemPalette.Disabled
    \endlist

    \sa QPalette::ColorGroup
*/
QQuickSystemPalette::ColorGroup QQuickSystemPalette::colorGroup() const
{
    Q_D(const QQuickSystemPalette);
    return (QQuickSystemPalette::ColorGroup)d->group;
}

void QQuickSystemPalette::setColorGroup(QQuickSystemPalette::ColorGroup colorGroup)
{
    Q_D(QQuickSystemPalette);
    d->group = (QPalette::ColorGroup)colorGroup;
    emit paletteChanged();
}

QT_END_NAMESPACE
