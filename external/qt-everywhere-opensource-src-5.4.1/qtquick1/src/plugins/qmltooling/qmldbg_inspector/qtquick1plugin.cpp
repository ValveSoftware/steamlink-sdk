/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "qtquick1plugin.h"
#include "qdeclarativeviewinspector.h"

#include <QtCore/qplugin.h>
#include <QtDeclarative/private/qdeclarativeinspectorservice_p.h>
#include <QtDeclarative/qdeclarativeview.h>

namespace QmlJSDebugger {
namespace QtQuick1 {

QtQuick1Plugin::QtQuick1Plugin() :
    m_inspector(0)
{
}

QtQuick1Plugin::~QtQuick1Plugin()
{
    delete m_inspector;
}

bool QtQuick1Plugin::canHandleView(QObject *view)
{
    return qobject_cast<QDeclarativeView*>(view);
}

void QtQuick1Plugin::activate()
{
    QDeclarativeInspectorService *service = QDeclarativeInspectorService::instance();
    QList<QDeclarativeView*> views = service->views();
    if (views.isEmpty())
        return;

    // TODO: Support multiple views
    QDeclarativeView *view = service->views().at(0);
    m_inspector = new QDeclarativeViewInspector(view, view);
}

void QtQuick1Plugin::deactivate()
{
    delete m_inspector;
}

void QtQuick1Plugin::clientMessage(const QByteArray &message)
{
    if (m_inspector)
        m_inspector->handleMessage(message);
}

} // namespace QtQuick1
} // namespace QmlJSDebugger
