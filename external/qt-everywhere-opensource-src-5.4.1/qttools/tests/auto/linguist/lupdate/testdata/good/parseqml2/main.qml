/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

import QtQuick 1.0; QtObject {
function translate() {
qsTranslate();
qsTranslate(10);
qsTranslate(10, 20);
qsTranslate("10", 20);
qsTranslate(10, "20");

QT_TRANSLATE_NOOP();
QT_TRANSLATE_NOOP(10);
QT_TRANSLATE_NOOP(10, 20);
QT_TRANSLATE_NOOP("10", 20);
QT_TRANSLATE_NOOP(10, "20");

qsTr();
qsTr(10);

QT_TR_NOOP();
QT_TR_NOOP(10);

qsTrId();
qsTrId(10);

QT_TRID_NOOP();
QT_TRID_NOOP(10);

//% This is wrong
//% "This is not wrong" This is wrong
//% "I forgot to close the meta string
//% "Being evil \

//% "Should cause a warning"
qsTranslate("FooContext", "Hello");
//% "Should cause a warning"
QT_TRANSLATE_NOOP("FooContext", "World");
//% "Should cause a warning"
qsTr("Hello");
//% "Should cause a warning"
QT_TR_NOOP("World");

//: This comment will be discarded.
Math.sin(1);
//= id_foobar
Math.cos(2);
//~ underflow False
Math.tan(3);

/*
Not tested for now, because these should perhaps not cause
translation entries to be generated at all; see QTBUG-11843.

//= qtn_foo
qsTrId("qtn_foo");
//= qtn_bar
QT_TRID_NOOP("qtn_bar");
*/

}

}