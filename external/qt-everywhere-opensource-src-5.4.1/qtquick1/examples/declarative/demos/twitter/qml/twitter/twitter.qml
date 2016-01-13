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
import "TwitterCore" 1.0 as Twitter

Item {
    id: screen; width: 320; height: 480
    property bool userView : false
    property variant tmpStr
    function setUser(str){hack.running = true; tmpStr = str}
    function reallySetUser(){rssModel.from = tmpStr;rssModel.to = ""; rssModel.phrase = ""}
    state:"searchquery"
    //Workaround for bug 260266
    Timer{ interval: 1; running: false; repeat: false; onTriggered: screen.reallySetUser(); id:hack }
    Keys.onEscapePressed: screen.state="searchquery"
    Keys.onBacktabPressed: screen.state="searchquery"
    Rectangle {
        id: background
        anchors.fill: parent; color: "#343434";

        state:"searchquery"
        Image { source: "TwitterCore/images/stripes.png"; fillMode: Image.Tile; anchors.fill: parent; opacity: 0.3 }

        MouseArea {
            anchors.fill: parent
            onClicked: screen.focus = false;
        }

        Twitter.RssModel { id: rssModel }
        Twitter.Loading { anchors.centerIn: parent; visible: rssModel.status==XmlListModel.Loading && state!='unauthed'}
        Text {
            width: 180
            text: "Could not access twitter using this screen name and password pair.";
            color: "#cccccc"; style: Text.Raised; styleColor: "black"; wrapMode: Text.WordWrap
            visible: rssModel.status==XmlListModel.Error; anchors.centerIn: parent
        }

        Item {
            id: views
            x: 2; width: parent.width - 4
            y:60 //Below the title bars
            height: parent.height - 100

            Text {
                id:title
                text: "Search Twitter"
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 20; font.bold: true; color: "#bbb"; style: Text.Raised; styleColor: "black"
                opacity:0
            }

            Twitter.SearchView{
                id: searchView
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width; height: parent.height-60;
                x: -(screen.width * 1.5)
            }

            Twitter.FatDelegate { id: fatDelegate }
            ListView {
                id: mainView; model: rssModel.model; delegate: fatDelegate;
                width: parent.width; height: parent.height; x: 0; cacheBuffer: 100;
            }
        }

        Twitter.MultiTitleBar { id: titleBar; width: parent.width }
        Twitter.ToolBar { id: toolBar; height: 40;
            //anchors.bottom: parent.bottom;
            //TODO: Use anchor changes instead of hard coding
            y: screen.height - 40
            width: parent.width; opacity: 0.9
            button1Label: "New Search"
            button2Label: "Update"
            onButton1Clicked:
            {
                screen.state="searchquery"
            }
            onButton2Clicked: rssModel.reload();
        }
    }
    states: [
        State {
            name: "searchquery";
            PropertyChanges { target: searchView; x: 0; focus:true}
            PropertyChanges { target: mainView; x: -(parent.width * 1.5) }
            PropertyChanges { target: titleBar; y: -80 }
            PropertyChanges { target: toolBar; y: screen.height }
            PropertyChanges { target: toolBar }
            PropertyChanges { target: title; opacity:1}
        }
    ]
    transitions: [
        Transition { NumberAnimation { properties: "x,y,opacity"; duration: 500; easing.type: Easing.InOutQuad } }
    ]
}
