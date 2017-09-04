/****************************************************************************
**
** Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwaylandinputmethodcontrol.h"
#include "qwaylandinputmethodcontrol_p.h"

#include "qwaylandcompositor.h"
#include "qwaylandseat.h"
#include "qwaylandsurface.h"
#include "qwaylandview.h"
#include "qwaylandtextinput.h"

#include <QtGui/QInputMethodEvent>

QWaylandInputMethodControl::QWaylandInputMethodControl(QWaylandSurface *surface)
    : QObject(*new QWaylandInputMethodControlPrivate(surface), surface)
{
    connect(d_func()->compositor, &QWaylandCompositor::defaultSeatChanged,
            this, &QWaylandInputMethodControl::defaultSeatChanged);
    QWaylandTextInput *textInput = d_func()->textInput();
    if (textInput) {
        connect(textInput, &QWaylandTextInput::surfaceEnabled, this, &QWaylandInputMethodControl::surfaceEnabled);
        connect(textInput, &QWaylandTextInput::surfaceDisabled, this, &QWaylandInputMethodControl::surfaceDisabled);
        connect(textInput, &QWaylandTextInput::updateInputMethod, this, &QWaylandInputMethodControl::updateInputMethod);
    }
}

QVariant QWaylandInputMethodControl::inputMethodQuery(Qt::InputMethodQuery query, QVariant argument) const
{
    Q_D(const QWaylandInputMethodControl);

    QWaylandTextInput *textInput = d->textInput();

    if (textInput && textInput->focus() == d->surface) {
        return textInput->inputMethodQuery(query, argument);
    }

    return QVariant();
}

void QWaylandInputMethodControl::inputMethodEvent(QInputMethodEvent *event)
{
    Q_D(QWaylandInputMethodControl);

    QWaylandTextInput *textInput = d->textInput();
    if (textInput) {
        textInput->sendInputMethodEvent(event);
    } else {
        event->ignore();
    }
}

bool QWaylandInputMethodControl::enabled() const
{
    Q_D(const QWaylandInputMethodControl);

    return d->enabled;
}

void QWaylandInputMethodControl::setEnabled(bool enabled)
{
    Q_D(QWaylandInputMethodControl);

    if (d->enabled == enabled)
        return;

    d->enabled = enabled;
    emit enabledChanged(enabled);
    emit updateInputMethod(Qt::ImQueryInput);
}

void QWaylandInputMethodControl::surfaceEnabled(QWaylandSurface *surface)
{
    Q_D(QWaylandInputMethodControl);

    if (surface == d->surface)
        setEnabled(true);
}

void QWaylandInputMethodControl::surfaceDisabled(QWaylandSurface *surface)
{
    Q_D(QWaylandInputMethodControl);

    if (surface == d->surface)
        setEnabled(false);
}

void QWaylandInputMethodControl::setSurface(QWaylandSurface *surface)
{
    Q_D(QWaylandInputMethodControl);

    if (d->surface == surface)
        return;

    d->surface = surface;

    QWaylandTextInput *textInput = d->textInput();
    setEnabled(textInput && textInput->isSurfaceEnabled(d->surface));
}

void QWaylandInputMethodControl::defaultSeatChanged()
{
    Q_D(QWaylandInputMethodControl);

    disconnect(d->textInput(), 0, this, 0);

    d->seat = d->compositor->defaultSeat();
    QWaylandTextInput *textInput = d->textInput();

    connect(textInput, &QWaylandTextInput::surfaceEnabled, this, &QWaylandInputMethodControl::surfaceEnabled);
    connect(textInput, &QWaylandTextInput::surfaceDisabled, this, &QWaylandInputMethodControl::surfaceDisabled);

    setEnabled(textInput && textInput->isSurfaceEnabled(d->surface));
}

QWaylandInputMethodControlPrivate::QWaylandInputMethodControlPrivate(QWaylandSurface *surface)
    : QObjectPrivate()
    , compositor(surface->compositor())
    , seat(compositor->defaultSeat())
    , surface(surface)
    , enabled(false)
{
}

QWaylandTextInput *QWaylandInputMethodControlPrivate::textInput() const
{
    return QWaylandTextInput::findIn(seat);
}
