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
import QtDataVisualization 1.1

Item {
    property alias column: columnAxis
    property alias row: rowAxis
    property alias value: valueAxis
    property alias total: totalAxis

    // Custom labels for columns, since the data contains abbreviated month names.
    //! [0]
    CategoryAxis3D {
        id: columnAxis
        labels: ["January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"]
        labelAutoRotation: 30
    }
    //! [0]
    CategoryAxis3D {
        id: totalAxis
        labels: ["Yearly total"]
        labelAutoRotation: 30
    }
    CategoryAxis3D {
        // For row labels we can use row labels from data proxy, no labels defined for rows.
        id: rowAxis
        labelAutoRotation: 30
    }

    ValueAxis3D {
        id: valueAxis
        min: 0
        max: 35
        labelFormat: "%.2f M\u20AC"
        title: "Monthly income"
        labelAutoRotation: 90
    }
}
