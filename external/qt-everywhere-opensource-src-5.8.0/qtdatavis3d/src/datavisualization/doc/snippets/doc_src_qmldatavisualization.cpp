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

//! [0]
import QtDataVisualization 1.2
//! [0]

//! [1]
import QtQuick 2.0
import QtDataVisualization 1.2

Item {
    width: 640
    height: 480

    Bars3D {
        width: parent.width
        height: parent.height

        Bar3DSeries {
            itemLabelFormat: "@colLabel, @rowLabel: @valueLabel"

            ItemModelBarDataProxy {
                itemModel: dataModel
                // Mapping model roles to bar series rows, columns, and values.
                rowRole: "year"
                columnRole: "city"
                valueRole: "expenses"
            }
        }
    }

    ListModel {
        id: dataModel
        ListElement{ year: "2012"; city: "Oulu";     expenses: "4200"; }
        ListElement{ year: "2012"; city: "Rauma";    expenses: "2100"; }
        ListElement{ year: "2012"; city: "Helsinki"; expenses: "7040"; }
        ListElement{ year: "2012"; city: "Tampere";  expenses: "4330"; }
        ListElement{ year: "2013"; city: "Oulu";     expenses: "3960"; }
        ListElement{ year: "2013"; city: "Rauma";    expenses: "1990"; }
        ListElement{ year: "2013"; city: "Helsinki"; expenses: "7230"; }
        ListElement{ year: "2013"; city: "Tampere";  expenses: "4650"; }
    }
}
//! [1]

//! [2]
import QtQuick 2.0
import QtDataVisualization 1.2

Item {
    width: 640
    height: 480

    Scatter3D {
        width: parent.width
        height: parent.height
        Scatter3DSeries {
            ItemModelScatterDataProxy {
                itemModel: dataModel
                // Mapping model roles to scatter series item coordinates.
                xPosRole: "xPos"
                yPosRole: "yPos"
                zPosRole: "zPos"
            }
        }
    }

    ListModel {
        id: dataModel
        ListElement{ xPos: "2.754"; yPos: "1.455"; zPos: "3.362"; }
        ListElement{ xPos: "3.164"; yPos: "2.022"; zPos: "4.348"; }
        ListElement{ xPos: "4.564"; yPos: "1.865"; zPos: "1.346"; }
        ListElement{ xPos: "1.068"; yPos: "1.224"; zPos: "2.983"; }
        ListElement{ xPos: "2.323"; yPos: "2.502"; zPos: "3.133"; }
    }
}
//! [2]

//! [3]
import QtQuick 2.0
import QtDataVisualization 1.2

Item {
    width: 640
    height: 480

    Surface3D {
        width: parent.width
        height: parent.height
        Surface3DSeries {
            itemLabelFormat: "Pop density at (@xLabel N, @zLabel E): @yLabel"
            ItemModelSurfaceDataProxy {
                itemModel: dataModel
                // Mapping model roles to surface series rows, columns, and values.
                rowRole: "longitude"
                columnRole: "latitude"
                yPosRole: "pop_density"
            }
        }
    }
    ListModel {
        id: dataModel
        ListElement{ longitude: "20"; latitude: "10"; pop_density: "4.75"; }
        ListElement{ longitude: "21"; latitude: "10"; pop_density: "3.00"; }
        ListElement{ longitude: "22"; latitude: "10"; pop_density: "1.24"; }
        ListElement{ longitude: "23"; latitude: "10"; pop_density: "2.53"; }
        ListElement{ longitude: "20"; latitude: "11"; pop_density: "2.55"; }
        ListElement{ longitude: "21"; latitude: "11"; pop_density: "2.03"; }
        ListElement{ longitude: "22"; latitude: "11"; pop_density: "3.46"; }
        ListElement{ longitude: "23"; latitude: "11"; pop_density: "5.12"; }
        ListElement{ longitude: "20"; latitude: "12"; pop_density: "1.37"; }
        ListElement{ longitude: "21"; latitude: "12"; pop_density: "2.98"; }
        ListElement{ longitude: "22"; latitude: "12"; pop_density: "3.33"; }
        ListElement{ longitude: "23"; latitude: "12"; pop_density: "3.23"; }
        ListElement{ longitude: "20"; latitude: "13"; pop_density: "4.34"; }
        ListElement{ longitude: "21"; latitude: "13"; pop_density: "3.54"; }
        ListElement{ longitude: "22"; latitude: "13"; pop_density: "1.65"; }
        ListElement{ longitude: "23"; latitude: "13"; pop_density: "2.67"; }
    }
}
//! [3]

//! [7]
ItemModelBarDataProxy {
    itemModel: model // E.g. a list model defined elsewhere containing yearly expenses data.
    // Mapping model roles to bar series rows, columns, and values.
    rowRole: "year"
    columnRole: "city"
    valueRole: "expenses"
    rowCategories: ["2010", "2011", "2012", "2013"]
    columnCategories: ["Oulu", "Rauma", "Helsinki", "Tampere"]
}
//! [7]

//! [8]
ItemModelScatterDataProxy {
    itemModel: model // E.g. a list model defined elsewhere containing point coordinates.
    // Mapping model roles to scatter series item coordinates.
    xPosRole: "xPos"
    yPosRole: "yPos"
    zPosRole: "zPos"
}
//! [8]

//! [9]
ItemModelSurfaceDataProxy {
    itemModel: model // E.g. a list model defined elsewhere containing population data.
    // Mapping model roles to surface series rows, columns, and values.
    rowRole: "longitude"
    columnRole: "latitude"
    valueRole: "pop_density"
}
//! [9]
