/****************************************************************************
**
** Copyright (C) 2016 Jolla Ltd, author: <gunnar.sletta@jollamobile.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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
import RenderNode 1.0

Item {
    width: 320
    height: 480

    Item { x: 10;   y: 10;  width: 10;  height: 10;
        StateRecorder { x: 10; y: 10; objectName: "no-clip; no-rotation"; }
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10; clip: true
        StateRecorder { x: 10; y: 10; objectName: "parent-clip; no-rotation"; }
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10;
        StateRecorder { x: 10; y: 10; objectName: "self-clip; no-rotation"; clip: true }
    }


    Item { x: 10;   y: 10;  width: 10;  height: 10; rotation: 90
        StateRecorder { x: 10; y: 10; objectName: "no-clip; parent-rotation"; }
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10; clip: true; rotation: 90
        StateRecorder { x: 10; y: 10; objectName: "parent-clip; parent-rotation"; }
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10; rotation: 90
        StateRecorder { x: 10; y: 10; objectName: "self-clip; parent-rotation"; clip: true }
    }


    Item { x: 10;   y: 10;  width: 10;  height: 10;
        StateRecorder { x: 10; y: 10; objectName: "no-clip; self-rotation"; rotation: 90 }
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10; clip: true;
        StateRecorder { x: 10; y: 10; objectName: "parent-clip; self-rotation"; rotation: 90}
    }

    Item { x: 10;   y: 10;  width: 10;  height: 10;
        StateRecorder { x: 10; y: 10; objectName: "self-clip; self-rotation"; clip: true; rotation: 90 }
    }

}
