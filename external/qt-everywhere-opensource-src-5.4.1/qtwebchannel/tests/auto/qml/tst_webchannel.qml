/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebChannel module of the Qt Toolkit.
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

import QtQuick 2.0
import QtTest 1.0

import QtWebChannel 1.0
import QtWebChannel.Tests 1.0
import "qrc:///qtwebchannel/qwebchannel.js" as JSClient

TestCase {
    name: "WebChannel"

    Client {
        id: client
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
    QtObject{ id: bar; objectName: "bar" }
    QtObject{ id: baz; objectName: "baz" }
    QtObject {
        id: myFactory
        property var lastObj
        function create(id)
        {
            lastObj = component.createObject(myFactory, {objectName: id});
            return lastObj;
        }
        property var objectInProperty: QtObject {
            objectName: "foo"
        }
        property var otherObject: myObj
        property var objects: [ bar, baz ];
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
        transports: [client.serverTransport]
        registeredObjects: [myObj, myOtherObj, myFactory]
    }

    function initChannel() {
        client.serverTransport.receiveMessage(JSON.stringify({type: JSClient.QWebChannelMessageTypes.idle}));
        webChannel.blockUpdates = true;
        webChannel.blockUpdates = false;
    }

    function init()
    {
        myObj.myProperty = 1
        // immediately send pending updates
        // to avoid property changed signals
        // during run of test case
        initChannel();
        client.cleanup();
    }
    function cleanup() {
        // make tests a bit more strict and predictable
        // by assuming that a test consumes all messages
        initChannel();
        compare(client.clientMessages.length, 0);
    }

    function test_property()
    {
        compare(myObj.myProperty, 1);

        var initialValue;
        var changedValue;

        var channel = client.createChannel(function(channel) {
            initialValue = channel.objects.myObj.myProperty;
            channel.objects.myObj.myPropertyChanged.connect(function() {
                changedValue = channel.objects.myObj.myProperty;
            });
            // now trigger a write from the client side
            channel.objects.myObj.myProperty = 3;
        });

        client.awaitInit();
        var msg = client.awaitMessage();

        compare(initialValue, 1);
        compare(myObj.myProperty, 3);

        client.awaitIdle(); // init
        client.awaitIdle(); // property update

        // change property, should be propagated to HTML client and a message be send there
        myObj.myProperty = 2;
        compare(myObj.myProperty, 2);
        client.awaitIdle(); // property update
        compare(changedValue, 2);
    }

    function test_method()
    {
        var channel = client.createChannel(function (channel) {
            channel.objects.myObj.myMethod("test");
        });

        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, "myObj");
        compare(msg.args, ["test"]);

        compare(lastMethodArg, "test")

        client.awaitIdle();
    }

    function test_signal()
    {
        var signalReceivedArg;
        var channel = client.createChannel(function(channel) {
            channel.objects.myObj.mySignal.connect(function(arg) {
                signalReceivedArg = arg;
            });
        });
        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, "myObj");

        client.awaitIdle(); // initialization

        myObj.mySignal("test");

        compare(signalReceivedArg, "test");
    }

    function test_grouping()
    {
        var receivedPropertyUpdates = 0;
        var properties = 0;
        var channel = client.createChannel(function(channel) {
            var originalHandler = channel.handlePropertyUpdate;
            channel.handlePropertyUpdate = function(message) {
                originalHandler(message);
                receivedPropertyUpdates++;
                properties = [channel.objects.myObj.myProperty, channel.objects.myOtherObj.foo, channel.objects.myOtherObj.bar];
            };
        });
        client.awaitInit();
        client.awaitIdle();

        // change properties a lot, we expect this to be grouped into a single update notification
        for (var i = 0; i < 10; ++i) {
            myObj.myProperty = i;
            myOtherObj.foo = i;
            myOtherObj.bar = i;
        }

        client.awaitIdle();
        compare(receivedPropertyUpdates, 1);
        compare(properties, [myObj.myProperty, myOtherObj.foo, myOtherObj.bar]);
        verify(!client.awaitMessage());
    }

    function test_wrapper()
    {
        var signalArgs;
        var testObjBeforeDeletion;
        var testObjAfterDeletion;
        var testObjId;
        var channel = client.createChannel(function(channel) {
            channel.objects.myFactory.create("testObj", function(obj) {
                testObjId = obj.__id__;
                compare(channel.objects[testObjId], obj);
                obj.mySignal.connect(function() {
                    signalArgs = arguments;
                    testObjBeforeDeletion = obj;
                    obj.deleteLater();
                    testObjAfterDeletion = obj;
                });
                obj.myProperty = 42;
                obj.myMethod("foobar");
            });
        });
        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, "myFactory");
        verify(myFactory.lastObj);
        compare(myFactory.lastObj.objectName, "testObj");
        compare(channel.objects[testObjId].objectName, "testObj");

        // mySignal connection
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, testObjId);

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.setProperty);
        compare(msg.object, testObjId);
        compare(myFactory.lastObj.myProperty, 42);

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, testObjId);
        compare(msg.args, ["foobar"]);
        compare(lastMethodArg, "foobar");

        client.awaitIdle();

        myFactory.lastObj.mySignal("foobar", 42);

        // property should be wrapped
        compare(channel.objects.myFactory.objectInProperty.objectName, "foo");
        // list property as well
        compare(channel.objects.myFactory.objects.length, 2);
        compare(channel.objects.myFactory.objects[0].objectName, "bar");
        compare(channel.objects.myFactory.objects[1].objectName, "baz");
        // also works with properties that reference other registered objects
        compare(channel.objects.myFactory.otherObject, channel.objects.myObj);

        // deleteLater call
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, testObjId);

        compare(signalArgs, {"0": "foobar", "1": 42});

        client.awaitIdle(); // destroyed signal

        compare(JSON.stringify(testObjBeforeDeletion), JSON.stringify({}));
        compare(JSON.stringify(testObjAfterDeletion), JSON.stringify({}));
        compare(typeof channel.objects[testObjId], "undefined");
    }

    // test if returned QObjects get inserted into list of
    // objects even if no callback function is set
    function test_wrapper_wrapEveryQObject()
    {
        var testObj;
        var channel = client.createChannel(function(channel) {
            channel.objects.myFactory.create("testObj", function(obj) {
                testObj = obj;
            });
        });
        client.awaitInit();

        // ignore first message (call to myFactory.create())
        client.awaitMessage();
        client.awaitIdle();

        verify(testObj);
        var testObjId = testObj.__id__;

        testObj.deleteLater();
        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, testObjId);

        // after receiving the destroyed signal the client deletes
        // local objects and sends back a idle message
        client.awaitIdle();

        compare(myFactory.lastObj, null);
        compare(typeof channel.objects[testObjId], "undefined");
    }

    function test_wrapper_propertyUpdateOfWrappedObjects() {
        var testObj;
        var testObjId;
        var channel = client.createChannel(function(channel) {
            channel.objects.myFactory.create("testObj", function(obj) {
                testObj = myFactory.lastObj;
                testObjId = obj.__id__;
            });
        });
        client.awaitInit();

        // call to myFactory.create()
        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);

        client.awaitIdle();

        testObj.myProperty = 42;
        client.awaitIdle();
        compare(channel.objects[testObjId].myProperty, 42);

        channel.objects[testObjId].deleteLater();
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
    }

    function test_disconnect()
    {
        var signalArg;
        var channel = client.createChannel(function(channel) {
            channel.objects.myObj.mySignal.connect(function(arg) {
                signalArg = arg;
                channel.objects.myObj.mySignal.disconnect(this);
            });
        });
        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, "myObj");

        client.awaitIdle();

        myObj.mySignal(42);
        compare(signalArg, 42);

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.disconnectFromSignal);
        compare(msg.object, "myObj");

        myObj.mySignal(0);
        compare(signalArg, 42);
    }
}
