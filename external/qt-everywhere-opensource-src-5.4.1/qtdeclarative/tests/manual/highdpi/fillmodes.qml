/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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
