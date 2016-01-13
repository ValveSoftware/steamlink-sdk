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

import QtQuick 2.0
import QtTest 1.0
import QtLocation 5.3
import "utils.js" as Utils

TestCase {
    id: testCase

    name: "Supplier"

    Supplier { id: emptySupplier }

    function test_empty() {
        compare(emptySupplier.supplierId, "");
        compare(emptySupplier.name, "");
        compare(emptySupplier.url, "");
        verify(emptySupplier.icon);
    }

    Supplier {
        id: qmlSupplier

        supplierId: "test-supplier-id"
        name: "Test Supplier"
        url: "http://example.com/test-supplier-id"

        icon: Icon {
            Component.onCompleted:  {
                parameters.singleUrl = "http://example.com/icons/test-supplier.png"
            }
        }
    }

    function test_qmlConstructedSupplier() {
        compare(qmlSupplier.supplierId, "test-supplier-id");
        compare(qmlSupplier.name, "Test Supplier");
        compare(qmlSupplier.url, "http://example.com/test-supplier-id");
        verify(qmlSupplier.icon);
        compare(qmlSupplier.icon.parameters.singleUrl, "http://example.com/icons/test-supplier.png");
    }

    Supplier {
        id: testSupplier
    }

    Plugin {
        id: testPlugin
        name: "qmlgeo.test.plugin"
        allowExperimental: true
    }

    Plugin {
        id: invalidPlugin
    }

    Icon {
        id: testIcon
    }

    function test_setAndGet_data() {
        return [
            { tag: "name", property: "name", signal: "nameChanged", value: "Test Supplier", reset: "" },
            { tag: "supplierId", property: "supplierId", signal: "supplierIdChanged", value: "test-supplier-id-1", reset: "" },
            { tag: "url", property: "url", signal: "urlChanged", value: "http://example.com/test-supplier-id-1", reset: "" },
            { tag: "icon", property: "icon", signal: "iconChanged", value: testIcon }
        ];
    }

    function test_setAndGet(data) {
        Utils.testObjectProperties(testCase, testSupplier, data);
    }
}
