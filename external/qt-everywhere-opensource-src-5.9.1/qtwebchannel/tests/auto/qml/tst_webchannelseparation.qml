/****************************************************************************
**
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
    name: "WebChannelSeparation"

    Client {
        id: client1
        objectName: "client1"
    }
    Client {
        id: client2
        objectName: "client2"
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
    QtObject {
        id: myObj2
        function myMethod()
        {
            // return a javascript object which is handled as an Object, but not a QObject
            return { "obj1": { "name": "n1", "value": 1 }, "obj2" : {"name": "n2", "value": 2} };
        }

        WebChannel.id: "myObj2"
    }

    QtObject {
        id: myObj3

        // always returns the same object
        function getObject()
        {
            return myObj;
        }
        WebChannel.id: "myObj3"
    }

    property var lastFactoryObj
    property var createdFactoryObjects: []
    QtObject {
        id: myFactory

        function cleanup()
        {
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
            function myMethod(arg)
            {
                lastMethodArg = arg;
            }
            signal mySignal(var arg1, var arg2)
        }
    }

    TestWebChannel {
        id: webChannel
        transports: [client1.serverTransport, client2.serverTransport]
        registeredObjects: [myObj, myOtherObj, myObj2, myObj3, myFactory]
    }

    function init()
    {
        myObj.myProperty = 1
        client1.cleanup();
        client2.cleanup();
    }

    function cleanup()
    {
        client1.debug = false;
        client2.debug = false;
        // delete all created objects
        myFactory.cleanup();
        lastFactoryObj = undefined;
        createdFactoryObjects = [];
        // reschedule current task to end of event loop
        wait(1);
    }

    function test_signalSeparation()
    {
        var testObj1;
        var testObj1Id;
        var testObj2;
        var testObj2Id;

        var channel1 = client1.createChannel(function (channel1) {
            channel1.objects.myFactory.create("testObj1", function (obj1) {
                testObj1 = lastFactoryObj;
                testObj1Id = obj1.__id__;

                obj1.mySignal.connect(function (arg1_1, arg1_2) {
                     console.debug("client 1 received signal 'mySignal' " + arg1_1);
                });

                // create second channel after factory has created first
                // object to make sure that a dynamically created object
                // exists but does not get exposed to new channels
                createSecondChannel();
            });
        });

        var channel2;
        function createSecondChannel()
        {
            // dismiss all messges received before channel creation
            client2.cleanup();

            channel2 = client2.createChannel(function (channel2) {
                channel2.objects.myFactory.create("testObj2", function (obj2) {
                    testObj2 = lastFactoryObj;
                    testObj2Id = obj2.__id__;
                    obj2.mySignal.connect(function (arg2_1, arg2_2) {
                         console.debug("client 2 received signal 'mySignal'");
                    });
                });
            });
        }

        client1.awaitInit();
        client1.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        client2.awaitInit();
        client2.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        // dismiss server messages
        client1.serverMessages = [];
        client2.serverMessages = [];

        // now everything is set-up
        // and we can kick off a signal
        testObj1.mySignal("foo", "bar");

        var msg1 = client1.awaitSignal();
        compare(msg1.signal, 6);

        // look if there is a signal send to client2, which should not happen
        var msg2 = client2.skipToMessage(1, "server", 10);
        console.log("client2 received a signal. let's check that it does not come from testObj1");
        if (msg2 !== false) {
            verify(msg2.object !== testObj1Id);
        }
    }

    function test_separationForSameObject()
    {
        var testObj1;
        var testObj2;
        var receivedSignal1 = false;
        var receivedSignal2 = false;

        var channel2;
        var channel1 = client1.createChannel(function (channel1) {
            channel1.objects.myObj3.getObject(function (obj) {
                testObj1 = obj;

                testObj1.mySignal.connect(function() {
                    receivedSignal1 = true;
                });

                // create second channel after factory has created first
                // object to make sure that a dynamically created object
                // exists but does not get exposed to new channels
                createSecondChannel();
            });
        });

        function createSecondChannel()
        {
            // dismiss all messges received before channel creation
            client2.cleanup();

            channel2 = client2.createChannel(function (channel2) {
                verify(channel2.objects.myObj2)
                channel2.objects.myObj3.getObject(function (obj) {
                    testObj2 = obj;
                    testObj2.mySignal.connect(function() {
                        receivedSignal2 = true;
                    });

                });
            });
        }

        client1.awaitInit();
        client1.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        client2.awaitInit();
        client2.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        // trigger signal, signal should be received by both channels
        myObj.mySignal("foo", "bar");

        verify(receivedSignal1, "Channel 1 missed signal")
        verify(receivedSignal2, "Channel 2 missed signal")
    }

    function test_propertyUpdateSeparation()
    {
        var testObj1;
        var testObj1Id;
        var testObj2;
        var testObj2Id;

        var channel1 = client1.createChannel(function (channel1) {
            channel1.objects.myFactory.create("testObj1", function (obj1) {
                testObj1 = lastFactoryObj;
                testObj1Id = obj1.__id__;

                obj1.myPropertyChanged.connect(function (arg1_1) {
                     console.debug("client 1 received property update 'myProperty' " + obj1.myProperty);
                });

                // create second channel after factory has created first
                // object to make sure that a dynamically created object
                // exists but does not get exposed to new channels
                createSecondChannel();
            });
        });

        var channel2;
        function createSecondChannel()
        {
            // dismiss all messges received before channel creation
            client2.cleanup();

            channel2 = client2.createChannel(function (channel2) {
                channel2.objects.myFactory.create("testObj2", function (obj2) {
                    testObj2 = lastFactoryObj;
                    testObj2Id = obj2.__id__;
                    obj2.myPropertyChanged.connect(function (arg1_1) {
                         console.debug("client 2 received property update 'myProperty' " + obj2.myProperty);
                    });
                });
            });
        }

        client1.awaitInit();
        client1.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        client2.awaitInit();
        client2.skipToMessage(JSClient.QWebChannelMessageTypes.idle);

        // dismiss server messages
        client1.serverMessages = [];
        client2.serverMessages = [];

        // now everything is set-up
        // and we can kick off a property change
        testObj1.myProperty = 5;

        var msg1 = client1.awaitPropertyUpdate();
        compare(msg1.type, JSClient.QWebChannelMessageTypes.propertyUpdate);

        //look if there is a propertyUpdate sent to client2, which should not happen
        var msg2 = client2.skipToMessage(2, "server", 10);
        console.log("client2 received a propertyUpdate. let's check that it does not come from testObj1");
        if (msg2 !== false) {
            verify(msg2.object !== testObj1Id);
        }
    }

    function test_returnNonQObject()
    {
        var retObj;

        var channel = client1.createChannel(function (channel) {
            channel.objects.myObj2.myMethod(function(result) {
                retObj = result;
            });
        });

        client1.awaitInit();

        var msg1 = client1.awaitMessage();
        compare(msg1.type, JSClient.QWebChannelMessageTypes.invokeMethod); // create

        msg1 = client1.awaitIdle();
        verify(retObj["obj1"]["name"]);
    }
}

