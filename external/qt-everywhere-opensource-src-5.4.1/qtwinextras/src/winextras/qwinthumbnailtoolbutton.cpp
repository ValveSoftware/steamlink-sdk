/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include "qwinthumbnailtoolbutton.h"
#include "qwinthumbnailtoolbutton_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QWinThumbnailToolButton
    \inmodule QtWinExtras
    \since 5.2
    \brief The QWinThumbnailToolButton class represents a button in a thumbnail toolbar.

    Buttons in a QWinThumbnailToolBar are instances of QWinThumbnailToolButton.
    It provides a set of properties for specifying the attributes of a thumbnail
    toolbar button. It also provides a signal that is emitted whenever the button
    is \l{clicked()}{clicked}, and a slot to perform \l{click()}{clicks}
    programmatically.

    \sa QWinThumbnailToolBar
 */

/*!
    \fn void QWinThumbnailToolButton::clicked()

    This signal is emitted when the button is clicked.
 */

/*!
    \internal
    \fn void QWinThumbnailToolButton::changed()
 */

/*!
    Constructs a QWinThumbnailToolButton with the specified \a parent.
 */
QWinThumbnailToolButton::QWinThumbnailToolButton(QObject *parent) :
    QObject(parent), d_ptr(new QWinThumbnailToolButtonPrivate)
{
}

/*!
    Destroys the QWinThumbnailToolButton.
 */
QWinThumbnailToolButton::~QWinThumbnailToolButton()
{
}

/*!
    \property QWinThumbnailToolButton::toolTip
    \brief the tooltip of the button
 */
void QWinThumbnailToolButton::setToolTip(const QString &toolTip)
{
    Q_D(QWinThumbnailToolButton);
    if (d->toolTip != toolTip) {
        d->toolTip = toolTip;
        emit changed();
    }
}

QString QWinThumbnailToolButton::toolTip() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->toolTip;
}

/*!
    \property QWinThumbnailToolButton::icon
    \brief the icon of the button
 */
void QWinThumbnailToolButton::setIcon(const QIcon &icon)
{
    Q_D(QWinThumbnailToolButton);
    if (d->icon.cacheKey() != icon.cacheKey()) {
        d->icon = icon;
        emit changed();
    }
}

QIcon QWinThumbnailToolButton::icon() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->icon;
}

/*!
    \property QWinThumbnailToolButton::enabled
    \brief whether the button is enabled

    The default value is \c true.

    A disabled button does not react to user interaction,
    and is also visually disabled.

    \sa interactive
 */
void QWinThumbnailToolButton::setEnabled(bool enabled)
{
    Q_D(QWinThumbnailToolButton);
    if (d->enabled != enabled) {
        d->enabled = enabled;
        emit changed();
    }
}

bool QWinThumbnailToolButton::isEnabled() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->enabled;
}

/*!
    \property QWinThumbnailToolButton::interactive
    \brief whether the button is interactive

    The default value is \c true.

    A non-interactive button does not react to user interaction,
    but is still visually enabled. A typical use case for non-
    interactive buttons are notification icons.

    \sa enabled
 */
void QWinThumbnailToolButton::setInteractive(bool interactive)
{
    Q_D(QWinThumbnailToolButton);
    if (d->interactive != interactive) {
        d->interactive = interactive;
        emit changed();
    }
}

bool QWinThumbnailToolButton::isInteractive() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->interactive;
}

/*!
    \property QWinThumbnailToolButton::visible
    \brief whether the button is visible

    The default value is \c true.
 */
void QWinThumbnailToolButton::setVisible(bool visible)
{
    Q_D(QWinThumbnailToolButton);
    if (d->visible != visible) {
        d->visible = visible;
        emit changed();
    }
}

bool QWinThumbnailToolButton::isVisible() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->visible;
}

/*!
    \property QWinThumbnailToolButton::dismissOnClick
    \brief whether the window thumbnail is dismissed after a button click

    The default value is \c false.
 */
void QWinThumbnailToolButton::setDismissOnClick(bool dismiss)
{
    Q_D(QWinThumbnailToolButton);
    if (d->dismiss != dismiss) {
        d->dismiss = dismiss;
        emit changed();
    }
}

bool QWinThumbnailToolButton::dismissOnClick() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->dismiss;
}

/*!
    \property QWinThumbnailToolButton::flat
    \brief whether the button is flat

    The default value is \c false.

    A flat button does not draw a background nor a frame - only an icon.
 */
void QWinThumbnailToolButton::setFlat(bool flat)
{
    Q_D(QWinThumbnailToolButton);
    if (d->flat != flat) {
        d->flat = flat;
        emit changed();
    }
}

bool QWinThumbnailToolButton::isFlat() const
{
    Q_D(const QWinThumbnailToolButton);
    return d->flat;
}

/*!
    Performs a click. The clicked() signal is emitted as appropriate.

    This function does nothing if the button is \l{enabled}{disabled}
    or \l{interactive}{non-interactive}.
 */
void QWinThumbnailToolButton::click()
{
    Q_D(QWinThumbnailToolButton);
    if (d->enabled && d->interactive)
        emit clicked();
}

QT_END_NAMESPACE
