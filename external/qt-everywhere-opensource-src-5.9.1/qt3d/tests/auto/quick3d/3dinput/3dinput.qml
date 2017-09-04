/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


import Qt3D.Input 2.0 as QQ3Input20
import QtQuick 2.0

Item {

    // Misc
    //QQ3Input20.KeyEvent                     // (uncreatable) Qt3DInput::QKeyEvent
    QQ3Input20.KeyboardDevice {}              //Qt3DInput::QKeyboardDevice
    QQ3Input20.KeyboardHandler {}             //Qt3DInput::QKeyboardHandler
    QQ3Input20.InputSettings {}               //Qt3DInput::QInputSettings
    //QQ3Input20.MouseEvent                   // (uncreatable) Qt3DInput::QMouseEvent
    //QQ3Input20.WheelEvent                   // (uncreatable) Qt3DInput::QWheelEvent
    QQ3Input20.MouseHandler {}                //Qt3DInput::QMouseHandler
    QQ3Input20.MouseDevice {}                 //Qt3DInput::QMouseDevice
    //QQ3Input20.AbstractActionInput          // (uncreatable) Qt3DInput::QAbstractActionInput
    QQ3Input20.ActionInput {}                 //Qt3DInput::QActionInput
    //QQ3Input20.AbstractAxisInput            // (uncreatable) Qt3DInput::QAbstractAxisInput
    QQ3Input20.AxisSetting {}                 //Qt3DInput::QAxisSetting
    QQ3Input20.AnalogAxisInput {}             //Qt3DInput::QAnalogAxisInput
    QQ3Input20.ButtonAxisInput {}             //Qt3DInput::QButtonAxisInput
    QQ3Input20.GamepadInput {}                //Qt3DInput::QGamepadInput

}
