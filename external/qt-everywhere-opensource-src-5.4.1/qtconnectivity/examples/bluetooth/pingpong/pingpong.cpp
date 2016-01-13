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

#include "pingpong.h"
#include <QDebug>

PingPong::PingPong():
    m_serverInfo(0), socket(0), discoveryAgent(0), interval(5), m_resultLeft(0), m_resultRight(0),
    m_showDialog(false), m_role(0), m_proportionX(0), m_proportionY(0), m_serviceFound(false)
{
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(update()));
}

PingPong::~PingPong()
{
    delete m_timer;
    delete m_serverInfo;
    delete socket;
    delete discoveryAgent;
}

void PingPong::startGame()
{
    m_showDialog = false;
    emit showDialogChanged();
    //! [Start the game]
    if (m_role == 1)
        updateDirection();

    m_timer->start(50);
    //! [Start the game]
}

void PingPong::update()
{
    QByteArray size;
    // Server is only updating the coordinates
    //! [Updating coordinates]
    if (m_role == 1) {
        checkBoundaries();
        m_ballPreviousX = m_ballX;
        m_ballPreviousY = m_ballY;
        m_ballY = m_direction*(m_ballX+interval) - m_direction*m_ballX + m_ballY;
        m_ballX = m_ballX + interval;

        size.setNum(m_ballX);
        size.append(' ');
        QByteArray size1;
        size1.setNum(m_ballY);
        size.append(size1);
        size.append(' ');
        size1.setNum(m_leftBlockY);
        size.append(size1);
        size.append(" \n");
        socket->write(size.constData());
        emit ballChanged();
    }
    else if (m_role == 2) {
        size.setNum(m_rightBlockY);
        size.append(" \n");
        socket->write(size.constData());
    }
    //! [Updating coordinates]
}



void PingPong::setSize(const float &x, const float &y)
{
    m_boardWidth = x;
    m_boardHeight = y;
    m_targetX = m_boardWidth;
    m_targetY = m_boardHeight/2;
    m_ballPreviousX = m_ballX = m_boardWidth/2;
    m_ballPreviousY = m_ballY = m_boardHeight - m_boardWidth/54;
    emit ballChanged();
}

void PingPong::updateBall(const float &bX, const float &bY)
{
    m_ballX = bX;
    m_ballY = bY;
}

void PingPong::updateLeftBlock(const float &lY)
{
    m_leftBlockY = lY;
}

void PingPong::updateRightBlock(const float &rY)
{
    m_rightBlockY = rY;
}

void PingPong::checkBoundaries()
{
    float ballWidth = m_boardWidth/54;
    float blockSize = m_boardWidth/27;
    float blockHeight = m_boardHeight/5;
    //! [Checking the boundaries]
    if (((m_ballX + ballWidth) > (m_boardWidth - blockSize)) && ((m_ballY + ballWidth) < (m_rightBlockY + blockHeight))
            && (m_ballY > m_rightBlockY)) {
        m_targetY = 2 * m_ballY - m_ballPreviousY;
        m_targetX = m_ballPreviousX;
        interval = -5;
        updateDirection();
    }
    else if ((m_ballX < blockSize) && ((m_ballY + ballWidth) < (m_leftBlockY + blockHeight))
             && (m_ballY > m_leftBlockY)) {
        m_targetY = 2 * m_ballY - m_ballPreviousY;
        m_targetX = m_ballPreviousX;
        interval = 5;
        updateDirection();
    }
    else if (m_ballY < 0 || (m_ballY + ballWidth > m_boardHeight)) {
        m_targetY = m_ballPreviousY;
        m_targetX = m_ballX + interval;
        updateDirection();
    }
    //! [Checking the boundaries]
    else if ((m_ballX + ballWidth) > m_boardWidth) {
        m_resultLeft++;
        m_targetX = m_boardWidth;
        m_targetY = m_boardHeight/2;
        m_ballPreviousX = m_ballX = m_boardWidth/2;
        m_ballPreviousY = m_ballY = m_boardHeight - m_boardWidth/54;

        updateDirection();
        checkResult();
        QByteArray result;
        result.append("result ");
        QByteArray res;
        res.setNum(m_resultLeft);
        result.append(res);
        result.append(' ');
        res.setNum(m_resultRight);
        result.append(res);
        result.append(" \n");
        socket->write(result);
        qDebug() << result;
        emit resultChanged();
    }
    else if (m_ballX < 0) {
        m_resultRight++;
        m_targetX = 0;
        m_targetY = m_boardHeight/2;
        m_ballPreviousX = m_ballX = m_boardWidth/2;
        m_ballPreviousY = m_ballY = m_boardHeight - m_boardWidth/54;
        updateDirection();
        checkResult();
        QByteArray result;
        result.append("result ");
        QByteArray res;
        res.setNum(m_resultLeft);
        result.append(res);
        result.append(' ');
        res.setNum(m_resultRight);
        result.append(res);
        result.append(" \n");
        socket->write(result);
        emit resultChanged();
    }
}

void PingPong::checkResult()
{
    if (m_resultRight == 10 && m_role == 2) {
        setMessage("Game over. You win!");
        m_timer->stop();
    }
    else if (m_resultRight == 10 && m_role == 1) {
        setMessage("Game over. You lose!");
        m_timer->stop();
    }
    else if (m_resultLeft == 10 && m_role == 1) {
        setMessage("Game over. You win!");
        m_timer->stop();
    }
    else if (m_resultLeft == 10 && m_role == 2) {
        setMessage("Game over. You lose!");
        m_timer->stop();
    }
}

void PingPong::updateDirection()
{
    m_direction = (m_targetY - m_ballY)/(m_targetX - m_ballX);
}

void PingPong::startServer()
{
    setMessage(QStringLiteral("Starting the server"));
    //! [Starting the server]
    m_serverInfo = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_serverInfo, SIGNAL(newConnection()), this, SLOT(clientConnected()));
    connect(m_serverInfo, SIGNAL(error(QBluetoothServer::Error)),
            this, SLOT(serverError(QBluetoothServer::Error)));
    const QBluetoothUuid uuid(serviceUuid);

    m_serverInfo->listen(uuid, QStringLiteral("PingPong server"));
    //! [Starting the server]
    setMessage(QStringLiteral("Server started, waiting for the client. You are the left player."));
    // m_role is set to 1 if it is a server
    m_role = 1;
    emit roleChanged();
}

void PingPong::startClient()
{
    //! [Searching for the service]
    discoveryAgent = new QBluetoothServiceDiscoveryAgent(QBluetoothAddress());

    connect(discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(addService(QBluetoothServiceInfo)));
    connect(discoveryAgent, SIGNAL(finished()), this, SLOT(done()));
    connect(discoveryAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)),
            this, SLOT(serviceScanError(QBluetoothServiceDiscoveryAgent::Error)));
    discoveryAgent->setUuidFilter(QBluetoothUuid(serviceUuid));
    discoveryAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
    //! [Searching for the service]
    setMessage(QStringLiteral("Starting server discovery. You are the right player"));
    // m_role is set to 2 if it is a client
    m_role = 2;
    emit roleChanged();
}

void PingPong::clientConnected()
{
    //! [Initiating server socket]
    if (!m_serverInfo->hasPendingConnections()) {
        setMessage("FAIL: expected pending server connection");
        return;
    }
    socket = m_serverInfo->nextPendingConnection();
    if (!socket)
        return;
    socket->setParent(this);
    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(clientDisconnected()));
    connect(socket, SIGNAL(error(QBluetoothSocket::SocketError)),
            this, SLOT(socketError(QBluetoothSocket::SocketError)));
    //! [Initiating server socket]
    setMessage(QStringLiteral("Client connected."));

    QByteArray size;
    size.setNum(m_boardWidth);
    size.append(' ');
    QByteArray size1;
    size1.setNum(m_boardHeight);
    size.append(size1);
    size.append(" \n");
    socket->write(size.constData());

}

void PingPong::clientDisconnected()
{
    setMessage(QStringLiteral("Client disconnected"));
    m_timer->stop();
}

void PingPong::socketError(QBluetoothSocket::SocketError error)
{
    Q_UNUSED(error);
    m_timer->stop();
}

void PingPong::serverError(QBluetoothServer::Error error)
{
    Q_UNUSED(error);
    m_timer->stop();
}

void PingPong::done()
{
    qDebug() << "Service scan done";
    if (!m_serviceFound)
        setMessage("PingPong service not found");
}

void PingPong::addService(const QBluetoothServiceInfo &service)
{
    setMessage("Service found. Setting parameters...");
    //! [Connecting the socket]
    socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    socket->connectToService(service);

    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
    connect(socket, SIGNAL(connected()), this, SLOT(serverConnected()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(serverDisconnected()));
    //! [Connecting the socket]
    m_serviceFound = true;
}

void PingPong::serviceScanError(QBluetoothServiceDiscoveryAgent::Error error)
{
    setMessage(QStringLiteral("Scanning error") + error);
}

bool PingPong::showDialog() const
{
    return m_showDialog;
}

QString PingPong::message() const
{
    return m_message;
}

void PingPong::serverConnected()
{
    setMessage("Server Connected");
    QByteArray size;
    size.setNum(m_boardWidth);
    size.append(' ');
    QByteArray size1;
    size1.setNum(m_boardHeight);
    size.append(size1);
    size.append(" \n");
    socket->write(size.constData());
}

void PingPong::serverDisconnected()
{
    setMessage("Server Disconnected");
    m_timer->stop();
}

void PingPong::readSocket()
{
    if (!socket)
        return;
    const char sep = ' ';
    QByteArray line;
    while (socket->canReadLine()) {
        line = socket->readLine();
        //qDebug() << QString::fromUtf8(line.constData(), line.length());
        if (line.contains("result")) {
            QList<QByteArray> result = line.split(sep);
            if (result.size() > 2) {
                QByteArray leftSide = result.at(1);
                QByteArray rightSide = result.at(2);
                m_resultLeft = leftSide.toInt();
                m_resultRight = rightSide.toInt();
                emit resultChanged();
                checkResult();
            }
        }
    }
    if ((m_proportionX == 0 || m_proportionY == 0)) {
        QList<QByteArray> boardSize = line.split(sep);
        if (boardSize.size() > 1) {
            QByteArray boardWidth = boardSize.at(0);
            QByteArray boardHeight = boardSize.at(1);
            m_proportionX = m_boardWidth/boardWidth.toFloat();
            m_proportionY = m_boardHeight/boardHeight.toFloat();
            setMessage("Screen adjusted. Get ready!");
            QTimer::singleShot(3000, this, SLOT(startGame()));
        }
    }
    else if (m_role == 1) {
        QList<QByteArray> boardSize = line.split(sep);
        if (boardSize.size() > 1) {
            QByteArray rightBlockY = boardSize.at(0);
            m_rightBlockY = m_proportionY * rightBlockY.toFloat();
            emit rightBlockChanged();
        }
    }
    else if (m_role == 2) {
        QList<QByteArray> boardSize = line.split(sep);
        if (boardSize.size() > 2) {
            QByteArray ballX = boardSize.at(0);
            QByteArray ballY = boardSize.at(1);
            QByteArray leftBlockY = boardSize.at(2);
            m_ballX = m_proportionX * ballX.toFloat();
            m_ballY = m_proportionY * ballY.toFloat();
            m_leftBlockY = m_proportionY * leftBlockY.toFloat();
            emit leftBlockChanged();
            emit ballChanged();
        }
    }
}

void PingPong::setMessage(const QString &message)
{
    m_showDialog = true;
    m_message = message;
    emit showDialogChanged();
}

int PingPong::role() const
{
    return m_role;
}

int PingPong::leftResult() const
{
    return m_resultLeft;
}

int PingPong::rightResult() const
{
    return m_resultRight;
}

float PingPong::ballX() const
{
    return m_ballX;
}

float PingPong::ballY() const
{
    return m_ballY;
}


float PingPong::leftBlockY() const
{
    return m_leftBlockY;
}

float PingPong::rightBlockY() const
{
    return m_rightBlockY;
}
