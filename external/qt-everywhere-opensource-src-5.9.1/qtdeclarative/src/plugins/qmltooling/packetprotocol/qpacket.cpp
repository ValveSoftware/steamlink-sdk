/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qpacket_p.h"

QT_BEGIN_NAMESPACE

/*!
  \class QPacket
  \internal

  \brief The QPacket class encapsulates an unfragmentable packet of data to be
  transmitted by QPacketProtocol.

  The QPacket class works together with QPacketProtocol to make it simple to
  send arbitrary sized data "packets" across fragmented transports such as TCP
  and UDP.

  QPacket provides a QDataStream interface to an unfragmentable packet.
  Applications should construct a QPacket, propagate it with data and then
  transmit it over a QPacketProtocol instance.  For example:
  \code
  int version = QDataStream::Qt_DefaultCompiledVersion;
  QPacketProtocol protocol(...);

  QPacket myPacket(version);
  myPacket << "Hello world!" << 123;
  protocol.send(myPacket.data());
  \endcode

  As long as both ends of the connection are using the QPacketProtocol class
  and the same data stream version, the data within this packet will be
  delivered unfragmented at the other end, ready for extraction.

  \code
  QByteArray greeting;
  int count;

  QPacket myPacket(version, protocol.read());

  myPacket >> greeting >> count;
  \endcode

  Only packets constructed from raw byte arrays may be read from. Empty QPacket
  instances are for transmission only and are considered "write only". Attempting
  to read data from them will result in undefined behavior.

  \ingroup io
  \sa QPacketProtocol
 */


/*!
  Constructs an empty write-only packet.
  */
QPacket::QPacket(int version)
{
    buf.open(QIODevice::WriteOnly);
    setDevice(&buf);
    setVersion(version);
}

/*!
  Constructs a read-only packet.
 */
QPacket::QPacket(int version, const QByteArray &data)
{
    buf.setData(data);
    buf.open(QIODevice::ReadOnly);
    setDevice(&buf);
    setVersion(version);
}

/*!
  Returns a reference to the raw packet data.
  */
const QByteArray &QPacket::data() const
{
    return buf.data();
}

/*!
  Returns a copy of the raw packet data, with extra reserved space removed.
  Mind that this triggers a deep copy. Use it if you anticipate the data to be detached soon anyway.
  */
QByteArray QPacket::squeezedData() const
{
    QByteArray ret = buf.data();
    ret.squeeze();
    return ret;
}

/*!
  Clears the packet, discarding any data.
 */
void QPacket::clear()
{
    buf.reset();
    QByteArray &buffer = buf.buffer();
    // Keep the old size to prevent unnecessary allocations
    buffer.reserve(buffer.capacity());
    buffer.truncate(0);
}

QT_END_NAMESPACE
