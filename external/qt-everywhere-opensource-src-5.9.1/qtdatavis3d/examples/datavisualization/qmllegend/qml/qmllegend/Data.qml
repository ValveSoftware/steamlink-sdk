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
    property alias model: dataModel

    ListModel {
        id: dataModel
        ListElement{ year: "2010"; month: "Jan"; s1: "-14"; s2: "-15"; s3: "-15" }
        ListElement{ year: "2010"; month: "Feb"; s1: "-15"; s2: "-16"; s3: "-9" }
        ListElement{ year: "2010"; month: "Mar"; s1: "-7";  s2: "-4";  s3: "-2" }
        ListElement{ year: "2010"; month: "Apr"; s1: "3";   s2: "2";   s3: "2" }
        ListElement{ year: "2010"; month: "May"; s1: "7";   s2: "9";   s3: "10" }
        ListElement{ year: "2010"; month: "Jun"; s1: "12";  s2: "13";  s3: "22" }
        ListElement{ year: "2010"; month: "Jul"; s1: "18";  s2: "19";  s3: "24" }
        ListElement{ year: "2010"; month: "Aug"; s1: "15";  s2: "13";  s3: "16" }
        ListElement{ year: "2010"; month: "Sep"; s1: "6";   s2: "3";   s3: "4" }
        ListElement{ year: "2010"; month: "Oct"; s1: "1";   s2: "2";   s3: "-2" }
        ListElement{ year: "2010"; month: "Nov"; s1: "-2";  s2: "-5";  s3: "-6" }
        ListElement{ year: "2010"; month: "Dec"; s1: "-3";  s2: "-3";  s3: "-9" }

        ListElement{ year: "2011"; month: "Jan"; s1: "-12"; s2: "-11"; s3: "-14" }
        ListElement{ year: "2011"; month: "Feb"; s1: "-13"; s2: "-12"; s3: "-10" }
        ListElement{ year: "2011"; month: "Mar"; s1: "-6";  s2: "-4";  s3: "-3" }
        ListElement{ year: "2011"; month: "Apr"; s1: "0";   s2: "1";   s3: "3" }
        ListElement{ year: "2011"; month: "May"; s1: "4";   s2: "12";  s3: "11" }
        ListElement{ year: "2011"; month: "Jun"; s1: "9";   s2: "17";  s3: "23" }
        ListElement{ year: "2011"; month: "Jul"; s1: "15";  s2: "22";  s3: "25" }
        ListElement{ year: "2011"; month: "Aug"; s1: "12";  s2: "15";  s3: "12" }
        ListElement{ year: "2011"; month: "Sep"; s1: "2";   s2: "4";   s3: "7" }
        ListElement{ year: "2011"; month: "Oct"; s1: "-2";  s2: "4";   s3: "-4" }
        ListElement{ year: "2011"; month: "Nov"; s1: "-4";  s2: "-8";  s3: "-5" }
        ListElement{ year: "2011"; month: "Dec"; s1: "-6";  s2: "-6";  s3: "-7" }

        ListElement{ year: "2012"; month: "Jan"; s1: "-10"; s2: "-19"; s3: "-11" }
        ListElement{ year: "2012"; month: "Feb"; s1: "-11"; s2: "-17"; s3: "-4" }
        ListElement{ year: "2012"; month: "Mar"; s1: "-6";  s2: "-3";  s3: "-1" }
        ListElement{ year: "2012"; month: "Apr"; s1: "5";   s2: "1";   s3: "2" }
        ListElement{ year: "2012"; month: "May"; s1: "9";   s2: "12";  s3: "13" }
        ListElement{ year: "2012"; month: "Jun"; s1: "11";  s2: "16";  s3: "26" }
        ListElement{ year: "2012"; month: "Jul"; s1: "18";  s2: "20";  s3: "23" }
        ListElement{ year: "2012"; month: "Aug"; s1: "19";  s2: "12";  s3: "12" }
        ListElement{ year: "2012"; month: "Sep"; s1: "9";   s2: "1";   s3: "3" }
        ListElement{ year: "2012"; month: "Oct"; s1: "-3";  s2: "2";   s3: "-1" }
        ListElement{ year: "2012"; month: "Nov"; s1: "-5";  s2: "-4";  s3: "-3" }
        ListElement{ year: "2012"; month: "Dec"; s1: "-7";  s2: "-2";  s3: "-4" }

        ListElement{ year: "2013"; month: "Jan"; s1: "-18"; s2: "-19"; s3: "-19" }
        ListElement{ year: "2013"; month: "Feb"; s1: "-17"; s2: "-19"; s3: "-12" }
        ListElement{ year: "2013"; month: "Mar"; s1: "-9";  s2: "-6";  s3: "-5" }
        ListElement{ year: "2013"; month: "Apr"; s1: "0";   s2: "0";   s3: "0" }
        ListElement{ year: "2013"; month: "May"; s1: "4";   s2: "7";   s3: "9" }
        ListElement{ year: "2013"; month: "Jun"; s1: "9";   s2: "11";  s3: "18" }
        ListElement{ year: "2013"; month: "Jul"; s1: "13";  s2: "15";  s3: "20" }
        ListElement{ year: "2013"; month: "Aug"; s1: "10";  s2: "11";  s3: "13" }
        ListElement{ year: "2013"; month: "Sep"; s1: "3";   s2: "1";   s3: "2" }
        ListElement{ year: "2013"; month: "Oct"; s1: "0";   s2: "1";   s3: "-4" }
        ListElement{ year: "2013"; month: "Nov"; s1: "-5";  s2: "-6";  s3: "-5" }
        ListElement{ year: "2013"; month: "Dec"; s1: "-6";  s2: "-7";  s3: "-10" }
    }
}
