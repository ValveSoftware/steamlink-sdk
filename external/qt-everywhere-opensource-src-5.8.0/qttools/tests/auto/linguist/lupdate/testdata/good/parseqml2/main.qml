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