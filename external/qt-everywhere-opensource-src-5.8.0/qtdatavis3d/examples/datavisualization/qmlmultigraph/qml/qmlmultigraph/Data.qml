/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.1

Item {
    property alias sharedData: dataModel

    ListModel {
        id: dataModel
        ListElement{ row: "0"; col: "0"; longitude: "20.0"; latitude: "10.0"; pop_density: "4.75"; }
        ListElement{ row: "1"; col: "0"; longitude: "21.1"; latitude: "10.3"; pop_density: "3.00"; }
        ListElement{ row: "2"; col: "0"; longitude: "22.5"; latitude: "10.7"; pop_density: "1.24"; }
        ListElement{ row: "3"; col: "0"; longitude: "24.0"; latitude: "10.5"; pop_density: "2.53"; }
        ListElement{ row: "0"; col: "1"; longitude: "20.2"; latitude: "11.2"; pop_density: "3.55"; }
        ListElement{ row: "1"; col: "1"; longitude: "21.3"; latitude: "11.5"; pop_density: "3.03"; }
        ListElement{ row: "2"; col: "1"; longitude: "22.6"; latitude: "11.7"; pop_density: "3.46"; }
        ListElement{ row: "3"; col: "1"; longitude: "23.4"; latitude: "11.5"; pop_density: "4.12"; }
        ListElement{ row: "0"; col: "2"; longitude: "20.2"; latitude: "12.3"; pop_density: "3.37"; }
        ListElement{ row: "1"; col: "2"; longitude: "21.1"; latitude: "12.4"; pop_density: "2.98"; }
        ListElement{ row: "2"; col: "2"; longitude: "22.5"; latitude: "12.1"; pop_density: "3.33"; }
        ListElement{ row: "3"; col: "2"; longitude: "23.3"; latitude: "12.7"; pop_density: "3.23"; }
        ListElement{ row: "0"; col: "3"; longitude: "20.7"; latitude: "13.3"; pop_density: "5.34"; }
        ListElement{ row: "1"; col: "3"; longitude: "21.5"; latitude: "13.2"; pop_density: "4.54"; }
        ListElement{ row: "2"; col: "3"; longitude: "22.4"; latitude: "13.6"; pop_density: "4.65"; }
        ListElement{ row: "3"; col: "3"; longitude: "23.2"; latitude: "13.4"; pop_density: "6.67"; }
        ListElement{ row: "0"; col: "4"; longitude: "20.6"; latitude: "15.0"; pop_density: "6.01"; }
        ListElement{ row: "1"; col: "4"; longitude: "21.3"; latitude: "14.6"; pop_density: "5.83"; }
        ListElement{ row: "2"; col: "4"; longitude: "22.5"; latitude: "14.8"; pop_density: "7.32"; }
        ListElement{ row: "3"; col: "4"; longitude: "23.7"; latitude: "14.3"; pop_density: "6.90"; }
    }
}

