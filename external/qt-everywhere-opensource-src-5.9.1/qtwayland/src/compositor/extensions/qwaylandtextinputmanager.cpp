/****************************************************************************
**
** Copyright (C) 2017-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#include "qwaylandtextinputmanager.h"
#include "qwaylandtextinputmanager_p.h"

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>

#include "qwaylandtextinput.h"

QT_BEGIN_NAMESPACE

QWaylandTextInputManagerPrivate::QWaylandTextInputManagerPrivate()
    : QWaylandCompositorExtensionPrivate()
    , QtWaylandServer::zwp_text_input_manager_v2()
{
}

void QWaylandTextInputManagerPrivate::zwp_text_input_manager_v2_get_text_input(Resource *resource, uint32_t id, struct ::wl_resource *seatResource)
{
    Q_Q(QWaylandTextInputManager);
    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(q->extensionContainer());
    QWaylandSeat *seat = QWaylandSeat::fromSeatResource(seatResource);
    QWaylandTextInput *textInput = QWaylandTextInput::findIn(seat);
    if (!textInput) {
        textInput = new QWaylandTextInput(seat, compositor);
    }
    textInput->add(resource->client(), id, wl_resource_get_version(resource->handle));
}

QWaylandTextInputManager::QWaylandTextInputManager()
    : QWaylandCompositorExtensionTemplate<QWaylandTextInputManager>(*new QWaylandTextInputManagerPrivate)
{
}

QWaylandTextInputManager::QWaylandTextInputManager(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate<QWaylandTextInputManager>(compositor, *new QWaylandTextInputManagerPrivate)
{
}

void QWaylandTextInputManager::initialize()
{
    Q_D(QWaylandTextInputManager);

    QWaylandCompositorExtensionTemplate::initialize();
    QWaylandCompositor *compositor = static_cast<QWaylandCompositor *>(extensionContainer());
    if (!compositor) {
        qWarning() << "Failed to find QWaylandCompositor when initializing QWaylandTextInputManager";
        return;
    }
    d->init(compositor->display(), 1);
}

const wl_interface *QWaylandTextInputManager::interface()
{
    return QWaylandTextInputManagerPrivate::interface();
}

QByteArray QWaylandTextInputManager::interfaceName()
{
    return QWaylandTextInputManagerPrivate::interfaceName();
}

QT_END_NAMESPACE
