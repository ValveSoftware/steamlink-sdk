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
    height: 400

    Row {
        Column {
            // should render a sharp image on retina displays: (sourceSize is in device-independent pixels)
            Image {
                sourceSize.width: 100
                sourceSize.height: 100
                source : "heart.svg"
            }

            // should render a sharp image on retina displays: if sourceSize is omitted
            // the svg viewBox size will be used; in this case 550x500
            Image {
                width : 100
                height : 100
                source : "heart.svg"
            }
        }

        // should render a sharp image on retina displays: (sourceSize is in device-independent pixels)
        Image {
            sourceSize.width: 700
            sourceSize.height: 700
            source : "heart.svg"
        }

        // should NOT render a sharp image on retina displays: if sourceSize is omitted
        // the svg viewBox size will be used; in this case 550x500
        Image {
            width : 700
            height : 700
            source : "heart.svg"
        }
    }
}
