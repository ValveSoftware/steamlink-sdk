/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
import QtWebEngine 1.2

TestWebEngineView {
    id: webEngineView
    width: 400
    height: 300

    SignalSpy {
        id: titleSpy
        target: webEngineView
        signalName: "titleChanged"
    }

    TestCase {
        name: "WebEngineViewKeyboardModifierMapping"

        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function getPressedModifiers() {
            var pressedModifiers;
            runJavaScript("getPressedModifiers()", function(result) {
                pressedModifiers = result;
            });
            tryVerify(function() { return pressedModifiers != undefined });
            return pressedModifiers;
        }

        function test_keyboardModifierMapping() {
            webEngineView.url = Qt.resolvedUrl("keyboardModifierMapping.html")
            waitForLoadSucceeded();
            titleSpy.wait()

            // Alt
            keyPress(Qt.Key_Alt);
            titleSpy.wait()
            compare(getPressedModifiers(), "alt:pressed ctrl:no meta:no");
            keyRelease(Qt.Key_Alt)
            titleSpy.wait()

            // Ctrl
            // On mac Qt automatically translates Meta to Ctrl and vice versa.
            // However, if sending the events manually no mapping is being done,
            // so we have to do this here manually.
            // For testing we assume that the flag Qt::AA_MacDontSwapCtrlAndMeta is NOT set.
            keyPress(Qt.platform.os == "osx" ? Qt.Key_Meta : Qt.Key_Control);
            titleSpy.wait()
            compare(getPressedModifiers(), "alt:released ctrl:pressed meta:no");
            keyRelease(Qt.platform.os == "osx" ? Qt.Key_Meta : Qt.Key_Control);
            titleSpy.wait()

            // Meta (Command on Mac)
            keyPress(Qt.platform.os == "osx" ? Qt.Key_Control : Qt.Key_Meta);
            titleSpy.wait()
            compare(getPressedModifiers(), "alt:released ctrl:released meta:pressed");
            keyRelease(Qt.platform.os == "osx" ? Qt.Key_Control : Qt.Key_Meta);
            titleSpy.wait()

            compare(getPressedModifiers(), "alt:released ctrl:released meta:released");
        }
    }
}
