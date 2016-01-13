/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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
import Enginio 1.0
import "config.js" as AppConfig

Item {
    id: root

    EnginioClient {
        id: enginio
        backendId: AppConfig.backendData.id
        serviceUrl: AppConfig.backendData.serviceUrl

        property int errorCount: 0
        property int finishedCount: 0

        onError: {
            finishedCount += 1
            errorCount += 1
        }

        onFinished: {
            finishedCount += 1
        }
    }

    SignalSpy {
           id: finishedSpy
           target: enginio
           signalName: "finished"
    }

    SignalSpy {
           id: errorSpy
           target: enginio
           signalName: "error"
    }

    TestCase {
        name: "EnginioReply"

        function init() {
            finishedSpy.clear()
            errorSpy.clear()
        }

        function test_create_data() {
            // function to call, json, area, errorType, errorString, networkError, backendStatus
            return [
                        {
                            "op": "create",
                            "q": { "objectType": AppConfig.testObjectType,
                                "testCase" : "EnginioReply_ObjectOperation",
                                "name" : "FOOBAR"
                            },
                            "area": Enginio.ObjectOperation,
                            "isError": false,
                            "errorType": EnginioReply.NoError,
                            "errorString": "Unknown error",
                            "networkError": QNetworkReply.NoError,
                            "backendStatus": 201,
                        },
                        {
                            "op": "create",
                            "q": {
                                "testCase" : "EnginioReply_NoObjectType",
                                "name" : "FOOBAR"
                            },
                            "area": Enginio.ObjectOperation,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ContentNotFoundError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"Requested object operation requires non empty 'objectType' value\",\"reason\": \"BadRequest\"}]}",
                        },
                        {
                            "op": "query",
                            "q": { "objectType": AppConfig.testObjectType },
                            "area": Enginio.ObjectOperation,
                            "isError": false,
                            "errorType": EnginioReply.NoError,
                            "networkError": QNetworkReply.NoError,
                            "backendStatus": 200,
                            "errorString": "Unknown error",
                        },
                        {
                            "op": "query",
                            "q": {},
                            "area": Enginio.ObjectOperation,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ContentNotFoundError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"Requested object operation requires non empty 'objectType' value\",\"reason\": \"BadRequest\"}]}",
                        },
                        {
                            "op": "create",
                            "q": { "objectType": "users", "username": "no password" },
                            "area": Enginio.UserOperation,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ProtocolInvalidOperationError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"can't be blank\",\"property\": \"password\",\"reason\": \"ValidationFailed\"}]}",
                        },
                        {
                            "op": "create",
                            "q": { "objectType": "users", "username": "first user", "password": "pw" },
                            "area": Enginio.UserOperation,
                            "isError": false,
                            "errorType": EnginioReply.NoError,
                            "networkError": QNetworkReply.NoError,
                            "backendStatus": 201,
                            "errorString": "Unknown error",
                        },
                        {
                            "op": "create",
                            "q": { "objectType": "users", "username": "first user", "password": "pw" },
                            "area": Enginio.UserOperation,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ProtocolInvalidOperationError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"has already been taken\",\"property\": \"username\",\"reason\": \"ValidationFailed\"}]}",
                        },
                        {
                            "op": "fullTextSearch",
                            "q": { "objectTypes": "notAnArrayAndSearchMissing" },
                            "area": null,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ContentNotFoundError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"Fulltext Search: 'objectTypes' parameter is missing or it is not an array\",\"reason\": \"BadRequest\"}]}",
                        },
                        {
                            "op": "fullTextSearch",
                            "q": { "objectType": "notAnArrayAndSearchMissing" },
                            "area": null,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ContentNotFoundError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"Fulltext Search: 'objectTypes' parameter is missing or it is not an array\",\"reason\": \"BadRequest\"}]}",
                        },
                        {
                            "op": "fullTextSearch",
                            "q": { "objectTypes": ["searchIsMissing"] },
                            "area": null,
                            "isError": true,
                            "errorType": EnginioReply.BackendError,
                            "networkError": QNetworkReply.ContentNotFoundError,
                            "backendStatus": 400,
                            "errorString": "{\"errors\": [{\"message\": \"Fulltext Search: 'search' parameter(s) missing\",\"reason\": \"BadRequest\"}]}",
                        },
                    ]
        }

        function test_create(d) {
            var finished = 0;
            var reply = enginio[d.op](d.q, d.area);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, d.isError ? 1 : 0)

            verify(reply.isFinished)
            compare(reply.isError, d.isError)
            compare(reply.errorType, d.errorType)

            // console.log("data:", JSON.stringify(reply.data))

            compare(reply.networkError, d.networkError)
            compare(reply.backendStatus, d.backendStatus)

            if (d.errorString !== reply.errorString) {
                var expectedErrorObject = JSON.parse(d.errorString)
                var actualErrorObject = JSON.parse(reply.errorString)
                for (var prop in expectedErrorObject) {
                    compare(actualErrorObject[prop], expectedErrorObject[prop])
                }
            }
        }
    }
}
