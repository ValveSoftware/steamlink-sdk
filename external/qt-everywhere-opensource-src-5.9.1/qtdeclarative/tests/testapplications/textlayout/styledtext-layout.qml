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
    id: main
    width: 1024; height: 1024
    focus: true

    property real offset: 0
    property real margin: 15

    Keys.onLeftPressed: myText.horizontalAlignment = Text.AlignLeft
    Keys.onUpPressed: myText.horizontalAlignment = Text.AlignHCenter
    Keys.onRightPressed: myText.horizontalAlignment = Text.AlignRight
    Keys.onDownPressed: myText.horizontalAlignment = Text.AlignJustify

    Text {
        id: myText
        anchors.fill: parent
        anchors.margins: 20
        wrapMode: Text.WordWrap
        font.family: "Times New Roman"
        font.pixelSize: 18
        textFormat: Text.StyledText
        horizontalAlignment: Text.AlignJustify

        text: "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Integer at ante dui sed eu egestas est facilis <a href=\"www.qt-project.org\">www.qt-project.org</a>.<br/>Curabitur ante est, pulvinar quis adipiscing a, iaculis id ipsum. Phasellus id neque id velit facilisis cursus ac sit amet nibh. Donec enim arcu, pharetra non semper nec, iaculis eget elit. Nunc blandit condimentum odio vel egestas.<br><ul type=\"bullet\"><li>Coffee<ol type=\"a\"><li>Espresso<li><b>Cappuccino</b><li><i>Flat White</i><li>Latte</ol><li>Juice<ol type=\"1\"><li>Orange</li><li>Apple</li><li>Pineapple</li><li>Tomato</li></ol></li></ul><p><font color=\"#434343\"><i>Proin consectetur <b>sapien</b> in ipsum lacinia sit amet mattis orci interdum. Quisque vitae accumsan lectus. Ut nisi turpis, sollicitudin ut dignissim id, fermentum ac est. Maecenas nec libero leo. Sed ac leo eget ipsum ultricies viverra sit amet eu orci. Praesent et tortor risus, viverra accumsan sapien. Sed faucibus eleifend lectus, sed euismod urna porta eu. Aenean ultricies lectus ut orci dictum quis convallis nisi ultrices. Nunc elit mi, iaculis a porttitor rutrum, venenatis malesuada nisi. Suspendisse turpis quam, euismod non imperdiet et, rutrum nec ligula. Lorem ipsum dolor sit amet, consectetur adipiscing elit. Etiam semper tristique metus eu sodales. Integer eget risus ipsum. Quisque ut risus ut nulla tristique volutpat at sit amet nisl. Aliquam pulvinar auctor diam nec bibendum. Quisque luctus sapien id arcu volutpat pharetra. Praesent pretium imperdiet euismod. Integer fringilla rhoncus condimentum. Quisque sit amet ornare nulla. Cras sapien augue, sagittis a dictum id, suscipit et nunc. Cras vitae augue in enim elementum venenatis sed nec risus. Sed nisi quam, mollis quis auctor ac, vestibulum in neque. Vivamus eu justo risus. Suspendisse vel mollis est. Vestibulum gravida interdum mi, in molestie neque gravida in.</i></font><br><br> Donec nibh odio, mattis facilisis vulputate et, scelerisque ut felis. Sed ornare eros nec odio aliquam eu varius augue adipiscing. Vivamus sit amet massa dapibus sapien pulvinar consectetur a sit amet felis. Cras non mi id libero dictum iaculis id dignissim eros. Praesent eget enim dui, sed bibendum neque. Ut interdum nisl id leo malesuada ornare.<br><br>Pellentesque id nisl eu odio volutpat posuere et at massa. Pellentesque nec lorem justo. Integer sem urna, pharetra sed sagittis vitae, condimentum ac felis. Ut vitae sapien ac tortor adipiscing pharetra. Cras tristique urna tempus ante volutpat eleifend non eu ligula. Mauris sodales nisl et lorem tristique sodales. Mauris arcu orci, vehicula semper cursus ac, dapibus ut mi. Cras orci ligula, lacinia non laoreet non, feugiat eget lorem. Duis commodo urna nunc. Ut eu diam quis magna volutpat auctor. Duis non nibh non leo aliquet gravida. <font color=\"green\">Aenean diam velit, eleifend sed porta eu, malesuada sed erat.</font> In hac habitasse platea dictumst. Ut nulla ligula, tincidunt ac volutpat nec, accumsan at risus. Donec eget ipsum sit amet nulla tempus auctor ut non massa. Donec enim purus, consectetur viverra congue vitae, vehicula eu sapien. Ut aliquam iaculis metus, a bibendum nisi fringilla ut. Maecenas ut libero augue, vitae tristique diam. Vivamus nec rhoncus ipsum. Maecenas rutrum, libero sit amet ultrices cursus, elit massa laoreet odio, in luctus elit quam eu quam. Sed non diam urna. Maecenas fringilla feugiat malesuada. In tellus nibh, gravida vitae cursus mollis, tincidunt eu urna. Cras turpis lorem, dictum in feugiat id, gravida eu nulla. In ultricies nisl in sapien consectetur eu ultricies nisl facilisis. Nam id mauris a leo pretium facilisis eget quis est. Fusce fermentum quam in metus facilisis semper."


        onLineLaidOut: {
            line.width = width / 2  - (2 * margin)
            if (line.number === 40) {
                main.offset = line.y
            }
            if (line.number >= 40) {
                line.x = width / 2 + margin
                line.y -= main.offset
            }
            if ((line.y + line.height) > rect.y && line.y < (rect.y + rect.height)) {
                if (line.number < 40)
                    line.width = Math.min((rect.x - line.x), line.width)
                else {
                    line.x = Math.max((rect.x + rect.width), width / 2 + margin)
                    line.width = Math.min((width - margin - line.x), line.width)
                }
            }
        }

        Item {
            id: rect
            x: 280; y: 200
            width: 300; height: 300

            Rectangle {
                anchors { fill: parent; leftMargin: 15; rightMargin: 15 }
                color: "lightsteelblue"; opacity: 0.3
            }

            MouseArea {
                anchors.fill: parent
                drag.target: rect
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse.button == Qt.RightButton ? myText.font.pixelSize -= 1 : myText.font.pixelSize += 1
                onPositionChanged: myText.forceLayout()
            }
        }
    }

}
