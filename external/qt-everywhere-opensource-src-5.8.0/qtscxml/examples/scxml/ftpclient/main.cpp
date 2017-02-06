/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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


#include "simpleftp.h"
#include "ftpcontrolchannel.h"
#include "ftpdatachannel.h"
#include <QCoreApplication>
#include <iostream>

struct Command {
    QString cmd;
    QString args;
};

int main(int argc, char *argv[])
{
    if (argc != 3) {
        qDebug() << "Usage: ftpclient <server> <file>";
        return 1;
    }

    QString server = QString::fromLocal8Bit(argv[1]);
    QString file = QString::fromLocal8Bit(argv[2]);

    QCoreApplication app(argc, argv);
    FtpClient ftpClient;
    FtpDataChannel dataChannel;
    FtpControlChannel controlChannel;

    // Print all data retrieved from the server on the console.
    QObject::connect(&dataChannel, &FtpDataChannel::dataReceived, [](const QByteArray &data) {
        std::cout << data.constData();
    });

    // Translate server replies into state machine events.
    QObject::connect(&controlChannel, &FtpControlChannel::reply, &ftpClient,
                     [&ftpClient](int code, const QString &parameters) {
        ftpClient.submitEvent(QString("reply.%1xx").arg(code / 100), parameters);
    });

    // Translate commands from the state machine into FTP control messages.
    ftpClient.connectToEvent("submit.cmd", &controlChannel,
                             [&controlChannel](const QScxmlEvent &event) {
        controlChannel.command(event.name().mid(11).toUtf8(), event.data().toByteArray());
    });

    // Commands to be sent
    QList<Command> commands({
        {"cmd.USER", "anonymous"},// login
        {"cmd.PORT", ""},         // announce port for data connection, args added below.
        {"cmd.RETR", file}        // retrieve a file
    });

    // When entering "B" state, send the next command.
    ftpClient.connectToState("B", QScxmlStateMachine::onEntry([&]() {
        if (commands.isEmpty()) {
            app.quit();
            return;
        }
        Command command = commands.takeFirst();
        qDebug() << "Posting command" << command.cmd << command.args;
        ftpClient.submitEvent(command.cmd, command.args);
    }));

    // If the server asks for a password, send one.
    ftpClient.connectToState("P", QScxmlStateMachine::onEntry([&ftpClient]() {
        qDebug() << "Sending password";
        ftpClient.submitEvent("cmd.PASS", QString());
    }));

    // Connect to our own local FTP server
    controlChannel.connectToServer(server);
    QObject::connect(&controlChannel, &FtpControlChannel::opened,
                     [&](const QHostAddress &address, int) {
        dataChannel.listen(address);
        commands[1].args = dataChannel.portspec();
        ftpClient.start();
    });

    return app.exec();
}
