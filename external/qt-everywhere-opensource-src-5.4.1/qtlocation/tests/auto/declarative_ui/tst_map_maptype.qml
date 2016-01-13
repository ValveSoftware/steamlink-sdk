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

TestCase {
    id: testCase

    name: "MapType"

    Plugin {
        id: nokiaPlugin
        name: "nokia"
        parameters: [
                       PluginParameter {
                           name: "app_id"
                           value: "stub"
                       },
                       PluginParameter {
                           name: "token"
                           value: "stub"
                       }
                   ]
    }

    Map {
        id: map;
        plugin: nokiaPlugin
        center {
            latitude: 62.240501
            longitude: 25.757014
        }
        width: 100
        height: 100
    }

    SignalSpy {id: supportedSetSpy; target: map; signalName: "supportedMapTypesChanged"}
    SignalSpy {id: activeMapTypeChangedSpy; target: map; signalName: "activeMapTypeChanged"}

    function initTestCase() {
        if (map.supportedMapTypes.length == 0 && supportedSetSpy.count == 0) {
            wait(1000)
            if (supportedSetSpy.count == 0)
                wait(2000)
            compare(supportedSetSpy.count, 1,
                    "supportedMapTypesChanged signal didn't arrive")
        }
    }

    function test_supported_types() {
        var count = map.supportedMapTypes.length
        console.log('Number of supported map types: ' + count)

        console.log('Supported map types:')
        for (var i = 0; i < count; i++) {
            console.log('style: ' + map.supportedMapTypes[i].style
                        + ', name: ' + map.supportedMapTypes[i].name
                        + ', desc: ' + map.supportedMapTypes[i].description
                        + ', mobile: ' + map.supportedMapTypes[i].mobile)
        }
    }

    function test_setting_types() {
        var count = map.supportedMapTypes.length
        console.log('Number of supported map types: '
                    + map.supportedMapTypes.length)

        activeMapTypeChangedSpy.clear();
        for (var i = 0; i < count; i++) {
            console.log('setting ' + map.supportedMapTypes[i].name)
            map.activeMapType = map.supportedMapTypes[i]
            compare(map.supportedMapTypes[i].name, map.activeMapType.name,
                    "Error setting the active maptype (or getting it after)")
        }
        console.log('change count: ' + activeMapTypeChangedSpy.count)
        compare(activeMapTypeChangedSpy.count, count)
    }
}
