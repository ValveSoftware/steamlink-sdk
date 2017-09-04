/****************************************************************************
**
** Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
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
    name: "WebChannel"

    Client {
        id: client
    }

    property var lastMethodArg

    QtObject {
        id: myObj
        property int myProperty: 1

        signal mySignal(var arg, QtObject object)

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
    QtObject{ id: bar; objectName: "bar" }
    QtObject{ id: baz; objectName: "baz" }
    QtObject {
        id: myFactory
        function create(id)
        {
            lastFactoryObj = component.createObject(myFactory, {objectName: id});
            return lastFactoryObj;
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

    TestObject {
        id: testObject
        WebChannel.id: "testObject"
    }

    TestWebChannel {
        id: webChannel
        transports: [client.serverTransport]
        registeredObjects: [myObj, myOtherObj, myFactory, testObject]
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
        var signalReceivedObject;
        var channel = client.createChannel(function(channel) {
            channel.objects.myObj.mySignal.connect(function(arg, object) {
                signalReceivedArg = arg;
                signalReceivedObject = object;
            });
        });
        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, "myObj");

        client.awaitIdle(); // initialization

        myObj.mySignal("test", myObj);

        compare(signalReceivedArg, "test");
        compare(signalReceivedObject.__id__, "myObj");

        var newObj = myFactory.create("newObj");
        myObj.mySignal(newObj, newObj);

        compare(signalReceivedArg.objectName, newObj.objectName);
        compare(signalReceivedObject.objectName, newObj.objectName);
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
        client.awaitResponse();

        // create testObj
        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, "myFactory");
        client.awaitResponse();
        verify(lastFactoryObj);
        compare(lastFactoryObj.objectName, "testObj");
        compare(channel.objects[testObjId].objectName, "testObj");

        // mySignal connection
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, testObjId);

        // set myProperty
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.setProperty);
        compare(msg.object, testObjId);
        compare(lastFactoryObj.myProperty, 42);

        // call myMethod
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, testObjId);
        compare(msg.args, ["foobar"]);
        client.awaitResponse();
        compare(lastMethodArg, "foobar");

        client.awaitIdle();

        // the server should eventually notify the client about the property update
        client.awaitPropertyUpdate();

        client.awaitIdle();

        // trigger a signal and ensure it gets transmitted
        lastFactoryObj.mySignal("foobar", 42);
        client.awaitSignal();

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
        client.awaitResponse();

        // now the signalArgs should also be set
        compare(signalArgs, {"0": "foobar", "1": 42});

        // and also a destroyed signal
        client.awaitSignal();

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
        client.awaitResponse();

        // call to myFactory.create()
        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        client.awaitResponse();

        client.awaitIdle();

        verify(testObj);
        var testObjId = testObj.__id__;

        testObj.deleteLater();
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        compare(msg.object, testObjId);
        client.awaitResponse();
        // destroyed signal
        client.awaitSignal();

        compare(lastFactoryObj, null);
        compare(typeof channel.objects[testObjId], "undefined");
    }

    function test_wrapper_propertyUpdateOfWrappedObjects()
    {
        var testObj;
        var testObjId;
        var channel = client.createChannel(function(channel) {
            channel.objects.myFactory.create("testObj", function(obj) {
                testObj = lastFactoryObj;
                testObjId = obj.__id__;
            });
        });
        client.awaitInit();
        client.awaitResponse();

        // call to myFactory.create()
        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        client.awaitResponse();

        client.awaitIdle();

        testObj.myProperty = 42;
        client.awaitPropertyUpdate();
        client.awaitIdle();
        compare(channel.objects[testObjId].myProperty, 42);

        channel.objects[testObjId].deleteLater();
        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        client.awaitResponse();
        // destroyed signal
        client.awaitSignal();
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

        myObj.mySignal(42, myObj);
        compare(signalArg, 42);

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.disconnectFromSignal);
        compare(msg.object, "myObj");

        myObj.mySignal(0, myObj);
        compare(signalArg, 42);
    }

    // see also: https://bugreports.qt.io/browse/QTBUG-54074
    function test_signalArgumentTypeConversion()
    {
        var signalArgs = [];
        function logSignalArgs(arg) {
            signalArgs.push(arg);
        }
        var channel = client.createChannel(function(channel) {
            var testObject = channel.objects.testObject;
            testObject.testSignalBool.connect(logSignalArgs);
            testObject.testSignalInt.connect(logSignalArgs);
            testObject.triggerSignals();
        });
        client.awaitInit();

        var msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, "testObject");

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.connectToSignal);
        compare(msg.object, "testObject");

        msg = client.awaitMessage();
        compare(msg.type, JSClient.QWebChannelMessageTypes.invokeMethod);
        client.awaitIdle();

        compare(signalArgs, [
            true,
            false,
            42,
            1,
            0
        ]);
    }
}
