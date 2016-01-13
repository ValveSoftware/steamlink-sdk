/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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
import QtQuick.Controls 1.2

Rectangle {
  width: 360
  height: 600

  ListModel {
    id: model_listmodel
    ListElement { value: "A" }
    ListElement { value: "B" }
    ListElement { value: "C" }
  }

  Column {
    anchors { left: parent.left; right: parent.right }
    TableView {
      model: model_listmodel // qml
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        role: "value"
        width: 100
      }
    }
    TableView {
      model: 3 // qml
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        width: 100
      }
    }
    TableView {
      model: ["A", "B", "C"] // qml
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        width: 100
      }
    }
    TableView {
      model: Item { x: 10 } // qml
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        role: "x"
        width: 100
      }
    }
    TableView {
      model: model_qobjectlist // c++
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        role: "value"
        width: 100
      }
    }
    TableView {
      model: model_qaim // c++
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        role: "test"
        width: 100
      }
    }
    TableView {
      model: model_qstringlist // c++
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        width: 100
      }
    }
    TableView {
      model: model_qobject // c++
      anchors { left: parent.left; right: parent.right }
      height: 70
      TableViewColumn {
        role: "value"
        width: 100
      }
    }
  }
}
