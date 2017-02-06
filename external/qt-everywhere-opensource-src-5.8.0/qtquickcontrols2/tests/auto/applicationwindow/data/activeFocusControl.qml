/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

import QtQuick 2.6
import QtQuick.Controls 2.1

ApplicationWindow {
    width: 400
    height: 400

    property alias container_column: column
    property alias textInput_column: ti_column
    property alias textEdit_column: te_column
    property alias textField_column: tf_column
    property alias textArea_column: ta_column
    property alias spinBox_column: sp_column
    property alias spinContent_column: sp_column.contentItem

    property alias container_frame: frame
    property alias textInput_frame: ti_frame
    property alias textEdit_frame: te_frame
    property alias textField_frame: tf_frame
    property alias textArea_frame: ta_frame
    property alias spinBox_frame: sp_frame
    property alias spinContent_frame: sp_frame.contentItem

    Column {
        id: column

        TextInput {
            id: ti_column
        }
        TextEdit {
            id: te_column
        }
        TextField {
            id: tf_column
        }
        TextArea {
            id: ta_column
        }
        SpinBox {
            id: sp_column
        }
    }

    Frame {
        id: frame

        Column {
            TextInput {
                id: ti_frame
            }
            TextEdit {
                id: te_frame
            }
            TextField {
                id: tf_frame
            }
            TextArea {
                id: ta_frame
            }
            SpinBox {
                id: sp_frame
            }
        }
    }
}
