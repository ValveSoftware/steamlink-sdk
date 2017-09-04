/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtquickscene2dplugin.h"

#include <Qt3DQuickScene2D/qscene2d.h>
#include <private/qrenderaspect_p.h>
#include <Qt3DQuickScene2D/private/qt3dquick3dscene2d_p.h>
#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

static void initScene2dPlugin()
{
    Qt3DRender::QRenderAspectPrivate::configurePlugin("scene2d");
}

Q_COREAPP_STARTUP_FUNCTION(initScene2dPlugin)

void QtQuickScene2DPlugin::registerTypes(const char *uri)
{
    qmlRegisterExtendedType<Qt3DRender::Quick::QScene2D, Qt3DRender::Render::Quick::QQuick3DScene2D>(uri, 2, 9, "Scene2D");
}

QT_END_NAMESPACE
