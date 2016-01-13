/***************************************************************************
**
** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the QtBluetooth module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#ifndef PINGPONG_H
#define PINGPONG_H

#include <QTimer>
#include <QObject>
#include <qbluetoothserver.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothservicediscoveryagent.h>

static const QString serviceUuid(QStringLiteral("e8e10f95-1a70-4b27-9ccf-02010264e9c9"));

class PingPong: public QObject
{
    Q_OBJECT
    Q_PROPERTY(float ballX READ ballX NOTIFY ballChanged)
    Q_PROPERTY(float ballY READ ballY NOTIFY ballChanged)
    Q_PROPERTY(float leftBlockY READ leftBlockY NOTIFY leftBlockChanged)
    Q_PROPERTY(float rightBlockY READ rightBlockY NOTIFY rightBlockChanged)
    Q_PROPERTY(bool showDialog READ showDialog NOTIFY showDialogChanged)
    Q_PROPERTY(QString message READ message NOTIFY showDialogChanged)
    Q_PROPERTY(int role READ role NOTIFY roleChanged)
    Q_PROPERTY(int leftResult READ leftResult NOTIFY resultChanged)
    Q_PROPERTY(int rightResult READ rightResult NOTIFY resultChanged)
public:
    PingPong();
    ~PingPong();
    float ballX() const;
    float ballY() const;
    float leftBlockY() const;
    float rightBlockY() const;
    void checkBoundaries();
    void updateDirection();
    bool showDialog() const;
    QString message() const;
    void setMessage(const QString &message);
    int role() const;
    int leftResult() const;
    int rightResult() const;
    void checkResult();

public slots:
    void startGame();
    void update();
    void setSize(const float &x, const float &y);
    void updateBall(const float &bX, const float &bY);
    void updateLeftBlock(const float &lY);
    void updateRightBlock(const float &rY);
    void startServer();
    void startClient();
    void clientConnected();
    void clientDisconnected();
    void serverConnected();
    void serverDisconnected();
    void socketError(QBluetoothSocket::SocketError);
    void serverError(QBluetoothServer::Error);
    void serviceScanError(QBluetoothServiceDiscoveryAgent::Error);
    void done();
    void addService(const QBluetoothServiceInfo &);
    void readSocket();

Q_SIGNALS:
    void ballChanged();
    void leftBlockChanged();
    void rightBlockChanged();
    void showDialogChanged();
    void roleChanged();
    void resultChanged();

private:
    QBluetoothServer *m_serverInfo;
    QBluetoothServiceInfo m_serviceInfo;
    QBluetoothSocket *socket;
    QBluetoothServiceDiscoveryAgent *discoveryAgent;

    float m_ballX;
    float m_ballY;
    float m_ballPreviousX;
    float m_ballPreviousY;
    float m_leftBlockY;
    float m_rightBlockY;
    QTimer *m_timer;
    float m_boardWidth;
    float m_boardHeight;
    float m_direction;
    float m_targetX;
    float m_targetY;
    int interval;
    int m_resultLeft;
    int m_resultRight;
    bool m_showDialog;
    QString m_message;
    int m_role;
    float m_proportionX;
    float m_proportionY;
    bool m_serviceFound;
};

#endif // PINGPONG_H
