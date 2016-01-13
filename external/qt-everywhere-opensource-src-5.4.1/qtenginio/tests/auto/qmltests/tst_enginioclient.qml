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
            console.log("\n\n### ERROR")
            console.log(reply.errorString)
            reply.dumpDebugInfo()
            console.log("\n###\n")
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
        name: "EnginioClient: MostlyFakeReplies"

        EnginioClient {
            id: fake
            backendId: AppConfig.backendData.id
            serviceUrl: AppConfig.backendData.serviceUrl

            property int errorCount: 0
            onError: {
                    if (reply.errorType === Enginio.BackendError
                        && reply.data.errors[0].message.length > 0
                        && reply.data.errors[0].reason.length > 0
                        && reply.backendStatus === 400
                        && reply.isError
                        && reply.isFinished)
                    {
                            ++errorCount
                    } else
                        console.log(JSON.stringify(reply))
            }
        }

        function test_fakeReply() {
            var empty = {}
            var objectTypeOnly = {"objectType" : "objects.todos"}
            var replies = [ fake.query(empty, Enginio.ObjectOperation),
                            fake.query(empty, Enginio.AccessControlOperation),
                            fake.query(objectTypeOnly, Enginio.AccessControlOperation),
                            fake.query(empty, Enginio.UsergroupMembersOperation),

                            fake.update(empty, Enginio.ObjectOperation),
                            fake.update(empty, Enginio.AccessControlOperation),
                            fake.update(objectTypeOnly, Enginio.AccessControlOperation),
                            fake.update(empty, Enginio.UsergroupMembersOperation),

                            fake.remove(empty, Enginio.ObjectOperation),
                            fake.remove(empty, Enginio.AccessControlOperation),
                            fake.remove(objectTypeOnly, Enginio.AccessControlOperation),
                            fake.remove(empty, Enginio.UsergroupMembersOperation),

                            fake.fullTextSearch(empty),
                            fake.downloadUrl(empty),
                            fake.uploadFile(empty, "")]

            tryCompare(fake, "errorCount", replies.length)
        }
    }

    TestCase {
        name: "EnginioClient: ObjectOperation CRUD"

        function init() {
            finishedSpy.clear()
            errorSpy.clear()
        }

        function test_CRUD() {
            var finished = 0;
            var reply = enginio.create({ "objectType": AppConfig.testObjectType,
                                         "testCase" : "EnginioClient_ObjectOperation",
                                         "title" : "CREATE",
                                         "count" : 1337,
                                       }, Enginio.ObjectOperation);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)

            var objectId = reply.data.id

            compare(reply.data.title, "CREATE")
            compare(reply.data.count, 1337)

            reply = enginio.query({ "objectType": AppConfig.testObjectType,
                                    "query" : { "id" : objectId }
                                  }, Enginio.ObjectOperation);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)
            verify(reply.data.results !== undefined)
            compare(reply.data.results.length, 1)
            compare(reply.data.results[0].id, objectId)
            compare(reply.data.results[0].testCase, "EnginioClient_ObjectOperation")
            compare(reply.data.results[0].title, "CREATE")
            compare(reply.data.results[0].count, 1337)

            reply = enginio.update({ "objectType": AppConfig.testObjectType,
                                     "id" : objectId,
                                     "testCase" : "EnginioClient_ObjectOperation_Update",
                                     "title" : "UPDATE",
                                  }, Enginio.ObjectOperation);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)
            compare(reply.data.id, objectId)
            compare(reply.data.testCase, "EnginioClient_ObjectOperation_Update")
            compare(reply.data.title, "UPDATE")
            compare(reply.data.count, 1337)

            reply = enginio.remove({ "objectType": AppConfig.testObjectType,
                                       "id" : objectId
                                   }, Enginio.ObjectOperation);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)

            reply = enginio.query({ "objectType": AppConfig.testObjectType
                                  }, Enginio.ObjectOperation);

            finishedSpy.wait()
            compare(finishedSpy.count, ++finished)
            compare(errorSpy.count, 0)
            compare(reply.data.results.length, 0)
        }
    }
}
