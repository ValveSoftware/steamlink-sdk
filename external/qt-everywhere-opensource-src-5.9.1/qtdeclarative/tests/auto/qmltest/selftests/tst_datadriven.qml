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

Item {
  TestCase {
    name:"data-driven-empty-init-data"
    property int tests:0;
    property int init_data_called_times:0;
    function init_data() {init_data_called_times++;}
    function initTestCase() {tests = 0; init_data_called_times = 0;}
    function cleanupTestCase() {compare(tests, 2); compare(init_data_called_times, 2);}

    function test_test1() {tests++;}
    function test_test2() {tests++;}
  }
  TestCase {
    name:"data-driven-no-init-data"
    property int tests:0;
    function initTestCase() {tests = 0;}
    function cleanupTestCase() {compare(tests, 2);}

    function test_test1() {tests++;}
    function test_test2() {tests++;}
  }
  TestCase {
    name:"data-driven-init-data"
    property int tests:0;
    property int init_data_called_times:0;
    function initTestCase() {tests = 0; init_data_called_times = 0;}
    function cleanupTestCase() {compare(tests, 2); compare(init_data_called_times, 1);}
    function init_data() {init_data_called_times++; return [{tag:"data1", data:"test data 1"}];}

    function test_test1(row) {tests++; compare(row.data, "test data 1");}
    function test_test2_data() {return [{tag:"data2", data:"test data 2"}]; }
    function test_test2(row) {tests++; compare(row.data, "test data 2");}
  }
}
