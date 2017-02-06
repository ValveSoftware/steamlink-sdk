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

#include "qsgsoftwareadaptation_p.h"
#include "qsgsoftwarecontext_p.h"
#include "qsgsoftwarerenderloop_p.h"
#include "qsgsoftwarethreadedrenderloop_p.h"

#include <private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

QT_BEGIN_NAMESPACE

QSGSoftwareAdaptation::QSGSoftwareAdaptation(QObject *parent)
    : QSGContextPlugin(parent)
{
}

QStringList QSGSoftwareAdaptation::keys() const
{
    return QStringList() << QLatin1String("software") << QLatin1String("softwarecontext");
}

QSGContext *QSGSoftwareAdaptation::create(const QString &) const
{
    if (!instance)
        instance = new QSGSoftwareContext();
    return instance;
}

QSGContextFactoryInterface::Flags QSGSoftwareAdaptation::flags(const QString &) const
{
    // Claim we support adaptable shader effects, then return null for the
    // shader effect node. The result is shader effects not being rendered,
    // with the application working fine in all other respects.
    return QSGContextFactoryInterface::SupportsShaderEffectNode;
}

QSGRenderLoop *QSGSoftwareAdaptation::createWindowManager()
{
    static bool threaded = false;
    static bool envChecked = false;
    if (!envChecked) {
        envChecked = true;
        threaded = qgetenv("QSG_RENDER_LOOP") == QByteArrayLiteral("threaded");
    }

    if (threaded)
        return new QSGSoftwareThreadedRenderLoop;

    return new QSGSoftwareRenderLoop();
}

QSGSoftwareContext *QSGSoftwareAdaptation::instance = 0;

QT_END_NAMESPACE
