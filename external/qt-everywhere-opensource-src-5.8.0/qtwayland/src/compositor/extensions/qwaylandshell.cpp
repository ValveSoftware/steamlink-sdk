/****************************************************************************
**
** Copyright (C) 2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwaylandshell.h"
#include "qwaylandshell_p.h"

QT_BEGIN_NAMESPACE

QWaylandShellPrivate::QWaylandShellPrivate()
    : focusPolicy(QWaylandShell::AutomaticFocus)
{
}

QWaylandShell::QWaylandShell()
    : QWaylandCompositorExtension()
{
}

QWaylandShell::QWaylandShell(QWaylandObject *waylandObject)
    : QWaylandCompositorExtension(waylandObject, *new QWaylandShellPrivate())
{
}

/*!
 * \qmlproperty enum QtWaylandCompositor::Shell::focusPolicy
 *
 * This property holds the focus policy of the Shell.
 */

/*!
 * \enum QWaylandShell::FocusPolicy
 *
 * This enum type is used to specify the focus policy for shell surfaces.
 *
 * \value AutomaticFocus Shell surfaces will automatically get keyboard focus when they are created.
 * \value ManualFocus The compositor will decide whether shell surfaces should get keyboard focus or not.
 */

/*!
 * \qmlproperty object QtWaylandCompositor::Shell::focusPolicy
 *
 * This property holds the focus policy of the Shell.
 */

/*!
 * \property QWaylandShell::focusPolicy
 *
 * This property holds the focus policy of the QWaylandShell.
 */
QWaylandShell::FocusPolicy QWaylandShell::focusPolicy() const
{
    Q_D(const QWaylandShell);
    return d->focusPolicy;
}

void QWaylandShell::setFocusPolicy(QWaylandShell::FocusPolicy focusPolicy)
{
    Q_D(QWaylandShell);

    if (d->focusPolicy == focusPolicy)
        return;

    d->focusPolicy = focusPolicy;
    emit focusPolicyChanged();
}

QT_END_NAMESPACE
