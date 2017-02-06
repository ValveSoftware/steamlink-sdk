/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
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

#include "qquickmessagedialog_p.h"
#include <QQuickItem>
#include <private/qguiapplication_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype AbstractMessageDialog
    \instantiates QQuickMessageDialog
    \inqmlmodule QtQuick.Dialogs
    \ingroup qtquick-visual
    \brief API wrapper for QML message dialog implementations
    \since 5.2
    \internal

    AbstractMessageDialog provides only the API for implementing a message dialog.
    The implementation (e.g. a Window or preferably an Item, in case it is
    shown on a device that doesn't support multiple windows) can be provided as
    \l implementation, which is the default property (the only allowed child
    element).
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractMessageDialog::accepted

    This signal is emitted by \l accept().

    The corresponding handler is \c onAccepted.
*/

/*!
    \qmlsignal QtQuick::Dialogs::AbstractMessageDialog::rejected

    This signal is emitted by \l reject().

    The corresponding handler is \c onRejected.
*/

/*!
    \class QQuickMessageDialog
    \inmodule QtQuick.Dialogs
    \internal

    The QQuickMessageDialog class is a concrete subclass of
    \l QQuickAbstractMessageDialog, but it is abstract from the QML perspective
    because it needs to enclose a graphical implementation. It exists in order
    to provide accessors and helper functions which the QML implementation will
    need.

    \since 5.2
*/

/*!
    Constructs a message dialog wrapper with parent window \a parent.
*/
QQuickMessageDialog::QQuickMessageDialog(QObject *parent)
    : QQuickAbstractMessageDialog(parent)
{
}


/*!
    Destroys the message dialog wrapper.
*/
QQuickMessageDialog::~QQuickMessageDialog()
{
}

/*!
    \qmlproperty bool AbstractMessageDialog::visible

    This property holds whether the dialog is visible. By default this is false.
*/

/*!
    \qmlproperty QObject AbstractMessageDialog::implementation

    The QML object which implements the actual message dialog. Should be either a
    \l Window or an \l Item.
*/

void QQuickMessageDialog::accept() {
    // enter key is treated like OK
    if (m_clickedButton == NoButton)
        m_clickedButton = Ok;
    QQuickAbstractMessageDialog::accept();
}

void QQuickMessageDialog::reject() {
    // escape key is treated like cancel
    if (m_clickedButton == NoButton)
        m_clickedButton = Cancel;
    QQuickAbstractMessageDialog::reject();
}

QT_END_NAMESPACE
