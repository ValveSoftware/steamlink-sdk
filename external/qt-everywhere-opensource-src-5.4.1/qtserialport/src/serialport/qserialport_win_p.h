/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#ifndef QSERIALPORT_WIN_P_H
#define QSERIALPORT_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qserialport_p.h"

#include <QtCore/qhash.h>

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

class QWinEventNotifier;
class QTimer;

class QSerialPortPrivate : public QSerialPortPrivateData
{
    Q_DECLARE_PUBLIC(QSerialPort)

public:
    QSerialPortPrivate(QSerialPort *q);
    ~QSerialPortPrivate();

    bool open(QIODevice::OpenMode mode);
    void close();

    QSerialPort::PinoutSignals pinoutSignals();

    bool setDataTerminalReady(bool set);
    bool setRequestToSend(bool set);

    bool flush();
    bool clear(QSerialPort::Directions directions);

    bool sendBreak(int duration);
    bool setBreakEnabled(bool set);

    qint64 readData(char *data, qint64 maxSize);

    bool waitForReadyRead(int msec);
    bool waitForBytesWritten(int msec);

    bool setBaudRate();
    bool setBaudRate(qint32 baudRate, QSerialPort::Directions directions);
    bool setDataBits(QSerialPort::DataBits dataBits);
    bool setParity(QSerialPort::Parity parity);
    bool setStopBits(QSerialPort::StopBits stopBits);
    bool setFlowControl(QSerialPort::FlowControl flowControl);
    bool setDataErrorPolicy(QSerialPort::DataErrorPolicy policy);

    void handleLineStatusErrors();
    QSerialPort::SerialPortError decodeSystemError() const;

    bool _q_completeAsyncCommunication();
    bool _q_completeAsyncRead();
    bool _q_completeAsyncWrite();

    bool startAsyncCommunication();
    bool startAsyncRead();
    bool _q_startAsyncWrite();

    bool emulateErrorPolicy();
    void emitReadyRead();

    qint64 bytesToWrite() const;
    qint64 writeData(const char *data, qint64 maxSize);

    static qint32 baudRateFromSetting(qint32 setting);
    static qint32 settingFromBaudRate(qint32 baudRate);

    static QList<qint32> standardBaudRates();

    DCB currentDcb;
    DCB restoredDcb;
    COMMTIMEOUTS currentCommTimeouts;
    COMMTIMEOUTS restoredCommTimeouts;
    HANDLE handle;
    bool parityErrorOccurred;
    QByteArray readChunkBuffer;
    bool readyReadEmitted;
    bool writeStarted;
    bool readStarted;
    QWinEventNotifier *communicationNotifier;
    QWinEventNotifier *readCompletionNotifier;
    QWinEventNotifier *writeCompletionNotifier;
    QTimer *startAsyncWriteTimer;
    OVERLAPPED communicationOverlapped;
    OVERLAPPED readCompletionOverlapped;
    OVERLAPPED writeCompletionOverlapped;
    DWORD originalEventMask;
    DWORD triggeredEventMask;
    qint64 actualBytesToWrite;

private:
    bool initialize(QIODevice::OpenMode mode);
    bool updateDcb();
    bool updateCommTimeouts();
    qint64 overlappedResult(OVERLAPPED &overlapped);

    bool waitAnyEvent(int msecs, HANDLE *triggeredEvent);

};

QT_END_NAMESPACE

#endif // QSERIALPORT_WIN_P_H
