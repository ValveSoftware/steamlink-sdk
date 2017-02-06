/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickutilmodule_p.h"
#include "qquickanimation_p.h"
#include "qquickanimation_p_p.h"
#include "qquickbehavior_p.h"
#include "qquicksmoothedanimation_p.h"
#include "qquickfontloader_p.h"
#include "qquickfontmetrics_p.h"
#include "qquickpropertychanges_p.h"
#include "qquickspringanimation_p.h"
#include "qquickstategroup_p.h"
#include "qquickstatechangescript_p.h"
#include "qquickstate_p.h"
#include "qquickstate_p_p.h"
#include "qquicksystempalette_p.h"
#include "qquicktextmetrics_p.h"
#include "qquicktransition_p.h"
#include "qquickanimator_p.h"
#include "qquickshortcut_p.h"
#include "qquickvalidator_p.h"
#include <qqmlinfo.h>
#include <private/qqmltypenotavailable_p.h>
#include <private/qquickanimationcontroller_p.h>
#include <QtCore/qcoreapplication.h>
#include <QtGui/QInputMethod>
#include <QtGui/QKeySequence>

Q_DECLARE_METATYPE(QKeySequence::StandardKey)

void QQuickUtilModule::defineModule()
{
#ifndef QT_NO_IM
    qmlRegisterUncreatableType<QInputMethod>("QtQuick",2,0,"InputMethod",
                                             QInputMethod::tr("InputMethod is an abstract class"));
#endif
    qmlRegisterUncreatableType<QQuickAbstractAnimation>("QtQuick",2,0,"Animation",QQuickAbstractAnimation::tr("Animation is an abstract class"));

    qmlRegisterType<QQuickBehavior>("QtQuick",2,0,"Behavior");
    qmlRegisterType<QQuickColorAnimation>("QtQuick",2,0,"ColorAnimation");
    qmlRegisterType<QQuickSmoothedAnimation>("QtQuick",2,0,"SmoothedAnimation");
    qmlRegisterType<QQuickFontLoader>("QtQuick",2,0,"FontLoader");
    qmlRegisterType<QQuickNumberAnimation>("QtQuick",2,0,"NumberAnimation");
    qmlRegisterType<QQuickParallelAnimation>("QtQuick",2,0,"ParallelAnimation");
    qmlRegisterType<QQuickPauseAnimation>("QtQuick",2,0,"PauseAnimation");
    qmlRegisterType<QQuickPropertyAction>("QtQuick",2,0,"PropertyAction");
    qmlRegisterType<QQuickPropertyAnimation>("QtQuick",2,0,"PropertyAnimation");
    qmlRegisterType<QQuickRotationAnimation>("QtQuick",2,0,"RotationAnimation");
    qmlRegisterType<QQuickScriptAction>("QtQuick",2,0,"ScriptAction");
    qmlRegisterType<QQuickSequentialAnimation>("QtQuick",2,0,"SequentialAnimation");
    qmlRegisterType<QQuickSpringAnimation>("QtQuick",2,0,"SpringAnimation");
    qmlRegisterType<QQuickAnimationController>("QtQuick",2,0,"AnimationController");
    qmlRegisterType<QQuickStateChangeScript>("QtQuick",2,0,"StateChangeScript");
    qmlRegisterType<QQuickStateGroup>("QtQuick",2,0,"StateGroup");
    qmlRegisterType<QQuickState>("QtQuick",2,0,"State");
    qmlRegisterType<QQuickSystemPalette>("QtQuick",2,0,"SystemPalette");
    qmlRegisterType<QQuickTransition>("QtQuick",2,0,"Transition");
    qmlRegisterType<QQuickVector3dAnimation>("QtQuick",2,0,"Vector3dAnimation");

#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QValidator>();
    qmlRegisterType<QQuickIntValidator>("QtQuick",2,0,"IntValidator");
    qmlRegisterType<QQuickDoubleValidator>("QtQuick",2,0,"DoubleValidator");
    qmlRegisterType<QRegExpValidator>("QtQuick",2,0,"RegExpValidator");
#endif

    qmlRegisterUncreatableType<QQuickAnimator>("QtQuick", 2, 2, "Animator", QQuickAbstractAnimation::tr("Animator is an abstract class"));
    qmlRegisterType<QQuickXAnimator>("QtQuick", 2, 2, "XAnimator");
    qmlRegisterType<QQuickYAnimator>("QtQuick", 2, 2, "YAnimator");
    qmlRegisterType<QQuickScaleAnimator>("QtQuick", 2, 2, "ScaleAnimator");
    qmlRegisterType<QQuickRotationAnimator>("QtQuick", 2, 2, "RotationAnimator");
    qmlRegisterType<QQuickOpacityAnimator>("QtQuick", 2, 2, "OpacityAnimator");
#if QT_CONFIG(quick_shadereffect) && QT_CONFIG(opengl)
    qmlRegisterType<QQuickUniformAnimator>("QtQuick", 2, 2, "UniformAnimator");
#endif
    qmlRegisterType<QQuickStateOperation>();

    qmlRegisterCustomType<QQuickPropertyChanges>("QtQuick",2,0,"PropertyChanges", new QQuickPropertyChangesParser);

    qRegisterMetaType<QKeySequence::StandardKey>();
    qmlRegisterUncreatableType<QKeySequence, 2>("QtQuick", 2, 2, "StandardKey", QStringLiteral("Cannot create an instance of StandardKey."));

    qmlRegisterType<QQuickFontMetrics>("QtQuick", 2, 4, "FontMetrics");
    qmlRegisterType<QQuickTextMetrics>("QtQuick", 2, 4, "TextMetrics");

    qmlRegisterType<QQuickShortcut>("QtQuick", 2, 5, "Shortcut");

    qmlRegisterType<QQuickShortcut,1>("QtQuick", 2, 6, "Shortcut");
}
