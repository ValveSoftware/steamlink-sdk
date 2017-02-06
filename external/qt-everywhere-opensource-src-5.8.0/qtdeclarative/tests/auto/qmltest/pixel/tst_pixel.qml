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
import QtTest 1.1

Rectangle {
    id:rect
    width: 40
    height: 40
    color:"red"
    TestCase {
        name: "Pixels"
        when: windowShown

        function test_pixel() {
           skip("test_pixel() is unstable, QTBUG-27671")
           var img = grabImage(rect);
           compare(img.pixel(20, 20), Qt.rgba(255, 0, 0, 255));
           compare(img.red(1,1), 255);
           compare(img.green(1,1), 0);
           compare(img.blue(1,1), 0);
           compare(img.alpha(1,1), 255);

           fuzzyCompare(img.red(1,1), 254, 2);
           fuzzyCompare(img.pixel(1,1), Qt.rgba(254, 0, 0, 254), 2);
           fuzzyCompare(img.pixel(1,1), "#FF0201", 2);

           rect.color = "blue";
           waitForRendering(rect);
           img = grabImage(rect);
           compare(img.pixel(20, 20), Qt.rgba(0, 0, 255, 255));
           compare(img.red(1,1), 0);
           compare(img.green(1,1), 0);
           compare(img.blue(1,1), 255);
           compare(img.alpha(1,1), 255);
        }

    }
}