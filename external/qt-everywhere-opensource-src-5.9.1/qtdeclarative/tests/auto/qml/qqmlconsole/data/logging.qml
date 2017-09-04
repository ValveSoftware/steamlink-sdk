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

QtObject {
    id:root

    function consoleCount() {
        console.count("console.count", "Ignore additional argument");
        console.count();
    }

    Component.onCompleted: {
        console.debug("console.debug");
        console.log("console.log");
        console.info("console.info");
        console.warn("console.warn");
        console.error("console.error");

        consoleCount();
        consoleCount();

        var a = [1, 2];
        var b = {a: "hello", d: 1 };
        b.toString = function() { return JSON.stringify(b) }
        var c
        var d = 12;
        var e = function() { return 5;};
        var f = true;
        var g = {toString: function() { throw new Error('toString'); }};

        console.log(a);
        console.log(b);
        console.log(c);
        console.log(d);
        console.log(e);
        console.log(f);
        console.log(root);
        console.log(g);
        console.log(1, "pong!", new Object);
        console.log(1, ["ping","pong"], new Object, 2);

        try {
            console.log(exception);
        } catch (e) {
            return;
        }

        throw ("console.log(exception) should have raised an exception");
    }
}
