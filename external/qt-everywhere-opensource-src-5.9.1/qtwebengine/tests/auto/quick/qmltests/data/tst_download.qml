/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
import QtWebEngine 1.5

TestWebEngineView {
    id: webEngineView
    width: 200
    height: 200
    profile: testDownloadProfile

    property int totalBytes: 0
    property int receivedBytes: 0
    property bool cancelDownload: false
    property var downloadState: []
    property var downloadInterruptReason: null

    SignalSpy {
        id: downLoadRequestedSpy
        target: testDownloadProfile
        signalName: "downloadRequested"
    }

    SignalSpy {
        id: downloadFinishedSpy
        target: testDownloadProfile
        signalName: "downloadFinished"
    }

    Connections {
        id: downloadItemConnections
        ignoreUnknownSignals: true
        onStateChanged: downloadState.push(target.state)
        onInterruptReasonChanged: downloadInterruptReason = target.interruptReason
    }

    WebEngineProfile {
        id: testDownloadProfile

        onDownloadRequested: {
            downloadState.push(download.state)
            downloadItemConnections.target = download
            if (cancelDownload) {
                download.cancel()
            } else {
                totalBytes = download.totalBytes
                download.path = "testfile.zip"
                download.accept()
            }
        }
        onDownloadFinished: {
            receivedBytes = download.receivedBytes;
        }
    }

    TestCase {
        name: "WebEngineViewDownload"

        function init() {
            downLoadRequestedSpy.clear()
            downloadFinishedSpy.clear()
            totalBytes = 0
            receivedBytes = 0
            cancelDownload = false
            downloadItemConnections.target = null
            downloadState = []
            downloadInterruptReason = null
        }

        function test_downloadRequest() {
            compare(downLoadRequestedSpy.count, 0)
            webEngineView.url = Qt.resolvedUrl("download.zip")
            downLoadRequestedSpy.wait()
            compare(downLoadRequestedSpy.count, 1)
            compare(downloadState[0], WebEngineDownloadItem.DownloadRequested)
            verify(!downloadInterruptReason)
        }

        function test_totalFileLength() {
            compare(downLoadRequestedSpy.count, 0)
            webEngineView.url = Qt.resolvedUrl("download.zip")
            downLoadRequestedSpy.wait()
            compare(downLoadRequestedSpy.count, 1)
            compare(totalBytes, 325)
            verify(!downloadInterruptReason)
        }

        function test_downloadSucceeded() {
            compare(downLoadRequestedSpy.count, 0)
            webEngineView.url = Qt.resolvedUrl("download.zip")
            downLoadRequestedSpy.wait()
            compare(downLoadRequestedSpy.count, 1)
            compare(downloadState[0], WebEngineDownloadItem.DownloadRequested)
            tryCompare(downloadState, "1", WebEngineDownloadItem.DownloadInProgress)
            downloadFinishedSpy.wait()
            compare(totalBytes, receivedBytes)
            tryCompare(downloadState, "2", WebEngineDownloadItem.DownloadCompleted)
            verify(!downloadInterruptReason)
        }

        function test_downloadCancelled() {
            compare(downLoadRequestedSpy.count, 0)
            cancelDownload = true
            webEngineView.url = Qt.resolvedUrl("download.zip")
            downLoadRequestedSpy.wait()
            compare(downLoadRequestedSpy.count, 1)
            compare(downloadFinishedSpy.count, 1)
            tryCompare(downloadState, "1", WebEngineDownloadItem.DownloadCancelled)
            tryCompare(webEngineView, "downloadInterruptReason", WebEngineDownloadItem.UserCanceled)
        }
    }
}
