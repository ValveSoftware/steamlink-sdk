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

import QtTest 1.0
import QtPositioning 5.2

TestCase {
    id: testCase

    name: "Address"

    Address {
        id: address

        street: "742 Evergreen Tce"
        district: "Pressboard Estates"
        city: "Springfield"
        state: "Oregon"
        postalCode: "8900"
        country: "United States"
        countryCode: "USA"
    }

    function test_qmlAddressText() {
        compare(address.isTextGenerated, true);
        compare(address.text, "742 Evergreen Tce<br/>Springfield, Oregon 8900<br/>United States");
        var textChangedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        textChangedSpy.target = address;
        textChangedSpy.signalName = "textChanged"

        var isTextGeneratedSpy = Qt.createQmlObject('import QtTest 1.0; SignalSpy {}', testCase, "SignalSpy");
        isTextGeneratedSpy.target = address
        isTextGeneratedSpy.signalName = "isTextGeneratedChanged"

        address.countryCode = "FRA";
        compare(address.text, "742 Evergreen Tce<br/>8900 Springfield<br/>United States");
        compare(textChangedSpy.count, 1);
        textChangedSpy.clear();
        compare(isTextGeneratedSpy.count, 0);

        address.text = "address label";
        compare(address.isTextGenerated, false);
        compare(address.text, "address label");
        compare(textChangedSpy.count, 1);
        textChangedSpy.clear();
        compare(isTextGeneratedSpy.count, 1);
        isTextGeneratedSpy.clear();

        address.countryCode = "USA";
        compare(address.text, "address label");
        compare(textChangedSpy.count, 0);
        textChangedSpy.clear();
        compare(isTextGeneratedSpy.count, 0);

        address.text = "";
        compare(address.isTextGenerated, true);
        compare(address.text, "742 Evergreen Tce<br/>Springfield, Oregon 8900<br/>United States");
        compare(textChangedSpy.count, 1);
        textChangedSpy.clear();
        compare(isTextGeneratedSpy.count, 1);
        isTextGeneratedSpy.clear();
    }
}
