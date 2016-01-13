/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

    qmlRegisterUncreatableType<QQuickAnimator>("QtQuick", 2, 2, "Animator", QQuickAbstractAnimation::tr("Animator is an abstract class"));
    qmlRegisterType<QQuickXAnimator>("QtQuick", 2, 2, "XAnimator");
    qmlRegisterType<QQuickYAnimator>("QtQuick", 2, 2, "YAnimator");
    qmlRegisterType<QQuickScaleAnimator>("QtQuick", 2, 2, "ScaleAnimator");
    qmlRegisterType<QQuickRotationAnimator>("QtQuick", 2, 2, "RotationAnimator");
    qmlRegisterType<QQuickOpacityAnimator>("QtQuick", 2, 2, "OpacityAnimator");
    qmlRegisterType<QQuickUniformAnimator>("QtQuick", 2, 2, "UniformAnimator");

    qmlRegisterType<QQuickStateOperation>();

    qmlRegisterCustomType<QQuickPropertyChanges>("QtQuick",2,0,"PropertyChanges", new QQuickPropertyChangesParser);

    qRegisterMetaType<QKeySequence::StandardKey>();
    qmlRegisterUncreatableType<QKeySequence, 2>("QtQuick", 2, 2, "StandardKey", QStringLiteral("Cannot create an instance of StandardKey."));

    qmlRegisterType<QQuickFontMetrics>("QtQuick", 2, 4, "FontMetrics");
    qmlRegisterType<QQuickTextMetrics>("QtQuick", 2, 4, "TextMetrics");
}
