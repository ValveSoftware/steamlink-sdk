/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtTest 1.0
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1
import "handwriting.js" as Handwriting
import "utils.js" as Utils

HandwritingInputPanel {
    id: handwritingInputPanel
    z: 99

    property var testcase
    readonly property var wordCandidatePopupList: Utils.findChildByProperty(handwritingInputPanel, "objectName", "wordCandidatePopupList", null)

    anchors.fill: parent

    signal inputMethodResult(var text)

    Connections {
        target: InputContext
        onPreeditTextChanged: if (InputContext.preeditText.length > 0) inputMethodResult(InputContext.preeditText)
    }

    Connections {
        target: InputContext.inputEngine
        onVirtualKeyClicked: inputMethodResult(text)
    }

    SignalSpy {
        id: inputMethodResultSpy
        target: handwritingInputPanel
        signalName: "inputMethodResult"
    }

    SignalSpy {
        id: wordCandidateListChangedSpy
        target: wordCandidatePopupList.model
        signalName: "dataChanged"
    }

    function wordCandidatePopupListSearchSuggestion(suggestion, timeout) {
        if (wordCandidatePopupList.visible === false)
            return false

        if (timeout !== undefined && timeout > 0) {
            // Note: Not using SignalSpy.wait() since it causes the test case to fail in case the signal is not emitted
            wordCandidateListChangedSpy.clear()
            var dt = new Date()
            var startTime = dt.getTime()
            while (wordCandidateListChangedSpy.count == 0) {
                dt = new Date()
                var elapsedTime = dt.getTime() - startTime
                if (elapsedTime >= timeout)
                    break
                var maxWait = Math.min(timeout - elapsedTime, 50)
                testcase.wait(maxWait)
            }
        }

        var suggestionFound = false
        var origIndex = handwritingInputPanel.wordCandidatePopupList.currentIndex
        if (origIndex !== -1) {
            while (true) {
                if (handwritingInputPanel.wordCandidatePopupList.model.dataAt(handwritingInputPanel.wordCandidatePopupList.currentIndex) === suggestion) {
                    suggestionFound = true
                    break
                }
                if (handwritingInputPanel.wordCandidatePopupList.currentIndex === handwritingInputPanel.wordCandidatePopupList.count - 1)
                    break
                handwritingInputPanel.wordCandidatePopupList.incrementCurrentIndex()
            }
            if (!suggestionFound) {
                while (handwritingInputPanel.wordCandidatePopupList.currentIndex !== origIndex) {
                    handwritingInputPanel.wordCandidatePopupList.decrementCurrentIndex()
                }
            }
            testcase.waitForRendering(handwritingInputPanel)
        }
        return suggestionFound
    }

    function wordCandidatePopupListSelectCurrentItem() {
        if (!handwritingInputPanel.wordCandidatePopupList.currentItem)
            return false
        testcase.wait(200)
        var itemPos = handwritingInputPanel.mapFromItem(handwritingInputPanel.wordCandidatePopupList.currentItem,
                                                        handwritingInputPanel.wordCandidatePopupList.currentItem.width / 2,
                                                        handwritingInputPanel.wordCandidatePopupList.currentItem.height / 2)
        testcase.mouseClick(handwritingInputPanel, itemPos.x, itemPos.y, Qt.LeftButton, 0, 20)
        testcase.waitForRendering(handwritingInputPanel)
        return true
    }

    function emulateHandwriting(ch, instant) {
        if (!available)
            return false
        active = true
        var hwrInputArea = Utils.findChildByProperty(handwritingInputPanel, "objectName", "hwrInputArea", null)
        inputMethodResultSpy.clear()
        if (!Handwriting.emulate(testcase, hwrInputArea, ch, instant)) {
            console.warn("Cannot produce the symbol '%1' in full screen handwriting mode".arg(ch))
            return false
        }
        inputMethodResultSpy.wait(3000)
        return inputMethodResultSpy.count > 0
    }
}
