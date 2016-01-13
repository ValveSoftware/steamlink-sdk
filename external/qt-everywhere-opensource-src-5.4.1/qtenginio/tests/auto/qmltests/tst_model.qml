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

    function replyDumpDebugInfo(reply) {
        console.log("\n\n### ERROR")
        console.log(reply.errorString)
        reply.dumpDebugInfo()
        console.log("\n###\n")
    }

    TestCase {
        name: "EnginioModel: without client"
        EnginioModel {
            id: modelMissingClient
        }
        function test_append()
        {
            ignoreWarning("EnginioQmlModel::append(): Enginio client is not set")
            compare(modelMissingClient.append(null), null)
            ignoreWarning("EnginioQmlModel::append(): Enginio client is not set")
            compare(modelMissingClient.append({"objectType": "objects.todos", "title": "no go"}), null)
        }
        function test_remove()
        {
            ignoreWarning("EnginioQmlModel::remove(): Enginio client is not set")
            compare(modelMissingClient.remove(0), null)
        }
        function test_setProperty()
        {
            ignoreWarning("EnginioQmlModel::setProperty(): Enginio client is not set")
            compare(modelMissingClient.setProperty(0, "title", "foo"), null)
        }
        function rowCount()
        {
            compare(rowCount(), 0)
        }
    }

    TestCase {
        name: "EnginioModel: invalid row number"

        EnginioModel {
            id: modelInvalidRow
            client: EnginioClient {
                serviceUrl: AppConfig.backendData.serviceUrl
                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: invalid row"}
                   }

            property int resetCounter: 0
            onModelReset: ++resetCounter
        }

        function test_remove()
        {
            tryCompare(modelInvalidRow, "resetCounter", 2)
            var reply = modelInvalidRow.remove(modelInvalidRow.rowCount)
            tryCompare(reply, "isError", true)
            verify(reply, "isError", true)
        }
        function test_setProperty()
        {
            tryCompare(modelInvalidRow, "resetCounter", 2)
            var reply = modelInvalidRow.setProperty(modelInvalidRow.rowCount, "title", "foo")
            tryCompare(reply, "isError", true)
            verify(reply, "isError", true)
        }
    }

    TestCase {
        name: "EnginioModel: create"

        EnginioClient {
            id: enginioClientCreate
            serviceUrl: AppConfig.backendData.serviceUrl

            property int errorCount: 0
            onError: {
                ++errorCount
                replyDumpDebugInfo(reply)
            }

            backendId: AppConfig.backendData.id
        }

        EnginioModel {
            id: modelCreate
        }

        SignalSpy {
            id: modelCreateEnginioChanged
            target: modelCreate
            signalName: "clientChanged"
        }

        SignalSpy {
            id: modelCreateQueryChanged
            target: modelCreate
            signalName: "queryChanged"
        }

        SignalSpy {
            id: modelCreateOperationChanged
            target: modelCreate
            signalName: "operationChanged"
        }

        function init() {
            modelCreate.client = null
        }

        function cleanupTestCase() {
            modelCreate.client = null
        }

        function test_assignClient() {
            var signalCount = modelCreateEnginioChanged.count
            modelCreate.client = enginioClientCreate
            verify(modelCreate.client == enginioClientCreate)
            tryCompare(modelCreateEnginioChanged, "count", ++signalCount)

            modelCreate.client = null
            verify(modelCreate.client === null)
            tryCompare(modelCreateEnginioChanged, "count", ++signalCount)

            modelCreate.client = enginioClientCreate
            verify(modelCreate.client == enginioClientCreate)
            tryCompare(modelCreateEnginioChanged, "count", ++signalCount)
        }

        function test_assignQuery() {
            var signalCount = modelCreateQueryChanged.count
            var query = {"objectType": "objects.todos"}
            var queryStr = JSON.stringify(query)
            modelCreate.query = query
            verify(JSON.stringify(modelCreate.query) == queryStr)
            tryCompare(modelCreateQueryChanged, "count", ++signalCount)

            modelCreate.query = {}
            verify(JSON.stringify(modelCreate.query) == "{}")
            tryCompare(modelCreateQueryChanged, "count", ++signalCount)

            modelCreate.query = query
            verify(JSON.stringify(modelCreate.query) == queryStr)
            tryCompare(modelCreateQueryChanged, "count", ++signalCount)
        }

        function test_assignOperation() {
            modelCreate.operation = Enginio.ObjectOperation
            verify(modelCreate.operation === Enginio.ObjectOperation)

            modelCreate.operation = Enginio.AccessControlOperation
            verify(modelCreate.operation === Enginio.AccessControlOperation)
        }
    }

    TestCase {
        name: "EnginioModel: query"

        EnginioModel {
            id: modelQuery
            client: EnginioClient {
                id: enginioClientQuery
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            onModelAboutToBeReset: { resetCount++ }
            property int resetCount: 0
        }

        function _query(query, operation) {
            var enginioClient = modelQuery.client
            modelQuery.client = null
            var count = modelQuery.resetCount
            modelQuery.operation = operation
            modelQuery.query = query
            // re-assigning the same object should not trigger the property change
            modelQuery.query = query
            modelQuery.client = enginioClient
            tryCompare(modelQuery, "resetCount", ++count, 10000)
        }

        function test_queryObjects() {
            var counterObject = { "counter" : 0, "enginioErrors" : enginioClientQuery.errorCount}
            enginioClientQuery.create({ "objectType": AppConfig.testObjectType,
                               "testCase": "EnginioModel: query",
                               "title": "prepare",
                               "count": 1,
                           }, Enginio.ObjectOperation).finished.connect(function(){ counterObject.counter++});
            enginioClientQuery.create({ "objectType": AppConfig.testObjectType,
                               "testCase": "EnginioModel: query",
                               "title": "prepare",
                               "count": 2,
                           }, Enginio.ObjectOperation).finished.connect(function(){ counterObject.counter++});

            tryCompare(counterObject, "counter", 2, 10000)

            _query({ "limit": 2, "objectType": AppConfig.testObjectType }, Enginio.ObjectOperation)
            compare(counterObject.enginioErrors, enginioClientQuery.errorCount)

        }

        function test_queryUsers() {
            _query({ "limit": 2, "objectType": "users" }, Enginio.UserOperation)
        }

        function test_queryUsersgroups() {
            _query({ "limit": 2, "objectType": "usersgroups" }, Enginio.UsergroupOperation)
        }
    }

    TestCase {
        name: "EnginioModel: modify"

        EnginioModel {
            id: modelModify
            client: EnginioClient {
                id: enginioClientModify
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: modify"}
                   }

            property int resetCounter: 0
            onModelReset: ++resetCounter
        }

        function test_modify() {
            var errorCount = enginioClientModify.errorCount
            var counterObject = {"counter": 0, "expectedCount": 0}
            tryCompare(modelModify, "resetCounter", 2)

            // append new data
            modelModify.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify",
                             "title": "test_modify",
                             "count": 42,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            modelModify.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify",
                             "title": "test_modify",
                             "count": 43,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            tryCompare(counterObject, "counter", counterObject.expectedCount)
            compare(enginioClientModify.errorCount, errorCount)


            // remove data
            modelModify.remove(0).finished.connect(function(reply) {counterObject.counter++})
            tryCompare(counterObject, "counter", ++counterObject.expectedCount, 10000)
            compare(enginioClientModify.errorCount, errorCount)


            // change data
            modelModify.setProperty(0, "count", 77).finished.connect(function() {counterObject.counter++})
            tryCompare(counterObject, "counter", ++counterObject.expectedCount)
            compare(enginioClientModify.errorCount, errorCount)
        }
    }

    TestCase {
        name: "EnginioModel: modify unblocked, wait for the initial reset"

        EnginioModel {
            id: modelModifyUndblocked
            client: EnginioClient {
                id: enginioClientModifyUndblocked
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: modify unblocked"}
                   }

            property int resetCounter: 0
            onModelReset: ++resetCounter
        }

        function test_modify() {
            var errorCount = enginioClientModifyUndblocked.errorCount
            var counterObject = {"counter": 0, "expectedCount": 0}
            tryCompare(modelModifyUndblocked, "resetCounter", 2)

            // append new data
            modelModifyUndblocked.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify unblocked",
                             "title": "test_modify",
                             "count": 42,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            modelModifyUndblocked.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify unblocked",
                             "title": "test_modify",
                             "count": 43,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount


            // remove data
            modelModifyUndblocked.remove(1).finished.connect(function(reply) {counterObject.counter++})
            ++counterObject.expectedCount


            // change data
            modelModifyUndblocked.setProperty(0, "count", 77).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            tryCompare(counterObject, "counter", counterObject.expectedCount)
            compare(enginioClientModifyUndblocked.errorCount, errorCount)
        }
    }


    TestCase {
        name: "EnginioModel: modify unblocked chaos"

        EnginioModel {
            id: modelModifyChaos
            client: EnginioClient {
                id: enginioClientModifyChaos
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: modify unblocked chaos"}
                   }

            property int resetCounter: 0
            onModelReset: ++resetCounter
        }

        function test_modify() {
            if (enginioClientModifyChaos.serviceUrl !== "https://staging.engin.io")
                skip("FIXME the test crashes on staging because of enabled notifications")
            var errorCount = enginioClientModifyChaos.errorCount
            var counterObject = {"counter": 0, "expectedCount": 0}

            // append new data
            modelModifyChaos.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify unblocked chaos",
                             "title": "test_modify",
                             "count": 42,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            modelModifyChaos.append({ "objectType": AppConfig.testObjectType,
                             "testCase": "EnginioModel: modify unblocked chaos",
                             "title": "test_modify",
                             "count": 43,
                         }).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount


            // remove data
            modelModifyChaos.remove(1).finished.connect(function(reply) {counterObject.counter++})
            ++counterObject.expectedCount


            // change data
            modelModifyChaos.setProperty(0, "count", 77).finished.connect(function() {counterObject.counter++})
            ++counterObject.expectedCount
            tryCompare(counterObject, "counter", counterObject.expectedCount)
            compare(enginioClientModifyChaos.errorCount, errorCount)
        }
    }


    TestCase {
        name: "EnginioModel: rowCount"

        EnginioModel {
            id: modelRowCount
            client: EnginioClient {
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: rowCount"}
                   }

            property int resetCounter: 0
            onModelReset: ++resetCounter

            property int rowCountChangedCounter: 0
            onRowCountChanged: ++rowCountChangedCounter
        }

        function test_rowCount()
        {
            tryCompare(modelRowCount, "resetCounter", 2)

            // append and remove
            compare(modelRowCount.rowCount, 0)

            var r0 = modelRowCount.append({"objectType": AppConfig.testObjectType, "testCase": "EnginioModel: rowCount"})
            tryCompare(r0, "isFinished", true)
            tryCompare(r0, "isError", false)

            var initialRowCount = modelRowCount.rowCount
            var initialRowCountChangedCounter = modelRowCount.rowCountChangedCounter
            var r1 = modelRowCount.append({"objectType": AppConfig.testObjectType, "testCase": "EnginioModel: rowCount"})
            tryCompare(r1, "isFinished", true)
            tryCompare(r1, "isError", false)
            tryCompare(modelRowCount, "rowCount", initialRowCount + 1)
            tryCompare(modelRowCount, "rowCountChangedCounter", initialRowCountChangedCounter + 1)

            var r2 = modelRowCount.remove(0)
            tryCompare(r2, "isFinished", true)
            tryCompare(r2, "isError", false)
            tryCompare(modelRowCount, "rowCount", initialRowCount)
            tryCompare(modelRowCount, "rowCountChangedCounter", initialRowCountChangedCounter + 2)

            // reset
            var query = modelRowCount.query;
            modelRowCount.query = undefined
            compare(modelRowCount.query, undefined)
            tryCompare(modelRowCount, "rowCount", 0)
            tryCompare(modelRowCount, "rowCountChangedCounter", initialRowCountChangedCounter + 3)
            modelRowCount.query = query
            tryCompare(modelRowCount, "rowCount", initialRowCount)
            tryCompare(modelRowCount, "rowCountChangedCounter", initialRowCountChangedCounter + 4)
        }
    }

    TestCase {
        name: "EnginioModel: reload"

        EnginioModel {
            id: model
            client: EnginioClient {
                id: client
                serviceUrl: AppConfig.backendData.serviceUrl

                property int errorCount: 0
                onError: {
                    ++errorCount
                    replyDumpDebugInfo(reply)
                }

                backendId: AppConfig.backendData.id
            }
            query: {
                     "objectType": AppConfig.testObjectType,
                     "query": {"testCase": "EnginioModel: rowCount"}
                   }
        }

        function test_rowCount()
        {
            var r = model.reload();
            tryCompare(r, "isFinished", true);
            compare(client.errorCount, 0)
        }
    }
}
