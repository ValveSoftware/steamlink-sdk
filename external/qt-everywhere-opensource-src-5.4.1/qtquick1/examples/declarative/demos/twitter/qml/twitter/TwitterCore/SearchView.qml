/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

import QtQuick 1.0

FocusScope {
    id: wrapper
    Column {
        anchors.centerIn: parent
        spacing: 20
        Column{
            spacing: 4
            Text {
                text: "Posted by:"
                font.pixelSize: 16; font.bold: true; color: "white"; style: Text.Raised; styleColor: "black"
                horizontalAlignment: Qt.AlignRight
            }
            Input{
                id: fromIn
                KeyNavigation.backtab: searchbutton
                KeyNavigation.tab:toIn
                onAccepted:searchbutton.doSearch();
                focus: true
            }
            Text {
                text: "In reply to:"
                font.pixelSize: 16; font.bold: true; color: "white"; style: Text.Raised; styleColor: "black"
                horizontalAlignment: Qt.AlignRight
            }
            Input{
                id: toIn
                KeyNavigation.backtab: fromIn
                KeyNavigation.tab:phraseIn
                onAccepted:searchbutton.doSearch();
            }
            Text {
                text: "Search phrase:"
                font.pixelSize: 16; font.bold: true; color: "white"; style: Text.Raised; styleColor: "black"
                horizontalAlignment: Qt.AlignRight
            }
            Input{
                id: phraseIn
                KeyNavigation.backtab: toIn
                KeyNavigation.tab:searchbutton
                onAccepted:searchbutton.doSearch();
                text: "Qt Quick"
            }
        }
        Button {
            width: 100
            height: 32
            id: searchbutton
            keyUsing: true;
            opacity: 1
            text: "Search"
            KeyNavigation.tab: fromIn
            Keys.onReturnPressed: searchbutton.doSearch();
            Keys.onEnterPressed: searchbutton.doSearch();
            Keys.onSelectPressed: searchbutton.doSearch();
            Keys.onSpacePressed: searchbutton.doSearch();
            onClicked: searchbutton.doSearch();

            function doSearch() {
                // Search ! allowed
                if (wrapper.state=="invalidinput")
                    return;

                rssModel.from=fromIn.text;
                rssModel.to= toIn.text;
                rssModel.phrase = phraseIn.text;
                screen.focus = true;
                screen.state = ""
            }
        }
    }
    states:
    State {
        name: "invalidinput"
        when: fromIn.text=="" && toIn.text=="" && phraseIn.text==""
        PropertyChanges { target: searchbutton ; opacity: 0.6 ; }
    }
    transitions:
    Transition {
        NumberAnimation { target: searchbutton; property: "opacity"; duration: 200 }
    }
}
