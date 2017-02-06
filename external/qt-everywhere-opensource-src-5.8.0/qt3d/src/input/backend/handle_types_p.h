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

#ifndef QT3DINPUT_INPUT_HANDLE_TYPES_P_H
#define QT3DINPUT_INPUT_HANDLE_TYPES_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <Qt3DCore/private/qhandle_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DInput {
namespace Input {

class KeyboardDevice;
class KeyboardHandler;
class MouseHandler;
class MouseDevice;
class Axis;
class AxisActionHandler;
class AbstractAxisInput;
class AxisSetting;
class Action;
class ActionInput;
class InputSequence;
class InputChord;
class LogicalDevice;
class GenericDeviceBackendNode;
class PhysicalDeviceProxy;
class AxisAccumulator;

typedef Qt3DCore::QHandle<KeyboardDevice, 8> HKeyboardDevice;
typedef Qt3DCore::QHandle<KeyboardHandler, 16> HKeyboardHandler;
typedef Qt3DCore::QHandle<MouseHandler, 16> HMouseHandler;
typedef Qt3DCore::QHandle<MouseDevice, 8> HMouseDevice;
typedef Qt3DCore::QHandle<Axis, 16> HAxis;
typedef Qt3DCore::QHandle<AxisActionHandler, 16> HAxisActionHandler;
typedef Qt3DCore::QHandle<AxisSetting, 16> HAxisSetting;
typedef Qt3DCore::QHandle<Action, 16> HAction;
typedef Qt3DCore::QHandle<AbstractAxisInput, 16> HAxisInput;
typedef Qt3DCore::QHandle<ActionInput, 16> HActionInput;
typedef Qt3DCore::QHandle<InputSequence, 16> HInputSequence;
typedef Qt3DCore::QHandle<InputChord, 16> HInputChord;
typedef Qt3DCore::QHandle<LogicalDevice, 16> HLogicalDevice;
typedef Qt3DCore::QHandle<GenericDeviceBackendNode, 8> HGenericDeviceBackendNode;
typedef Qt3DCore::QHandle<PhysicalDeviceProxy, 16> HPhysicalDeviceProxy;
typedef Qt3DCore::QHandle<AxisAccumulator, 16> HAxisAccumulator;

} // namespace Input
} // namespace Qt3DInput

QT_END_NAMESPACE

#endif // QT3DINPUT_INPUT_HANDLE_TYPES_P_H
