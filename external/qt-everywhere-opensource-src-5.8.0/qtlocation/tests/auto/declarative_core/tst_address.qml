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
