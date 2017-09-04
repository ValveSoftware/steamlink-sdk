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

    Connections { target: pointvalue; onReset: { textelement.font.pointSize = 12 } }

    Item {
        id: textpanel

        anchors.fill: parent
        anchors.rightMargin: controlpanel.width

        Text {
            id: textelement
            height: parent.height - 20; width: parent.width - 20
            anchors.centerIn: parent
            anchors.fill: parent;
            anchors.margins: 10

            text: { textvalue.model.get(textvalue.currentIndex).value }
            textFormat: { formatvalue.model.get(formatvalue.currentIndex).value }
            color: { colorvalue.model.get(colorvalue.currentIndex).value }
            elide: { elidevalue.model.get(elidevalue.currentIndex).value }
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
            maximumLineCount: { maxlinevalue.model.get(maxlinevalue.currentIndex).value }
            lineHeight: { lineheightvalue.model.get(lineheightvalue.currentIndex).value }
            lineHeightMode: { lineheightmodevalue.model.get(lineheightmodevalue.currentIndex).value }
            smooth: { smoothvalue.model.get(smoothvalue.currentIndex).value }
            style: { stylevalue.model.get(stylevalue.currentIndex).value }
            styleColor: { stylecolorvalue.model.get(stylecolorvalue.currentIndex).value }
            fontSizeMode : { fontsizemodevalue.model.get(fontsizemodevalue.currentIndex).value }
            minimumPointSize : { minimumpointsizevalue.model.get(minimumpointsizevalue.currentIndex).value }

            Rectangle{ color: "transparent"; border.color: "green"; anchors.fill: parent }
        }

        Text {
            id: infopanel
            anchors { left: parent.left; leftMargin: 10; bottom: parent.bottom }
            height: 150; color: "black"; width: 150
            function elidename() {
                switch (textelement.elide) {
                    case Text.ElideNone: return "None";
                    case Text.ElideLeft: return "Left";
                    case Text.ElideMiddle: return "Middle";
                    case Text.ElideRight: return "Right";
                }
            }
            text:
            "LineCount: "+textelement.lineCount+" of "+textelement.maximumLineCount+
            "\nPaintedHeight/Width: "+textelement.paintedHeight+"/"+textelement.paintedWidth+
            "\nPointSize: "+textelement.font.pointSize+
            "\nPixelSize: "+textelement.font.pixelSize+
            "\nTruncated: "+textelement.truncated+
            "\nElide: "+ elidename()

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
                    textmodel.append({ "name": "Basic",
                    "value": "Qt Quick is a collection of technologies that are designed to help developers create the kind of intuitive, "+
                    "modern, fluid user interfaces that are increasingly used on mobile phones, media players, set-top boxes and other "+
                    "portable devices.\n"+
                    "Qt Quick consists of a rich set of user interface elements, a language for describing user interfaces "+
                    "and a language runtime. "+
                    "A collection of C++ APIs is used to integrate these high level features with classic Qt applications."});
                    textmodel.append({ "name": "Short",
                    "value": "Button Text."});
                    textmodel.append({ "name": "Long",
                    "value": "QtQuickisacollectionoftechnologiesthataredesignedtohelpdeveloperscreatethekindofintuitive,"+
                    "modern,fluiduserinterfacesthatareincreasinglyusedonmobilephones,mediaplayers,set-topboxesandother"+
                    "portabledevices."});
                    textmodel.append({ "name": "Rich",
                    "value": "<b>Qt Quick</b> is a collection of technologies that are designed to help developers create the kind of <i>intuitive, "+
                    "modern, fluid</i> user interfaces that are increasingly used on mobile phones, media players, set-top boxes and other "+
                    "portable devices.<br>"+
                    "Qt Quick consists of a rich set of user interface elements, a language for describing user interfaces "+
                    "and a language runtime. "+
                    "A collection of C++ APIs is used to integrate these high level features with classic Qt applications."});
                }
            }
            ControlView {
                id: formatvalue
                controlname: "Format"
                model: ListModel { ListElement { name: "Auto"; value: Text.AutoText } ListElement { name: "Plain"; value: Text.PlainText }
                    ListElement { name: "Rich"; value: Text.RichText } ListElement { name: "Styled"; value: Text.StyledText } } }
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
                model: ListModel { ListElement { name: "-1"; value: -1 } ListElement { name: "8"; value: 8 } ListElement { name: "20"; value: 20 } ListElement { name: "50"; value: 20 } } }
            ControlView {
                id: pointvalue
                controlname: "Point"
                model: ListModel { ListElement { name: "-1"; value: -1 } ListElement { name: "8"; value: 8 } ListElement { name: "20"; value: 20 } ListElement { name: "50"; value: 20 } } }
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
                model: ListModel { ListElement { name: "Max"; value: 2147483647 } ListElement { name: "1"; value: 1 }
                    ListElement { name: "2"; value: 2 } ListElement { name: "10"; value: 10 }} }
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
                id: fontsizemodevalue
                controlname: "FontSize"
                model: ListModel { ListElement { name: "FixedSize"; value: Text.FixedSize } ListElement { name: "Horizontal"; value: Text.HorizontalFit }
                    ListElement { name: "Vertical"; value: Text.VerticalFit } ListElement { name: "Fit"; value: Text.Fit } } }
            ControlView {
                id: minimumpixelsizevalue
                controlname: "MinPixelSize"
                model: ListModel { ListElement { name: "8"; value: 8 } ListElement { name: "12"; value: 12 }
                    ListElement { name: "24"; value: 24 } ListElement { name: "32"; value: 32 } } }
            ControlView {
                id: minimumpointsizevalue
                controlname: "MinPointSize"
                model: ListModel { ListElement { name: "8"; value: 8 } ListElement { name: "12"; value: 12 }
                    ListElement { name: "24"; value: 24 } ListElement { name: "32"; value: 32 } } }
        }
    }
}

















