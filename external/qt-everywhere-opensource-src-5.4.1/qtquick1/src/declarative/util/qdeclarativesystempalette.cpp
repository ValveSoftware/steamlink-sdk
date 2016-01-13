/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativesystempalette_p.h"

#include <QApplication>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QDeclarativeSystemPalettePrivate : public QObjectPrivate
{
public:
    QPalette palette;
    QPalette::ColorGroup group;
};



/*!
    \qmltype SystemPalette
    \instantiates QDeclarativeSystemPalette
    \ingroup qml-utility-elements
    \since 4.7
    \brief The SystemPalette element provides access to the Qt palettes.

    The SystemPalette element provides access to the Qt application
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

    \snippet doc/src/snippets/declarative/systempalette.qml 0

    \sa QPalette
*/
QDeclarativeSystemPalette::QDeclarativeSystemPalette(QObject *parent)
    : QObject(*(new QDeclarativeSystemPalettePrivate), parent)
{
    Q_D(QDeclarativeSystemPalette);
    d->palette = QApplication::palette();
    d->group = QPalette::Active;
    qApp->installEventFilter(this);
}

QDeclarativeSystemPalette::~QDeclarativeSystemPalette()
{
}

/*!
    \qmlproperty color SystemPalette::window
    The window (general background) color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::window() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Window);
}

/*!
    \qmlproperty color SystemPalette::windowText
    The window text (general foreground) color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::windowText() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::WindowText);
}

/*!
    \qmlproperty color SystemPalette::base
    The base color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::base() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Base);
}

/*!
    \qmlproperty color SystemPalette::text
    The text color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::text() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Text);
}

/*!
    \qmlproperty color SystemPalette::alternateBase
    The alternate base color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::alternateBase() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::AlternateBase);
}

/*!
    \qmlproperty color SystemPalette::button
    The button color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::button() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Button);
}

/*!
    \qmlproperty color SystemPalette::buttonText
    The button text foreground color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::buttonText() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::ButtonText);
}

/*!
    \qmlproperty color SystemPalette::light
    The light color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::light() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Light);
}

/*!
    \qmlproperty color SystemPalette::midlight
    The midlight color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::midlight() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Midlight);
}

/*!
    \qmlproperty color SystemPalette::dark
    The dark color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::dark() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Dark);
}

/*!
    \qmlproperty color SystemPalette::mid
    The mid color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::mid() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Mid);
}

/*!
    \qmlproperty color SystemPalette::shadow
    The shadow color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::shadow() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Shadow);
}

/*!
    \qmlproperty color SystemPalette::highlight
    The highlight color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::highlight() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::Highlight);
}

/*!
    \qmlproperty color SystemPalette::highlightedText
    The highlighted text color of the current color group.

    \sa QPalette::ColorRole
*/
QColor QDeclarativeSystemPalette::highlightedText() const
{
    Q_D(const QDeclarativeSystemPalette);
    return d->palette.color(d->group, QPalette::HighlightedText);
}

/*!
    \qmlproperty enumeration SystemPalette::colorGroup

    The color group of the palette. This can be one of:

    \list
    \li SystemPalette.Active (default)
    \li SystemPalette.Inactive
    \li SystemPalette.Disabled
    \endlist

    \sa QPalette::ColorGroup
*/
QDeclarativeSystemPalette::ColorGroup QDeclarativeSystemPalette::colorGroup() const
{
    Q_D(const QDeclarativeSystemPalette);
    return (QDeclarativeSystemPalette::ColorGroup)d->group;
}

void QDeclarativeSystemPalette::setColorGroup(QDeclarativeSystemPalette::ColorGroup colorGroup)
{
    Q_D(QDeclarativeSystemPalette);
    d->group = (QPalette::ColorGroup)colorGroup;
    emit paletteChanged();
}

bool QDeclarativeSystemPalette::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == qApp) {
        if (event->type() == QEvent::ApplicationPaletteChange) {
            QApplication::postEvent(this, new QEvent(QEvent::ApplicationPaletteChange));
            return false;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool QDeclarativeSystemPalette::event(QEvent *event)
{
    Q_D(QDeclarativeSystemPalette);
    if (event->type() == QEvent::ApplicationPaletteChange) {
        d->palette = QApplication::palette();
        emit paletteChanged();
        return true;
    }
    return QObject::event(event);
}

QT_END_NAMESPACE
