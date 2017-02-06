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
import QtQuick.Window 2.2
import QtQuick.Controls 2.1

ApplicationWindow {
    property alias childControl: childControl
    property alias childItem: childItem
    property alias childObject: childObject

    Control {
        id: childControl

        property ApplicationWindow attached_window: ApplicationWindow.window
        property Item attached_contentItem: ApplicationWindow.contentItem
        property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
        property Item attached_header: ApplicationWindow.header
        property Item attached_footer: ApplicationWindow.footer
        property Item attached_overlay: ApplicationWindow.overlay
    }

    Control {
        id: childItem

        property ApplicationWindow attached_window: ApplicationWindow.window
        property Item attached_contentItem: ApplicationWindow.contentItem
        property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
        property Item attached_header: ApplicationWindow.header
        property Item attached_footer: ApplicationWindow.footer
        property Item attached_overlay: ApplicationWindow.overlay
    }

    QtObject {
        id: childObject

        property ApplicationWindow attached_window: ApplicationWindow.window
        property Item attached_contentItem: ApplicationWindow.contentItem
        property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
        property Item attached_header: ApplicationWindow.header
        property Item attached_footer: ApplicationWindow.footer
        property Item attached_overlay: ApplicationWindow.overlay
    }

    property alias childWindow: childWindow
    property alias childWindowControl: childWindowControl
    property alias childWindowItem: childWindowItem
    property alias childWindowObject: childWindowObject

    Window {
        id: childWindow

        property ApplicationWindow attached_window: ApplicationWindow.window
        property Item attached_contentItem: ApplicationWindow.contentItem
        property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
        property Item attached_header: ApplicationWindow.header
        property Item attached_footer: ApplicationWindow.footer
        property Item attached_overlay: ApplicationWindow.overlay

        Control {
            id: childWindowControl

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }

        Control {
            id: childWindowItem

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }

        QtObject {
            id: childWindowObject

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }
    }

    property alias childAppWindow: childAppWindow
    property alias childAppWindowControl: childAppWindowControl
    property alias childAppWindowItem: childAppWindowItem
    property alias childAppWindowObject: childAppWindowObject

    ApplicationWindow {
        id: childAppWindow

        property ApplicationWindow attached_window: ApplicationWindow.window
        property Item attached_contentItem: ApplicationWindow.contentItem
        property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
        property Item attached_header: ApplicationWindow.header
        property Item attached_footer: ApplicationWindow.footer
        property Item attached_overlay: ApplicationWindow.overlay

        Control {
            id: childAppWindowControl

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }

        Control {
            id: childAppWindowItem

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }

        QtObject {
            id: childAppWindowObject

            property ApplicationWindow attached_window: ApplicationWindow.window
            property Item attached_contentItem: ApplicationWindow.contentItem
            property Item attached_activeFocusControl: ApplicationWindow.activeFocusControl
            property Item attached_header: ApplicationWindow.header
            property Item attached_footer: ApplicationWindow.footer
            property Item attached_overlay: ApplicationWindow.overlay
        }
    }
}
