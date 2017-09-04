/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

Rectangle {
    height: 360; width: 640

    Connections { target: pointvalue; onReset: { textinputelement.font.pointSize = 12 } }

    Item {
        id: textpanel
        height: 360
        width: 440
        MouseArea { anchors.fill: parent; onClicked: textinputelement.focus = false }
        TextInput {
            id: textinputelement
            height: 50; width: parent.width - 20
            anchors.centerIn: parent
            focus: true
            property int validatorval
            validatorval: { validvalue.model.get(validvalue.currentIndex).value }
            text: { textvalue.model.get(textvalue.currentIndex).value }
            autoScroll: { autoscrollvalue.model.get(autoscrollvalue.currentIndex).value }
            color: { colorvalue.model.get(colorvalue.currentIndex).value }
            font.bold: { boldvalue.model.get(boldvalue.currentIndex).value }
            font.italic: { italicsvalue.model.get(italicsvalue.currentIndex).value }
            font.capitalization: { capsvalue.model.get(capsvalue.currentIndex).value }
            font.family: { familyvalue.model.get(familyvalue.currentIndex).value }
            font.strikeout: strikeoutvalue.currentIndex
            font.underline: underlinevalue.currentIndex
            font.letterSpacing: { lspacingvalue.model.get(lspacingvalue.currentIndex).value }
            font.wordSpacing: { wspacingvalue.model.get(wspacingvalue.currentIndex).value }
            font.weight: { weightvalue.model.get(weightvalue.currentIndex).value }
            font.pointSize: { pointvalue.model.get(pointvalue.currentIndex).value }
            font.pixelSize: { pixelvalue.model.get(pixelvalue.currentIndex).value }
            horizontalAlignment: { halignvalue.model.get(halignvalue.currentIndex).value }
            verticalAlignment: { valignvalue.model.get(valignvalue.currentIndex).value }
            wrapMode: { wrapvalue.model.get(wrapvalue.currentIndex).value }
            smooth: { smoothvalue.model.get(smoothvalue.currentIndex).value }
            selectByMouse: { mousevalue.model.get(mousevalue.currentIndex).value }
            echoMode: { echovalue.model.get(echovalue.currentIndex).value }
            cursorVisible: { cursorvisiblevalue.model.get(cursorvisiblevalue.currentIndex).value }
            cursorDelegate: cursordelegate
            onValidatorvalChanged: {
                //console.log(validatorval);
                if (validatorval == 0)
                    validator = intval;
                else if (validatorval == 1)
                    validator = dubval;
                else if (validatorval == 2)
                    validator = regval;
                else
                    validator = noval;
            }
            IntValidator { id: intval; top: 30; bottom: 12 }
            DoubleValidator { id: dubval; top: 30; bottom: 12 }
            RegExpValidator { id: regval; regExp: /Qt/ }
            RegExpValidator { id: noval; regExp: /.*/ }
            Rectangle{ color: "transparent"; border.color: "green"; anchors.fill: parent }
        }

        Row {
            id: buttonrow
            height: 40
            width: parent.width - 20
            anchors.left: textinputelement.left
            anchors.bottom: buttonrow2.top; anchors.bottomMargin: 2
            Button { buttontext: "Select Word"; onClicked: { textinputelement.selectWord() } }
            Button { buttontext: "Select All"; onClicked: { textinputelement.selectAll() } }
            Button { buttontext: "Select to 5"; onClicked: { textinputelement.moveCursorSelection(5) } }
            Button { buttontext: "Select None"; onClicked: { textinputelement.deselect() } }
        }
        Row {
            id: buttonrow2
            height: 40
            width: parent.width - 20
            anchors.left: textinputelement.left
            anchors.bottom: buttonrow3.top; anchors.bottomMargin: 2
            Button { buttontext: "Cut"; onClicked: { textinputelement.cut() } }
            Button { buttontext: "Copy"; onClicked: { textinputelement.copy() } }
            Button { buttontext: "Paste"; onClicked: { textinputelement.paste() } }
        }
        Row {
            id: buttonrow3
            height: 40
            width: parent.width - 20
            anchors.left: textinputelement.left
            anchors.bottom: textinputelement.top; anchors.bottomMargin: 2
            Button { buttontext: "Position 12px"; onClicked: { textinputelement.cursorPosition = textinputelement.positionAt(12) } }
            Button { buttontext: "Select 3-6"; onClicked: { textinputelement.select(3,6) } }
            Button { buttontext: "Position End"; onClicked: { textinputelement.cursorPosition = textinputelement.text.length } }
        }
        Component {
            id: cursordelegate
            Rectangle {
                id: cursordelegaterect
                height: 48; width: 3; color: "red"; visible: parent.cursorVisible
                SequentialAnimation on opacity { running: true; loops: Animation.Infinite
                    NumberAnimation { to: 0; duration: 1000 }
                    NumberAnimation { to: 1; duration: 1000 }
                }
            }
        }

        Text {
            id: infopanel
            anchors { left: parent.left; leftMargin: 10; bottom: parent.bottom }
            height: 150; color: "black"; width: 150
            text:
            "\nPointSize: "+textinputelement.font.pointSize+
            "\nPixelSize: "+textinputelement.font.pixelSize+
            "\nAcceptable Input: "+textinputelement.acceptableInput+
            "\nCan Paste: "+textinputelement.canPaste+
            "\nCursor Position: "+textinputelement.cursorPosition+
            "\nRight to Left: "+textinputelement.isRightToLeft(0,textinputelement.text.length);
        }
    }

    Item {
        id: controlpanel
        width: 200; height: parent.height
        anchors.right: parent.right
        Rectangle { anchors.fill: parent; color: "transparent"; border.color: "black" }
        ListView { id: controls; model: controlsmodel; anchors.fill: parent; clip: true; cacheBuffer: 500 }
        VisualItemModel {
            id: controlsmodel
            ControlView {
                id: textvalue
                controlname: "Text"
                model: textmodel
                ListModel { id: textmodel }
                Component.onCompleted: {
                    textmodel.append({ "name": "Basic", "value": "The TextInput item displays an editable line of text."});
                    textmodel.append({ "name": "Short", "value": "Hello World"});
                    textmodel.append({ "name": "Long",
                    "value": "Qt Quick is a collection of technologies that are designed to help developers create the kind of intuitive, "+
                    "modern, fluid user interfaces that are increasingly used on mobile phones, media players, set-top boxes and other "+
                    "portable devices. "+
                    "Qt Quick consists of a rich set of user interface elements, a language for describing user interfaces "+
                    "and a language runtime. "+
                    "A collection of C++ APIs is used to integrate these high level features with classic Qt applications."});
                }
            }
            ControlView {
                id: autoscrollvalue
                controlname: "AutoScroll"
                model: ListModel { ListElement { name: "On"; value: true } ListElement { name: "Off"; value: false } }
            }
            ControlView {
                id: colorvalue
                controlname: "Color"
                model: ListModel { ListElement { name: "Red"; value: "red" }
                    ListElement { name: "Green"; value: "green" } ListElement { name: "Blue"; value: "blue" } }
            }
            ControlView {
                id: elidevalue
                controlname: "Elide"
                model: ListModel { ListElement { name: "None"; value: Text.ElideNone } ListElement { name: "Left"; value: Text.ElideLeft }
                    ListElement { name: "Middle"; value: Text.ElideMiddle } ListElement { name: "Right"; value: Text.ElideRight } }
            }
            ControlView {
                id: boldvalue
                controlname: "Bold"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } }
            }
            ControlView {
                id: italicsvalue
                controlname: "Italic"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } }
            }
            ControlView {
                id: capsvalue
                controlname: "Cap10n"
                model: ListModel {
                    ListElement { name: "Mixed"; value: Font.MixedCase } ListElement { name: "Upper"; value: Font.AllUppercase }
                    ListElement { name: "Lower"; value: Font.AllLowercase } ListElement { name: "SmallCaps"; value: Font.SmallCaps }
                    ListElement { name: "Capitals"; value: Font.Capitalize }
                }
            }
            ControlView {
                id: cursorvisiblevalue
                controlname: "Cursor"
                model: ListModel { ListElement { name: "On"; value: true } ListElement { name: "Off"; value: false } }
            }
            ControlView {
                id: echovalue
                controlname: "Echo"
                model: ListModel { ListElement { name: "Normal"; value: TextInput.Normal } ListElement { name: "Password"; value: TextInput.Password }
                    ListElement { name: "None"; value: TextInput.NoEcho } ListElement { name: "OnEdit"; value: TextInput.PasswordEchoOnEdit } } }
            ControlView {
                id: familyvalue
                controlname: "Font"
                property variant fontfamilies
                function setModel() {
                    familiesmodel.clear();
                    for (var i = 0; i < fontfamilies.length; ++i) {
                        familiesmodel.append({ "name": fontfamilies[i], "value": fontfamilies[i] });
                    }
                    familyvalue.currentIndex = 0;
                }
                model: familiesmodel
                ListModel { id: familiesmodel }
                Component.onCompleted: { fontfamilies = Qt.fontFamilies(); setModel(); }
            }
            ControlView {
                id: lspacingvalue
                controlname: "LSpacing"
                model: ListModel { ListElement { name: "0"; value: 0 } ListElement { name: "1"; value: 1 } ListElement { name: "2.5"; value: 2.5 } } }
            ControlView {
                id: wspacingvalue
                controlname: "WSpacing"
                model: ListModel { ListElement { name: "-1"; value: -1 } ListElement { name: "8"; value: 8 } ListElement { name: "20"; value: 20 } } }
            ControlView {
                id: pixelvalue
                controlname: "Pixel"
                model: ListModel { ListElement { name: "-1"; value: -1 } ListElement { name: "8"; value: 8 } ListElement { name: "20"; value: 20 } } }
            ControlView {
                id: pointvalue
                controlname: "Point"
                model: ListModel { ListElement { name: "-1"; value: -1 } ListElement { name: "8"; value: 8 } ListElement { name: "20"; value: 20 } } }
            ControlView {
                id: strikeoutvalue
                controlname: "Strike"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } } }
            ControlView {
                id: underlinevalue
                controlname: "U_line"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } } }
            ControlView {
                id: weightvalue
                controlname: "Weight"
                model: ListModel { ListElement { name: "Light"; value: Font.Light } ListElement { name: "Normal"; value: Font.Normal }
                    ListElement { name: "DemiBold"; value: Font.DemiBold } ListElement { name: "Bold"; value: Font.Bold }
                    ListElement { name: "Black"; value: Font.Black } }
                    Component.onCompleted: { currentIndex = 1 } // set to default
            }
            ControlView {
                id: halignvalue
                controlname: "HAlign"
                model: ListModel { ListElement { name: "Left"; value: Text.AlignLeft } ListElement { name: "Right"; value: Text.AlignRight }
                    ListElement { name: "Center"; value: Text.AlignHCenter } ListElement { name: "Justify"; value: Text.AlignJustify } } }
            ControlView {
                id: valignvalue
                controlname: "VAlign"
                model: ListModel { ListElement { name: "Top"; value: Text.AlignTop } ListElement { name: "Bottom"; value: Text.AlignBottom }
                    ListElement { name: "Center"; value: Text.AlignVCenter } } }
            ControlView {
                id: maxlinevalue
                controlname: "Lines"
                model: ListModel { ListElement { name: "Unset"; value: 1 } ListElement { name: "2"; value: 2 } ListElement { name: "10"; value: 10 }} }
            ControlView {
                id: lineheightvalue
                controlname: "LHeight"
                model: ListModel { ListElement { name: "1"; value: 1.0 } ListElement { name: "2"; value: 2.0 } ListElement { name: "30"; value: 30.0 }} }
            ControlView {
                id: lineheightmodevalue
                controlname: "LHMode"
                model: ListModel {
                    ListElement { name: "Multiply"; value: Text.ProportionalHeight } ListElement { name: "Fixed"; value: Text.FixedHeight } } }
            ControlView {
                id: mousevalue
                controlname: "Mouse"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } } }
            ControlView {
                id: smoothvalue
                controlname: "Smooth"
                model: ListModel { ListElement { name: "Off"; value: false } ListElement { name: "On"; value: true } } }
            ControlView {
                id: stylevalue
                controlname: "Style"
                model: ListModel { ListElement { name: "Normal"; value: Text.Normal } ListElement { name: "Outline"; value: Text.Outline }
                    ListElement { name: "Raised"; value: Text.Raised } ListElement { name: "Sunken"; value: Text.Sunken } } }
            ControlView {
                id: stylecolorvalue
                controlname: "SColor"
                model: ListModel { ListElement { name: "Green"; value: "green" } ListElement { name: "Blue"; value: "blue" } } }
            ControlView {
                id: wrapvalue
                controlname: "Wrap"
                model: ListModel { ListElement { name: "None"; value: Text.NoWrap } ListElement { name: "Word"; value: Text.WordWrap }
                    ListElement { name: "Anywhere"; value: Text.WrapAnywhere } ListElement { name: "Wrap"; value: Text.Wrap } } }
            ControlView {
                id: validvalue
                controlname: "Valid"
                model: ListModel { ListElement { name: "None"; value: -1 } ListElement { name: "Int"; value: 0 }
                    ListElement { name: "Double"; value: 1 } ListElement { name: "RegExp"; value: 2 } } }
        }
    }
}

















