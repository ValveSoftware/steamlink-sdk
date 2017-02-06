/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.2
import QtTest 1.0
import QtQuick.Controls 2.1

TestCase {
    id: testCase
    width: 200
    height: 200
    visible: true
    when: windowShown
    name: "DialogButtonBox"

    Component {
        id: buttonBox
        DialogButtonBox { }
    }

    Component {
        id: button
        Button { }
    }

    Component {
        id: signalSpy
        SignalSpy { }
    }

    function test_defaults() {
        var control = buttonBox.createObject(testCase)
        verify(control)
        compare(control.count, 0)
        verify(control.delegate)
        compare(control.standardButtons, 0)
        control.destroy()
    }

    function test_standardButtons() {
        var control = buttonBox.createObject(testCase)

        compare(control.count, 0)

        control.standardButtons = DialogButtonBox.Ok
        compare(control.count, 1)
        var okButton = control.itemAt(0)
        verify(okButton)
        compare(okButton.text.toUpperCase(), "OK")

        control.standardButtons = DialogButtonBox.Cancel
        compare(control.count, 1)
        var cancelButton = control.itemAt(0)
        verify(cancelButton)
        compare(cancelButton.text.toUpperCase(), "CANCEL")

        control.standardButtons = DialogButtonBox.Ok | DialogButtonBox.Cancel
        compare(control.count, 2)
        if (control.itemAt(0).text.toUpperCase() === "OK") {
            okButton = control.itemAt(0)
            cancelButton = control.itemAt(1)
        } else {
            okButton = control.itemAt(1)
            cancelButton = control.itemAt(0)
        }
        verify(okButton)
        verify(cancelButton)
        compare(okButton.text.toUpperCase(), "OK")
        compare(cancelButton.text.toUpperCase(), "CANCEL")
        compare(control.standardButton(DialogButtonBox.Ok), okButton)
        compare(control.standardButton(DialogButtonBox.Cancel), cancelButton)

        control.standardButtons = 0
        compare(control.count, 0)
        compare(control.standardButton(DialogButtonBox.Ok), null)
        compare(control.standardButton(DialogButtonBox.Cancel), null)

        control.destroy()
    }

    function test_attached() {
        var control = buttonBox.createObject(testCase)

        control.standardButtons = DialogButtonBox.Ok
        var okButton = control.itemAt(0)
        compare(okButton.DialogButtonBox.buttonBox, control)
        compare(okButton.DialogButtonBox.buttonRole, DialogButtonBox.AcceptRole)

        var saveButton = button.createObject(control, {text: "Save"})
        compare(saveButton.DialogButtonBox.buttonBox, control)
        compare(saveButton.DialogButtonBox.buttonRole, DialogButtonBox.InvalidRole)
        saveButton.DialogButtonBox.buttonRole = DialogButtonBox.AcceptRole
        compare(saveButton.DialogButtonBox.buttonRole, DialogButtonBox.AcceptRole)

        var closeButton = button.createObject(null, {text: "Save"})
        compare(closeButton.DialogButtonBox.buttonBox, null)
        compare(closeButton.DialogButtonBox.buttonRole, DialogButtonBox.InvalidRole)
        closeButton.DialogButtonBox.buttonRole = DialogButtonBox.DestructiveRole
        compare(closeButton.DialogButtonBox.buttonRole, DialogButtonBox.DestructiveRole)
        control.addItem(closeButton)
        compare(closeButton.DialogButtonBox.buttonBox, control)

        control.contentModel.clear()
        compare(okButton.DialogButtonBox.buttonBox, null)
        compare(saveButton.DialogButtonBox.buttonBox, null)
        compare(closeButton.DialogButtonBox.buttonBox, null)

        control.destroy()
    }

    function test_signals_data() {
        return [
            { tag: "Ok", standardButton: DialogButtonBox.Ok, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" },
            { tag: "Open", standardButton: DialogButtonBox.Open, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" },
            { tag: "Save", standardButton: DialogButtonBox.Save, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" },
            { tag: "Cancel", standardButton: DialogButtonBox.Cancel, buttonRole: DialogButtonBox.RejectRole, signalName: "rejected" },
            { tag: "Close", standardButton: DialogButtonBox.Close, buttonRole: DialogButtonBox.RejectRole, signalName: "rejected" },
            { tag: "Discard", standardButton: DialogButtonBox.Discard, buttonRole: DialogButtonBox.DestructiveRole },
            { tag: "Apply", standardButton: DialogButtonBox.Apply, buttonRole: DialogButtonBox.ApplyRole },
            { tag: "Reset", standardButton: DialogButtonBox.Reset, buttonRole: DialogButtonBox.ResetRole },
            { tag: "RestoreDefaults", standardButton: DialogButtonBox.RestoreDefaults, buttonRole: DialogButtonBox.ResetRole },
            { tag: "Help", standardButton: DialogButtonBox.Help, buttonRole: DialogButtonBox.HelpRole, signalName: "helpRequested" },
            { tag: "SaveAll", standardButton: DialogButtonBox.SaveAll, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" },
            { tag: "Yes", standardButton: DialogButtonBox.Yes, buttonRole: DialogButtonBox.YesRole, signalName: "accepted" },
            { tag: "YesToAll", standardButton: DialogButtonBox.YesToAll, buttonRole: DialogButtonBox.YesRole, signalName: "accepted" },
            { tag: "No", standardButton: DialogButtonBox.No, buttonRole: DialogButtonBox.NoRole, signalName: "rejected" },
            { tag: "NoToAll", standardButton: DialogButtonBox.NoToAll, buttonRole: DialogButtonBox.NoRole, signalName: "rejected" },
            { tag: "Abort", standardButton: DialogButtonBox.Abort, buttonRole: DialogButtonBox.RejectRole, signalName: "rejected" },
            { tag: "Retry", standardButton: DialogButtonBox.Retry, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" },
            { tag: "Ignore", standardButton: DialogButtonBox.Ignore, buttonRole: DialogButtonBox.AcceptRole, signalName: "accepted" }
        ]
    }

    function test_signals(data) {
        var control = buttonBox.createObject(testCase)

        control.standardButtons = data.standardButton
        compare(control.count, 1)
        var button = control.itemAt(0)
        verify(button)
        compare(button.DialogButtonBox.buttonRole, data.buttonRole)

        var clickedSpy = signalSpy.createObject(control, {target: control, signalName: "clicked"})
        verify(clickedSpy.valid)
        var roleSpy = signalSpy.createObject(control, {target: control, signalName: data.signalName})
        compare(roleSpy.valid, !!data.signalName)

        button.clicked()
        compare(clickedSpy.count, 1)
        compare(clickedSpy.signalArguments[0][0], button)
        compare(roleSpy.count, !!data.signalName ? 1 : 0)

        control.destroy()
    }
}
