#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the documentation of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL21$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia. For licensing terms and
## conditions see http://qt.digia.com/licensing. For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 or version 3 as published by the Free
## Software Foundation and appearing in the file LICENSE.LGPLv21 and
## LICENSE.LGPLv3 included in the packaging of this file. Please review the
## following information to ensure the GNU Lesser General Public License
## requirements will be met: https://www.gnu.org/licenses/lgpl.html and
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights. These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## $QT_END_LICENSE$
##
#############################################################################

import os, sys
from PyQt4.QtCore import *
from PyQt4.QtGui import *

class CustomWidget(QWidget):

    def __init__(self, parent, fake = False):

        QWidget.__init__(self, parent)
        gradient = QLinearGradient(QPointF(0, 0), QPointF(100.0, 100.0))
        baseColor = QColor(0xa6, 0xce, 0x39, 0x7f)
        gradient.setColorAt(0.0, baseColor.light(150))
        gradient.setColorAt(0.75, baseColor.light(75))
        self.brush = QBrush(gradient)
        self.fake = fake
        self.fakeBrush = QBrush(Qt.red, Qt.DiagCrossPattern)

        qtPath = QPainterPath()
        qtPath.setFillRule(Qt.OddEvenFill)
        qtPath.moveTo(-45.0, -20.0)
        qtPath.lineTo(0.0, -45.0)
        qtPath.lineTo(45.0, -20.0)
        qtPath.lineTo(45.0, 45.0)
        qtPath.lineTo(-45.0, 45.0)
        qtPath.lineTo(-45.0, -20.0)
        qtPath.closeSubpath()
        qtPath.moveTo(15.0, 5.0)
        qtPath.lineTo(35.0, 5.0)
        qtPath.lineTo(35.0, 40.0)
        qtPath.lineTo(15.0, 40.0)
        qtPath.lineTo(15.0, 5.0)
        qtPath.moveTo(-35.0, -15.0)
        qtPath.closeSubpath()
        qtPath.lineTo(-10.0, -15.0)
        qtPath.lineTo(-10.0, 10.0)
        qtPath.lineTo(-35.0, 10.0)
        qtPath.lineTo(-35.0, -15.0)
        qtPath.closeSubpath()
        self.path = qtPath

    def paintEvent(self, event):

        painter = QPainter()
        painter.begin(self)
        painter.setRenderHint(QPainter.Antialiasing)
        if self.fake:
            painter.fillRect(event.rect(), QBrush(Qt.white))
            painter.fillRect(event.rect(), self.fakeBrush)
        painter.setBrush(self.brush)
        painter.translate(60, 60)
        painter.drawPath(self.path)
        painter.end()

    def sizeHint(self):

        return QSize(120, 120)

    def minimumSizeHint(self):

        return QSize(120, 120)


if __name__ == "__main__":

    try:
        qt = sys.argv[1]
    except IndexError:
        qt = "4.1"

    if qt != "4.0" and qt != "4.1":
        sys.stderr.write("Usage: %s [4.0|4.1]\n" % sys.argv[0])
        sys.exit(1)

    app = QApplication(sys.argv)
    exec_dir = os.path.split(os.path.abspath(sys.argv[0]))[0]
    label = QLabel()
    label.setPixmap(QPixmap(os.path.join(exec_dir, "background.png")))

    layout = QGridLayout()
    label.setLayout(layout)
    if qt == "4.0":
        layout.addWidget(CustomWidget(label), 0, 0, Qt.AlignCenter)
        caption = QLabel("Opaque (Default)", label)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 0, Qt.AlignCenter | Qt.AlignTop)
    elif qt == "4.1":
        layout.addWidget(CustomWidget(label), 0, 0, Qt.AlignCenter)
        caption = QLabel("Contents Propagated (Default)", label)
        caption.setAutoFillBackground(True)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 0, Qt.AlignCenter | Qt.AlignTop)

    if qt == "4.0":
        contentsWidget = CustomWidget(label)
        contentsWidget.setAttribute(Qt.WA_ContentsPropagated, True)
        layout.addWidget(contentsWidget, 0, 1, Qt.AlignCenter)
        caption = QLabel("With WA_ContentsPropagated set", label)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 1, Qt.AlignCenter | Qt.AlignTop)
    elif qt == "4.1":
        autoFillWidget = CustomWidget(label)
        autoFillWidget.setAutoFillBackground(True)
        layout.addWidget(autoFillWidget, 0, 1, Qt.AlignCenter)
        caption = QLabel("With autoFillBackground set", label)
        caption.setAutoFillBackground(True)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 1, Qt.AlignCenter | Qt.AlignTop)

    if qt == "4.0":
        noBackgroundWidget = CustomWidget(label, fake = True)
        noBackgroundWidget.setAttribute(Qt.WA_NoBackground, True)
        layout.addWidget(noBackgroundWidget, 0, 2, Qt.AlignCenter)
        caption = QLabel("With WA_NoBackground set", label)
        caption.setWordWrap(True)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 2, Qt.AlignCenter | Qt.AlignTop)
    elif qt == "4.1":
        opaqueWidget = CustomWidget(label, fake = True)
        opaqueWidget.setAttribute(Qt.WA_OpaquePaintEvent, True)
        layout.addWidget(opaqueWidget, 0, 2, Qt.AlignCenter)
        caption = QLabel("With WA_OpaquePaintEvent set", label)
        caption.setAutoFillBackground(True)
        caption.setMargin(2)
        layout.addWidget(caption, 1, 2, Qt.AlignCenter | Qt.AlignTop)

    if qt == "4.0":
        label.setWindowTitle("Qt 4.0: Painting Custom Widgets")
    elif qt == "4.1":
        label.setWindowTitle("Qt 4.1: Painting Custom Widgets")

    label.resize(404, 160)
    label.show()
    sys.exit(app.exec_())
