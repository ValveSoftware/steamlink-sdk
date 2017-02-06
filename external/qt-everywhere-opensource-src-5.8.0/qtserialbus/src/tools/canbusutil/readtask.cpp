/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the QtSerialBus module.
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

#include "readtask.h"

ReadTask::ReadTask(QTextStream &output, QObject *parent)
    : QObject(parent),
      output(output) { }

void ReadTask::setShowTimeStamp(bool showTimeStamp)
{
    m_showTimeStamp = showTimeStamp;
}

void ReadTask::checkMessages() {
    auto canDevice = qobject_cast<QCanBusDevice *>(QObject::sender());
    if (canDevice == nullptr) {
        qWarning() << "ReadTask::checkMessages: Unknown sender";
        return;
    }

    while (canDevice->framesAvailable()) {
        const QCanBusFrame frame = canDevice->readFrame();

        QString view;

        if (m_showTimeStamp) {
            view = QString::fromLatin1("%1.%2  ")
                    .arg(frame.timeStamp().seconds(), 10, 10, QLatin1Char(' '))
                    .arg(frame.timeStamp().microSeconds() / 100, 4, 10, QLatin1Char('0'));
        }

        if (frame.frameType() == QCanBusFrame::ErrorFrame)
            view += canDevice->interpretErrorFrame(frame);
        else
            view += frame.toString();

        output << view << endl;
    }
}

void ReadTask::receiveError(QCanBusDevice::CanBusError /*error*/) {
    auto canDevice = qobject_cast<QCanBusDevice *>(QObject::sender());
    if (canDevice == nullptr) {
        qWarning() << "ReadTask::receiveError: Unknown sender";
        return;
    }

    output << "Read error: " << canDevice->errorString() << endl;
}
