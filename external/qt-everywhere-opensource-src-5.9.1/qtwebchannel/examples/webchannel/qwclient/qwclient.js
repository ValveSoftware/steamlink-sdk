#!/usr/bin/env node
/****************************************************************************
**
** Copyright (C) 2016 basysKom GmbH, author Sumedha Widyadharma <sumedha.widyadharma@basyskom.com>
** Copyright (C) 2016 basysKom GmbH, author Lutz Schönemann <lutz.schoenemann@basyskom.com>
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
var repl = require('repl');
var WebSocket = require('faye-websocket').Client;
var QWebChannel = new require('./qwebchannel.js').QWebChannel;

var serverAddress = 'ws://localhost:12345';
var channels = [];

var autoConnect = process.argv.pop();
if (autoConnect === __filename) {
    autoConnect = false;
}

var openChannel = function (address) {
    // this should be bound to the repl
    var self = this;
    address = address ? address : serverAddress;
    if (address.indexOf('://') === -1) {
        address = 'ws://' + address;
    }

    var ws = new WebSocket(address);

    ws.on('open', function (event) {
        var transport = {
        onmessage: function (data) {},
          send: function (data) {
              ws.send(data, {binary: false});
          }
        };
        ws.on('message', function (event) {
          transport.onmessage(event);
        }); // onmessage

        var webChannel = new QWebChannel(transport, function (channel) {
            channels.push(channel);
            var channelIdx = (channels.length - 1);
            console.log('channel opened', channelIdx);
            // Create a nice alias to access this channels objects
            self.context['c' + channelIdx] = channel.objects;

            ws.on('close', function () {
                for (var i = 0; i < channels.length; ++i) {
                    if (channels[i] === channel) {
                        console.log('channel closed', i);
                        channels[i] = null;
                        return;
                    }
                }
            }); // onclose
        }); // new QWebChannel
    }); // onopen

    ws.on('error', function (error) {
        console.log('websocket error', error.message);
    });
}; // openChannel

var setupRepl = function() {
    var r = repl.start({
        prompt: "webchannel> ",
        input: process.stdin,
        output: process.stdout
    });

    r.context.serverAddress = serverAddress;
    r.context.openChannel = openChannel.bind(r);
    r.context.channels = channels;

    r.context.lsObjects = function() {
        channels.forEach(function(channel){
            console.log('Channel ' + channel);
            Object.keys(channel.objects);
        });
    }
    return r;
}

var welcome = function() {
    console.log('Welcome to the qwebchannel/websocket REPL.');
    console.log('Use openChannel(url) to connect to a service.');
    console.log('For the standalone example, just openChannel() should suffice.');
    console.log('Opened channels have their objects aliased to c<channel number>, i.e. c0');
    console.log('So for the standalone example try: c0.dialog.receiveText(\'hello world\')');
}

welcome();
var repl = setupRepl();

if (autoConnect) {
    repl.context.openChannel(autoConnect);
}
