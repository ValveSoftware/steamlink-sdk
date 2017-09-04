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

Item {
    id: listviewelementtest
    anchors.fill: parent
    property string testtext: ""
    property bool hivis: false

    Rectangle {
        id: listviewelementbackground
        color: "lightgray";  border.color: "gray"; radius: 5; clip: true; opacity: .1
        height: listviewelement.height + 10; width: listviewelement.width + 10
        anchors.centerIn: listviewelement
    }

    ListView {
        id: listviewelement
        height: 250; width: parent.width *.8; clip: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 15
        model: devices
        highlightFollowsCurrentItem: true
        highlight: highlightrect
        delegate: delegaterect
        section.property: "os"
        section.delegate: sectiondelegaterect
        header: headerrect
        footer: footerrect
        spacing: 0
        Behavior on spacing { NumberAnimation { duration: 200 } }
    }

    Component { id: delegaterect
        Item {
            width: listviewelement.width - 4; height: 30
            Rectangle { color: "blue"; anchors.fill: parent; radius: 5; border.color: "black"; opacity: .1 }
            Text { text: model.name; anchors.centerIn: parent }
        }
    }

    Component { id: sectiondelegaterect
        Item {
            width: listviewelement.width - 4; height: 30
            Rectangle { color: "green"; anchors.fill: parent; radius: 5; border.color: "black"; opacity: .5 }
            Text { text: section; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 5 }
        }
    }

    Component { id: highlightrect
        Rectangle {
            id: coloredbox; color: "yellow"; width: listviewelement.width - 4; height: 30; radius: 5; opacity: .5; visible: hivis
            SequentialAnimation on color { loops: Animation.Infinite; running: coloredbox.visible
                ColorAnimation { to: "green"; duration: 2000 }
                ColorAnimation { to: "yellow"; duration: 2000 }
            }
        }
    }

    Component { id: headerrect
        Rectangle { id: hdr; color: "brown"; width: listviewelement.width - 4; height: 30; radius: 5; opacity: .5
            Text { anchors.centerIn: parent; text: "Nokia Devices" }
        }
    }

    Component { id: footerrect
        Rectangle { id: ftr; color: "brown"; width: listviewelement.width - 4; height: 30; radius: 5; opacity: .5
            Text { anchors.centerIn: parent; text: "Nokia pty ltd." }
        }
    }

    SystemTestHelp { id: helpbubble; visible: statenum != 0
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 50 }
    }
    BugPanel { id: bugpanel }

    states: [
        State { name: "start"; when: statenum == 1
            PropertyChanges { target: listviewelementtest
                testtext: "This is a ListView element. At present it should be displaying the names of a few Nokia devices, ordered by Operating System.\n"+
                "Next, let's view the end of the list" }
        },
        State { name: "end"; when: statenum == 2
            StateChangeScript { script: listviewelement.positionViewAtEnd(); }
            PropertyChanges { target: listviewelementtest
                testtext: "The end of the list should now be visible.\nNext, let's add a highlight." }
        },
        State { name: "highlight"; when: statenum == 3
            PropertyChanges { target: listviewelement; currentIndex: 0 }
            StateChangeScript { script: listviewelement.positionViewAtBeginning(); }
            PropertyChanges { target: listviewelementtest; hivis: true }
            PropertyChanges { target: listviewelementtest
                testtext: "The ListView should now be showing a highlight on the first item.\n"+
                "Next, let's move to an item down the list." }
        },
        State { name: "highlight7"; when: statenum == 4
            PropertyChanges { target: listviewelement; currentIndex: 6 }
            PropertyChanges { target: listviewelementtest; hivis: true }
            PropertyChanges { target: listviewelementtest
                testtext: "The ListView should now be showing the highlight over item 7 - the N95\n"+
                "Next let's add some space between them." }
        },
        State { name: "sections"; when: statenum == 5
            PropertyChanges { target: listviewelement; currentIndex: 6 }
            PropertyChanges { target: listviewelement; spacing: 5 }
            PropertyChanges { target: listviewelementtest; hivis: true }
            PropertyChanges { target: listviewelementtest
                testtext: "The ListView should now be showing a space between each list item,\n"+
                "but not after the header and sections, or before the footer.\n"+
                "Advance to restart the test." }
        }
    ]

    ListModel {
        id: devices
        ListElement { name: "N900"; os: "Maemo" }
        ListElement { name: "N810"; os: "Maemo" }
        ListElement { name: "N9"; os: "Meego" }
        ListElement { name: "N-Gage QD"; os: "S60 1.2" }
        ListElement { name: "E90"; os: "S60 2nd" }
        ListElement { name: "E72"; os: "S60 3.2" }
        ListElement { name: "N95"; os: "S60 5.0" }
        ListElement { name: "X6"; os: "S60 5.0" }
        ListElement { name: "N97"; os: "Symbian^1" }
        ListElement { name: "5800"; os: "Symbian^1" }
        ListElement { name: "E7"; os: "Symbian^3" }
        ListElement { name: "N8"; os: "Symbian^3" }
        ListElement { name: "C7"; os: "Symbian^3" }
        ListElement { name: "X7"; os: "Symbian Anna" }
    }

}
