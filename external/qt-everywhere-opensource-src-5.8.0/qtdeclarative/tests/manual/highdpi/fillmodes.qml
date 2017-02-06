/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

Rectangle {
    width: 400
    height: 700

    // the images should have the same display size and appearance on each row.
    Column {
        anchors.centerIn: parent.Center
        Row {
            Image {  width: 130; height: 70; fillMode: Image.Stretch; source : "heart-lowdpi.png" }
            Image {  width: 130; height: 70; fillMode: Image.Stretch; source : "heart.png" }
            Image {  width: 130; height: 70; fillMode: Image.Stretch; source : "heart-highdpi@2x.png" }
        }
        Row {
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectFit; source : "heart-lowdpi.png" }
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectFit; source : "heart.png" }
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectFit; source : "heart-highdpi@2x.png" }
        }
        Row {
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectCrop; source : "heart-lowdpi.png" }
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectCrop; source : "heart.png" }
            Image {  width: 130; height: 100; fillMode: Image.PreserveAspectCrop; source : "heart-highdpi@2x.png" }
        }
        Row {
            Image {  width: 230; height: 200; fillMode: Image.Tile; source : "heart-lowdpi.png" }
            Image {  width: 230; height: 200; fillMode: Image.Tile; source : "heart.png" }
            Image {  width: 230; height: 200; fillMode: Image.Tile; source : "heart-highdpi@2x.png" }
        }
    }
}
