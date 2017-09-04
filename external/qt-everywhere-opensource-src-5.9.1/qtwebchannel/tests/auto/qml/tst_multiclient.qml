/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Copyright (C) 2016 basysKom GmbH, info@basyskom.com, author Lutz Schönemann <lutz.schoenemann@basyskom.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0
import QtTest 1.0

import QtWebChannel 1.0
import QtWebChannel.Tests 1.0

import "qrc:///qtwebchannel/qwebchannel.js" as JSClient

TestCase {
    name: "MultiClient"

    Client {
        id: client1
    }

    Client {
        id: client2
    }

    property int bar: 0
    QtObject {
        id: foo

        signal ping()

        function pong()
        {
            return ++bar;
        }

        WebChannel.id: "foo"
    }

    property var lastMethodArg
    QtObject {
        id: myObj
        property int myProperty: 1

        signal mySignal(var arg)

        function myMethod(arg)
        {
            lastMethodArg = arg;
        }

        WebChannel.id: "myObj"
    }

    QtObject {
        id: myOtherObj
        property var foo: 1
        property var bar: 1
        WebChannel.id: "myOtherObj"
    }

    property var lastFactoryObj
    property var createdFactoryObjects: []
    QtObject {
        id: myFactory

        function cleanup() {
            while (createdFactoryObjects.length) {
                var obj = createdFactoryObjects.shift();
                if (obj) {
                    obj.destroy();
                }
            }
        }

        function create(id)
        {
            lastFactoryObj = component.createObject(myFactory, {objectName: id});
            createdFactoryObjects.push(lastFactoryObj);
            return lastFactoryObj;
        }
        WebChannel.id: "myFactory"
    }

    Component {
        id: component
        QtObject {
            property var myProperty : 0
            function myMethod(arg) {
                lastMethodArg = arg;
            }
            signal mySignal(var arg1, var arg2)
        }
    }

    TestWebChannel {
        id: webChannel
        transports: [client1.serverTransport, client2.serverTransport]
        registeredObjects: [foo, myObj, myOtherObj, myFactory]
    }

    function init()
    {
        myObj.myProperty = 1
        client1.cleanup();
        client2.cleanup();
    }

    function cleanup() {
        client1.debug = false;
        client2.debug = false;
        // delete all created objects
        myFactory.cleanup();
        lastFactoryObj = undefined;
        createdFactoryObjects = [];
        // reschedule current task to end of event loop
        wait(1);
    }

    function clientInitCallback(channel)
    {
        channel.objects.foo.ping.connect(function() {
            channel.objects.foo.pong(function(value) {
                channel.pongAnswer = value;
            });
        });
    }

    function test_multiclient()
    {
        var c1 = client1.createChannel(clientInitCallback);
        var c2 = client2.createChannel(clientInitCallback);

        // init, connect & idle messages for two clients
        for (var i = 0; i < 3; ++i) {
            client1.awaitMessage();
            client2.awaitMessage();
        }

        foo.ping();

        // invoke of pong method
        client1.awaitMessage();
        client2.awaitMessage();

        compare(c1.pongAnswer, 1);
        compare(c2.pongAnswer, 2);
    }

    function test_autowrappedObjectsNotInInit() {
        var testObj1;
        var testObj1Id;

        var channel1 = client1.createChannel(function (channel1) {
            channel1.objects.myFactory.create("testObj1", function (obj1) {
                testObj1 = lastFactoryObj;
                testObj1Id = obj1.__id__;

                // create second channel after factory has created first
                // object to make sure that a dynamically created object
                // exists but does not get exposed to new channels
                createSecondChannel();
            });
        });
        var channel2;
        function createSecondChannel() {
            // dismiss all messges received before channel creation
            client2.cleanup();

            channel2 = client2.createChannel(function (channel2) {
            });
        }

        client1.awaitInit();
        var msg1 = client1.awaitMessage();
        compare(msg1.type, JSClient.QWebChannelMessageTypes.invokeMethod); // create

        client1.awaitIdle();

        client2.awaitInit();
        client2.awaitIdle();

        compare(typeof channel2.objects[testObj1Id], "undefined")
    }

    function test_autowrappedObjectsNotBroadcasted() {
        var testObj2;
        var testObj2Id;

        var channel1 = client1.createChannel(function (channel1) {
            // create second channel after first channel to make sure
            // that a dynamically created object do not get exposed to
            // existing channels
            createSecondChannel();
        });
        var channel2;
        function createSecondChannel() {
            // dismiss all messges received before channel creation
            client2.cleanup();

            channel2 = client2.createChannel(function (channel2) {
                channel2.objects.myFactory.create("testObj2", function (obj2) {
                    testObj2 = lastFactoryObj;
                    testObj2Id = obj2.__id__;
                });
            });
        }

        client1.awaitInit();
        client1.awaitIdle();

        client2.awaitInit();
        var msg2 = client2.awaitMessage();
        compare(msg2.type, JSClient.QWebChannelMessageTypes.invokeMethod); // create

        client2.awaitIdle();

        compare(typeof channel1.objects[testObj2Id], "undefined")
    }
}
