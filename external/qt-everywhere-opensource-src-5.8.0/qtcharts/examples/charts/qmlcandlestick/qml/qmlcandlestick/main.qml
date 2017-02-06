/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
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

import QtQuick 2.0
import QtCharts 2.2

ChartView {
    title: "Candlestick series"
    width: 800
    height: 600
    theme: ChartView.ChartThemeLight
    legend.alignment: Qt.AlignBottom
    antialiasing: true

    CandlestickSeries {
        name: "Acme Ltd."
        increasingColor: "green"
        decreasingColor: "red"

        CandlestickSet { timestamp: 1435708800000; open: 6.90; high: 6.94; low: 5.99; close: 6.60 }
        CandlestickSet { timestamp: 1435795200000; open: 6.69; high: 6.69; low: 6.69; close: 6.69 }
        CandlestickSet { timestamp: 1436140800000; open: 4.85; high: 6.23; low: 4.85; close: 6.00 }
        CandlestickSet { timestamp: 1436227200000; open: 5.89; high: 6.15; low: 3.77; close: 5.69 }
        CandlestickSet { timestamp: 1436313600000; open: 4.64; high: 4.64; low: 2.54; close: 2.54 }
    }
}
