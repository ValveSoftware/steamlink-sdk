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
    \class QDefaultMaskGenerator

    \inmodule QtWebSockets

    \brief The QDefaultMaskGenerator class provides the default mask generator for QtWebSockets.

    The WebSockets specification as outlined in \l {RFC 6455}
    requires that all communication from client to server must be masked. This is to prevent
    malicious scripts to attack bad behaving proxies.
    For more information about the importance of good masking,
    see \l {"Talking to Yourself for Fun and Profit" by Lin-Shung Huang et al}.
    The default mask generator uses the cryptographically insecure qrand() function.
    The best measure against attacks mentioned in the document above,
    is to use QWebSocket over a secure connection (\e wss://).
    In general, always be careful to not have 3rd party script access to
    a QWebSocket in your application.

    \internal
*/

#include "qdefaultmaskgenerator_p.h"
#include <QDateTime>
#include <limits>

QT_BEGIN_NAMESPACE

/*!
    Constructs a new QDefaultMaskGenerator with the given \a parent.

    \internal
*/
QDefaultMaskGenerator::QDefaultMaskGenerator(QObject *parent) :
    QMaskGenerator(parent)
{
}

/*!
    Destroys the QDefaultMaskGenerator object.

    \internal
*/
QDefaultMaskGenerator::~QDefaultMaskGenerator()
{
}

/*!
    Seeds the QDefaultMaskGenerator using qsrand().
    When seed() is not called, no seed is used at all.

    \internal
*/
bool QDefaultMaskGenerator::seed() Q_DECL_NOEXCEPT
{
    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    return true;
}

/*!
    Generates a new random mask using the insecure qrand() method.

    \internal
*/
quint32 QDefaultMaskGenerator::nextMask() Q_DECL_NOEXCEPT
{
    return quint32((double(qrand()) / RAND_MAX) * std::numeric_limits<quint32>::max());
}

QT_END_NAMESPACE
