/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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

/*!
    \class QMaskGenerator

    \inmodule QtWebSockets
    \since 5.3

    \brief The QMaskGenerator class provides an abstract base for custom 32-bit mask generators.

    The WebSockets specification as outlined in \l {RFC 6455}
    requires that all communication from client to server be masked. This is to prevent
    malicious scripts from attacking badly behaving proxies.
    For more information about the importance of good masking,
    see \l {"Talking to Yourself for Fun and Profit" by Lin-Shung Huang et al}.
    By default QWebSocket uses the cryptographically insecure qrand() function.
    The best measure against attacks mentioned in the document above,
    is to use QWebSocket over a secure connection (\e wss://).
    In general, always be careful to not have 3rd party script access to
    a QWebSocket in your application.
*/

/*!
    \fn bool QMaskGenerator::seed()

    Initializes the QMaskGenerator by seeding the randomizer.
    When seed() is not called, it depends on the specific implementation of a subclass if
    a default seed is used or no seed is used at all.
    Returns \e true if seeding succeeds, otherwise false.
*/

/*!
    \fn quint32 QMaskGenerator::nextMask()

    Returns a new random 32-bit mask. The randomness depends on the RNG used to created the
    mask.
*/

#include "qmaskgenerator.h"

QT_BEGIN_NAMESPACE

/*!
    Creates a new QMaskGenerator object with the given optional QObject \a parent.
 */
QMaskGenerator::QMaskGenerator(QObject *parent) :
    QObject(parent)
{
}

/*!
    Destroys the QMaskGenerator object.
 */
QMaskGenerator::~QMaskGenerator()
{}

QT_END_NAMESPACE
