/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQml>
#include <Qt3DInput/qkeyboarddevice.h>
#include <Qt3DInput/qkeyboardhandler.h>
#include <Qt3DInput/qkeyevent.h>
#include <Qt3DInput/qmousedevice.h>
#include <Qt3DInput/qmousehandler.h>
#include <Qt3DInput/qmouseevent.h>

#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/qaxisaccumulator.h>
#include <Qt3DInput/qaxissetting.h>
#include <Qt3DInput/qaction.h>
#include <Qt3DInput/qactioninput.h>
#include <Qt3DInput/qanalogaxisinput.h>
#include <Qt3DInput/qbuttonaxisinput.h>
#include <Qt3DInput/qinputsequence.h>
#include <Qt3DInput/qinputchord.h>
#include <Qt3DInput/qlogicaldevice.h>
#include <Qt3DInput/qabstractphysicaldevice.h>
#include <Qt3DInput/qinputsettings.h>
#include <Qt3DInput/private/qgenericinputdevice_p.h>
#include <Qt3DQuickInput/private/quick3daxis_p.h>
#include <Qt3DQuickInput/private/quick3daction_p.h>
#include <Qt3DQuickInput/private/quick3dinputchord_p.h>
#include <Qt3DQuickInput/private/quick3dinputsequence_p.h>
#include <Qt3DQuickInput/private/quick3dlogicaldevice_p.h>
#include <Qt3DQuickInput/private/quick3dphysicaldevice_p.h>

#ifdef HAVE_QGAMEPAD
# include <Qt3DInput/private/qgamepadinput_p.h>
#endif

#include "qt3dquick3dinputplugin.h"

QT_BEGIN_NAMESPACE

void Qt3DQuick3DInputPlugin::registerTypes(const char *uri)
{
    qmlRegisterUncreatableType<Qt3DInput::QKeyEvent>(uri, 2, 0, "KeyEvent", QStringLiteral("Events cannot be created"));
    qmlRegisterType<Qt3DInput::QKeyboardDevice>(uri, 2, 0, "KeyboardDevice");
    qmlRegisterType<Qt3DInput::QKeyboardHandler>(uri, 2, 0, "KeyboardHandler");
    qmlRegisterType<Qt3DInput::QInputSettings>(uri, 2, 0, "InputSettings");

    qmlRegisterUncreatableType<Qt3DInput::QMouseEvent>(uri, 2, 0, "MouseEvent", QStringLiteral("Events cannot be created"));
    qmlRegisterUncreatableType<Qt3DInput::QWheelEvent>(uri, 2, 0, "WheelEvent", QStringLiteral("Events cannot be created"));
    qmlRegisterType<Qt3DInput::QMouseHandler>(uri, 2, 0, "MouseHandler");
    qmlRegisterType<Qt3DInput::QMouseDevice>(uri, 2, 0, "MouseDevice");

    qmlRegisterExtendedType<Qt3DInput::QLogicalDevice, Qt3DInput::Input::Quick::Quick3DLogicalDevice>(uri, 2, 0, "LogicalDevice");
    qmlRegisterUncreatableType<Qt3DInput::QAbstractActionInput>(uri, 2, 0, "AbstractActionInput", QStringLiteral("AbstractActionInput is abstract"));
    qmlRegisterType<Qt3DInput::QActionInput>(uri, 2, 0, "ActionInput");
    qmlRegisterUncreatableType<Qt3DInput::QAbstractAxisInput>(uri, 2, 0, "AbstractAxisInput", QStringLiteral("AbstractAxisInput is abstract"));
    qmlRegisterType<Qt3DInput::QAxisSetting>(uri, 2, 0, "AxisSetting");
    qmlRegisterType<Qt3DInput::QAnalogAxisInput>(uri, 2, 0, "AnalogAxisInput");
    qmlRegisterType<Qt3DInput::QButtonAxisInput>(uri, 2, 0, "ButtonAxisInput");
    qmlRegisterExtendedType<Qt3DInput::QAxis, Qt3DInput::Input::Quick::Quick3DAxis>(uri, 2, 0, "Axis");
    qmlRegisterExtendedType<Qt3DInput::QAction, Qt3DInput::Input::Quick::Quick3DAction>(uri, 2, 0, "Action");
    qmlRegisterExtendedType<Qt3DInput::QInputSequence, Qt3DInput::Input::Quick::Quick3DInputSequence>(uri, 2, 0, "InputSequence");
    qmlRegisterExtendedType<Qt3DInput::QInputChord, Qt3DInput::Input::Quick::Quick3DInputChord>(uri, 2, 0, "InputChord");
    qmlRegisterExtendedUncreatableType<Qt3DInput::QAbstractPhysicalDevice, Qt3DInput::Input::Quick::Quick3DPhysicalDevice>(uri, 2, 0, "QAbstractPhysicalDevice", QStringLiteral("QAbstractPhysicalDevice is abstract"));
    qmlRegisterType<Qt3DInput::QAxisAccumulator>(uri, 2, 1, "AxisAccumulator");

#ifdef HAVE_QGAMEPAD
    qmlRegisterType<Qt3DInput::QGamepadInput>(uri, 2, 0, "GamepadInput");
#endif
}

QT_END_NAMESPACE
