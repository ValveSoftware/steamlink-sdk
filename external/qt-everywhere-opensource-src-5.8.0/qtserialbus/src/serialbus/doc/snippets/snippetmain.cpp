/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QVariant>
#include <QCanBusDevice>

int main(int /*argc*/, char ** /*argv*/)
{
    QCanBusDevice *device = nullptr;

    //! [Filter Examples]
    QCanBusDevice::Filter filter;
    QList<QCanBusDevice::Filter> filterList;

    // filter all CAN bus packages with id 0x444 (base) or 0xXXXXX444 (extended)
    filter.frameId = 0x444u;
    filter.frameIdMask = 0x7FFu;
    filter.format = QCanBusDevice::Filter::MatchBaseAndExtendedFormat;
    filter.type = QCanBusFrame::InvalidFrame;
    filterList.append(filter);

    // filter all DataFrames with extended CAN bus package format
    filter.frameId = 0x0;
    filter.frameIdMask = 0x0;
    filter.format = QCanBusDevice::Filter::MatchExtendedFormat;
    filter.type = QCanBusFrame::DataFrame;
    filterList.append(filter);

    // apply filter
    device->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(filterList));
    //! [Filter Examples]

    //! [SocketCan Filter Example]
    QList<QCanBusDevice::Filter> list;
    QCanBusDevice::Filter f;

    // only accept odd numbered frame id of type remote request
    // frame can utilize extended or base format
    f.frameId = 0x1;
    f.frameIdMask = 0x1;
    f.format = QCanBusDevice::Filter::MatchBaseAndExtendedFormat;
    f.type = QCanBusFrame::RemoteRequestFrame;
    list.append(f);

    device->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(list));
    device->setConfigurationParameter(QCanBusDevice::ErrorFilterKey,
                                      QVariant::fromValue(QCanBusFrame::FrameErrors(QCanBusFrame::AnyError)));
    //! [SocketCan Filter Example]

    Q_UNUSED(filter);
    return 0;
}

