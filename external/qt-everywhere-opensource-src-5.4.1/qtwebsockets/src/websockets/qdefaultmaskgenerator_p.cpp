/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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
/*!
    \class QDefaultMaskGenerator

    \inmodule QtWebSockets

    \brief The QDefaultMaskGenerator class provides the default mask generator for QtWebSockets.

    The WebSockets specification as outlined in {http://tools.ietf.org/html/rfc6455}{RFC 6455}
    requires that all communication from client to server must be masked. This is to prevent
    malicious scripts to attack bad behaving proxies.
    For more information about the importance of good masking,
    see \l {http://w2spconf.com/2011/papers/websocket.pdf}.
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
