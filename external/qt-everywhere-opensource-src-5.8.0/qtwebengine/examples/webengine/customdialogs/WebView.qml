/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

import QtQuick 2.0
import QtWebEngine 1.4

WebEngineView {

    url: "qrc:/index.html"
    property bool useDefaultDialogs: true
    signal openForm(var form)

    onContextMenuRequested: function(request) {
        // we only show menu for links with #openMenu
        if (!request.linkUrl.toString().endsWith("#openMenu")) {
            request.accepted = true;
            return;
        }
        // return early to show default menu
        if (useDefaultDialogs)
            return;

        request.accepted = true;
        openForm({item: Qt.resolvedUrl("forms/Menu.qml"),
                     properties: {"request": request}});
    }

    onAuthenticationDialogRequested: function(request) {
        if (useDefaultDialogs)
            return;

        request.accepted = true;
        openForm({item: Qt.resolvedUrl("forms/Authentication.qml"),
                     properties: {"request": request}});
    }

    onJavaScriptDialogRequested: function(request) {
        if (useDefaultDialogs)
            return;

        request.accepted = true;
        openForm({item: Qt.resolvedUrl("forms/JavaScript.qml"),
                     properties: {"request": request}});
    }

    onColorDialogRequested: function(request) {
        if (useDefaultDialogs)
            return;

        request.accepted = true;
        openForm({item: Qt.resolvedUrl("forms/ColorPicker.qml"),
                     properties: {"request": request}});
    }

    onFileDialogRequested: function(request) {
        if (useDefaultDialogs)
            return;

        request.accepted = true;
        openForm({item: Qt.resolvedUrl("forms/FilePicker.qml"),
                     properties: {"request": request}});

    }

    onFormValidationMessageRequested: function(request) {
        switch (request.type) {
        case FormValidationMessageRequest.Show:
            if (useDefaultDialogs)
                return;

            request.accepted = true;
            validationMessage.text = request.text;
            validationMessage.y = request.anchor.y + request.anchor.height + 10;
            validationMessage.visible = true;
            break;
        case FormValidationMessageRequest.Move:
            break;
        case FormValidationMessageRequest.Hide:
            validationMessage.visible = false;
            break;
        }
    }

    MessageRectangle {
        id: validationMessage
        z: 1
    }
}
