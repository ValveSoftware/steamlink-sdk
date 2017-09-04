/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

Flow {
    flow: Flow.TopToBottom
    spacing: 5
    property string fontDescription: ""

    Button {
        text: fontDescription + " bold"
        onClicked: editedFont().bold = !editedFont().bold;
    }
    Button {
        text: fontDescription + " capitalization"
        onClicked: editedFont().capitalization++;
    }
    Button {
        text: fontDescription + " font family"
        onClicked: editedFont().family = "courier";
    }
    Button {
        text: fontDescription + " font italic"
        onClicked: editedFont().italic = !editedFont().italic;
    }
    Button {
        text: fontDescription + " letterSpacing +"
        onClicked: editedFont().letterSpacing++;
    }
    Button {
        text: fontDescription + " letterSpacing -"
        onClicked: editedFont().letterSpacing--;
    }
    Button {
        text: fontDescription + " pixelSize +"
        onClicked: editedFont().pixelSize++;
    }
    Button {
        text: fontDescription + " pixelSize -"
        onClicked: editedFont().pixelSize--;
    }
    Button {
        text: fontDescription + " pointSize +"
        onClicked: editedFont().pointSize++;
    }
    Button {
        text: fontDescription + " pointSize -"
        onClicked: editedFont().pointSize--;
    }
    Button {
        text: fontDescription + " strikeout"
        onClicked: editedFont().strikeout = !editedFont().strikeout;
    }
    Button {
        text: fontDescription + " underline"
        onClicked: editedFont().underline = !editedFont().underline;
    }
    Button {
        text: fontDescription + " weight +"
        onClicked: editedFont().weight++;
    }
    Button {
        text: fontDescription + " weight -"
        onClicked: editedFont().weight--;
    }
    Button {
        text: fontDescription + " wordSpacing +"
        onClicked: editedFont().wordSpacing++;
    }
    Button {
        text: fontDescription + " wordSpacing -"
        onClicked: editedFont().wordSpacing--;
    }
}
