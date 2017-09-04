
/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the documentation of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:FDL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Free Documentation License Usage
** Alternatively, this file may be used under the terms of the GNU Free
** Documentation License version 1.3 as published by the Free Software
** Foundation and appearing in the file included in the packaging of
** this file. Please review the following information to ensure
** the GNU Free Documentation License version 1.3 requirements
** will be met: http://www.gnu.org/copyleft/fdl.html.
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 2.7
import QtQuick.Window 2.0
import QtQuick.LocalStorage 2.0
import "Database.js" as JS
import QtQuick.Layouts 1.1

Item {
    id: root
    width: Screen.width / 2
    height: Screen.height / 7

    function insertrec() {
        var rowid = parseInt(JS.dbInsert(dateInput.text, descInput.text, distInput.text), 10)
        if (rowid) {
            listView.model.setProperty(listView.currentIndex, "id", rowid)
            listView.forceLayout()
        }
        return rowid;
    }

    function editrec(Pdate, Pdesc, Pdistance, Prowid) {
        dateInput.text = Pdate
        descInput.text = Pdesc
        distInput.text = Pdistance
    }

    function initrec_new() {
        dateInput.clear()
        descInput.clear()
        distInput.clear()
        listView.model.insert(0, {
                                  date: "",
                                  trip_desc: "",
                                  distance: 0
                              })
        listView.currentIndex = 0
        dateInput.forceActiveFocus()
    }

    function initrec() {
        dateInput.clear()
        descInput.clear()
        distInput.clear()
    }

    function setlistview() {
        listView.model.setProperty(listView.currentIndex, "date",
                                   dateInput.text)
        listView.model.setProperty(listView.currentIndex, "trip_desc",
                                   descInput.text)
        listView.model.setProperty(listView.currentIndex, "distance",
                                   parseInt(distInput.text,10))
    }

    Rectangle {
        id: rootrect
        border.width: 10
        color: "#161616"

        ColumnLayout {
            id: mainLayout
            anchors.fill: parent

            Rectangle {
                id: gridBox
                Layout.fillWidth: true

                GridLayout {
                    id: gridLayout
                    rows: 3
                    flow: GridLayout.TopToBottom
                    anchors.fill: parent

                    Text {
                        text: "Date"
                        font.pixelSize: 22
                        rightPadding: 10
                    }

                    Text {
                        text: "Description"
                        font.pixelSize: 22
                        rightPadding: 10
                    }

                    Text {
                        text: "Distance"
                        font.pixelSize: 22
                    }

                    TextInput {
                        id: dateInput
                        font.pixelSize: 22
                        activeFocusOnPress: true
                        activeFocusOnTab: true
                        validator: RegExpValidator {
                            regExp: /[0-9/,:.]+/
                        }
                        onEditingFinished: {
                            if (dateInput.text == "") {
                                statustext.text = "Please fill in the date"
                                dateInput.forceActiveFocus()
                            }
                        }
                    }

                    TextInput {
                        id: descInput
                        font.pixelSize: 22
                        activeFocusOnPress: true
                        activeFocusOnTab: true
                        onEditingFinished: {
                            if (descInput.text.length < 8) {
                                statustext.text = "Enter a description of minimum 8 characters"
                                descInput.forceActiveFocus()
                            } else {
                                statustext.text = ""
                            }
                        }
                    }

                    TextInput {
                        id: distInput
                        font.pixelSize: 22
                        activeFocusOnPress: true
                        activeFocusOnTab: true
                        validator: RegExpValidator {
                            regExp: /\d{1,3}/
                        }
                        onEditingFinished: {
                            if (distInput.text == "") {
                                statustext.text = "Please fill in the distance"
                                distInput.forceActiveFocus()
                            }
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: dateInput.forceActiveFocus()
            }
        }
    }
}
