/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 basysKom GmbH, author Bernd Lamecker <bernd.lamecker@basyskom.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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
'use strict';

var readline = require('readline');
var QWebChannel = require('./qwebchannel.js').QWebChannel;
var websocket = require('faye-websocket');

var address = 'ws://127.0.0.1:12345';
var socket = new websocket.Client(address);

console.log('Chat client connecting to ' + address + ' (Ctrl-D to quit)');

var createReadlineInterface = function() {
    var bye = function() {
        console.log('Bye...');
        process.exit(0);
    }

    var rlif = readline.createInterface({
        input: process.stdin,
        output: process.stdout
    });
    rlif.setPrompt('chat: ');

    // Handle Ctrl-D and Ctrl-C
    rlif.on('close', bye);
    rlif.on('SIGINT', bye);

    return rlif;
}

var createWebChannel = function(transport, rlif) {
    return new QWebChannel(transport, function(channel) {
        // We connect to the 'sendText' signal of the remote QObject
        // Be aware, that the signal is named for the remote side,
        // i.e. the server wants to 'send text'.
        // This can be confusing, as we connect to the signal
        // to receive incoming messages on our side
        channel.objects.dialog.sendText.connect(function(message) {
            process.stdout.cursorTo(0);
            process.stdout.clearLine(0);
            console.log(' <<   ' + message);
            rlif.prompt();
            // Go to end of existing input if any
            rlif.write(null, {ctrl: true, name: 'e'})
        });

        rlif.on('line', function(line) {
            var l = line.trim();
            if (l !== '') {
                process.stdout.moveCursor(0, -1);
                process.stdout.cursorTo(0);
                process.stdout.clearLine(0);
                // The 'receiveText' slot of the remote QObject
                // is called with our message.
                // Again the naming is for the server side,
                // i.e. the slot is used _by the server_ to receive text.
                channel.objects.dialog.receiveText(l);
                console.log('   >> ' + l);
            }
            rlif.prompt();
        });
        rlif.prompt();
    });
}

socket.on('open', function(event) {
    console.log("info: Client connected");
    var transport = {
        // We cant't do 'send: socket.send' here
        // because 'send' wouldn't be bound to 'socket'
        send: function(data) {socket.send(data)}
    };

    createWebChannel(transport, createReadlineInterface());

    // QWebChannel has set up its onmessage handler
    // on the transport in the constructor.
    // Now we connect it to the websocket event.
    socket.on('message', function(event) {
        transport.onmessage(event);
    });
});

socket.on('error', function (error) {
    console.log('Connection error: ' + error.message);
    process.exit(1);
});

socket.on('close', function () {
    console.log('Connection closed.');
    process.exit(1);
});
