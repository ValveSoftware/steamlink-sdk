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

    //! [0]
    ListModel {
        id: dataModel
        ListElement{ timestamp: "2006-01"; expenses: "-4";  income: "5" }
        ListElement{ timestamp: "2006-02"; expenses: "-5";  income: "6" }
        ListElement{ timestamp: "2006-03"; expenses: "-7";  income: "4" }
        //! [0]
        ListElement{ timestamp: "2006-04"; expenses: "-3";  income: "2" }
        ListElement{ timestamp: "2006-05"; expenses: "-4";  income: "1" }
        ListElement{ timestamp: "2006-06"; expenses: "-2";  income: "2" }
        ListElement{ timestamp: "2006-07"; expenses: "-1";  income: "3" }
        ListElement{ timestamp: "2006-08"; expenses: "-5";  income: "1" }
        ListElement{ timestamp: "2006-09"; expenses: "-2";  income: "3" }
        ListElement{ timestamp: "2006-10"; expenses: "-5";  income: "2" }
        ListElement{ timestamp: "2006-11"; expenses: "-8";  income: "5" }
        ListElement{ timestamp: "2006-12"; expenses: "-3";  income: "3" }

        ListElement{ timestamp: "2007-01"; expenses: "-3";  income: "1" }
        ListElement{ timestamp: "2007-02"; expenses: "-4";  income: "2" }
        ListElement{ timestamp: "2007-03"; expenses: "-12"; income: "4" }
        ListElement{ timestamp: "2007-04"; expenses: "-13"; income: "6" }
        ListElement{ timestamp: "2007-05"; expenses: "-14"; income: "11" }
        ListElement{ timestamp: "2007-06"; expenses: "-7";  income: "7" }
        ListElement{ timestamp: "2007-07"; expenses: "-6";  income: "4" }
        ListElement{ timestamp: "2007-08"; expenses: "-4";  income: "15" }
        ListElement{ timestamp: "2007-09"; expenses: "-2";  income: "18" }
        ListElement{ timestamp: "2007-10"; expenses: "-29"; income: "25" }
        ListElement{ timestamp: "2007-11"; expenses: "-23"; income: "29" }
        ListElement{ timestamp: "2007-12"; expenses: "-5";  income: "9" }

        ListElement{ timestamp: "2008-01"; expenses: "-3";  income: "8" }
        ListElement{ timestamp: "2008-02"; expenses: "-8";  income: "14" }
        ListElement{ timestamp: "2008-03"; expenses: "-10"; income: "20" }
        ListElement{ timestamp: "2008-04"; expenses: "-12"; income: "24" }
        ListElement{ timestamp: "2008-05"; expenses: "-10"; income: "19" }
        ListElement{ timestamp: "2008-06"; expenses: "-5";  income: "8" }
        ListElement{ timestamp: "2008-07"; expenses: "-1";  income: "4" }
        ListElement{ timestamp: "2008-08"; expenses: "-7";  income: "12" }
        ListElement{ timestamp: "2008-09"; expenses: "-4";  income: "16" }
        ListElement{ timestamp: "2008-10"; expenses: "-22"; income: "33" }
        ListElement{ timestamp: "2008-11"; expenses: "-16"; income: "25" }
        ListElement{ timestamp: "2008-12"; expenses: "-2";  income: "7" }

        ListElement{ timestamp: "2009-01"; expenses: "-4";  income: "5"  }
        ListElement{ timestamp: "2009-02"; expenses: "-4";  income: "7"  }
        ListElement{ timestamp: "2009-03"; expenses: "-11"; income: "14"  }
        ListElement{ timestamp: "2009-04"; expenses: "-16"; income: "22"  }
        ListElement{ timestamp: "2009-05"; expenses: "-3";  income: "5"  }
        ListElement{ timestamp: "2009-06"; expenses: "-4";  income: "8"  }
        ListElement{ timestamp: "2009-07"; expenses: "-7";  income: "9"  }
        ListElement{ timestamp: "2009-08"; expenses: "-9";  income: "13"  }
        ListElement{ timestamp: "2009-09"; expenses: "-1";  income: "6"  }
        ListElement{ timestamp: "2009-10"; expenses: "-14"; income: "25"  }
        ListElement{ timestamp: "2009-11"; expenses: "-19"; income: "29"  }
        ListElement{ timestamp: "2009-12"; expenses: "-5";  income: "7"  }

        ListElement{ timestamp: "2010-01"; expenses: "-14"; income: "22"  }
        ListElement{ timestamp: "2010-02"; expenses: "-5";  income: "7"  }
        ListElement{ timestamp: "2010-03"; expenses: "-1";  income: "9"  }
        ListElement{ timestamp: "2010-04"; expenses: "-1";  income: "12"  }
        ListElement{ timestamp: "2010-05"; expenses: "-5";  income: "9"  }
        ListElement{ timestamp: "2010-06"; expenses: "-5";  income: "8"  }
        ListElement{ timestamp: "2010-07"; expenses: "-3";  income: "7"  }
        ListElement{ timestamp: "2010-08"; expenses: "-1";  income: "5"  }
        ListElement{ timestamp: "2010-09"; expenses: "-2";  income: "4"  }
        ListElement{ timestamp: "2010-10"; expenses: "-10"; income: "13"  }
        ListElement{ timestamp: "2010-11"; expenses: "-12"; income: "17"  }
        ListElement{ timestamp: "2010-12"; expenses: "-6";  income: "9"  }

        ListElement{ timestamp: "2011-01"; expenses: "-2";  income: "6"  }
        ListElement{ timestamp: "2011-02"; expenses: "-4";  income: "8"  }
        ListElement{ timestamp: "2011-03"; expenses: "-7";  income: "12"  }
        ListElement{ timestamp: "2011-04"; expenses: "-9";  income: "15"  }
        ListElement{ timestamp: "2011-05"; expenses: "-7";  income: "19"  }
        ListElement{ timestamp: "2011-06"; expenses: "-9";  income: "18"  }
        ListElement{ timestamp: "2011-07"; expenses: "-13"; income: "17"  }
        ListElement{ timestamp: "2011-08"; expenses: "-5";  income: "9"  }
        ListElement{ timestamp: "2011-09"; expenses: "-3";  income: "8"  }
        ListElement{ timestamp: "2011-10"; expenses: "-13"; income: "15"  }
        ListElement{ timestamp: "2011-11"; expenses: "-8";  income: "17"  }
        ListElement{ timestamp: "2011-12"; expenses: "-7";  income: "10"  }

        ListElement{ timestamp: "2012-01"; expenses: "-12";  income: "16"  }
        ListElement{ timestamp: "2012-02"; expenses: "-24";  income: "28"  }
        ListElement{ timestamp: "2012-03"; expenses: "-27";  income: "22"  }
        ListElement{ timestamp: "2012-04"; expenses: "-29";  income: "25"  }
        ListElement{ timestamp: "2012-05"; expenses: "-27";  income: "29"  }
        ListElement{ timestamp: "2012-06"; expenses: "-19";  income: "18"  }
        ListElement{ timestamp: "2012-07"; expenses: "-13";  income: "17"  }
        ListElement{ timestamp: "2012-08"; expenses: "-15";  income: "19"  }
        ListElement{ timestamp: "2012-09"; expenses: "-3";   income: "8"  }
        ListElement{ timestamp: "2012-10"; expenses: "-3";   income: "6"  }
        ListElement{ timestamp: "2012-11"; expenses: "-4";   income: "8"  }
        ListElement{ timestamp: "2012-12"; expenses: "-5";   income: "9"  }
    }
}
