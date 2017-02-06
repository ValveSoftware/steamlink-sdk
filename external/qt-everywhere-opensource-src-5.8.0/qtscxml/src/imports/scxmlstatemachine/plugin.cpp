/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "statemachineloader_p.h"
#include "eventconnection_p.h"
#include "qscxmlevent.h"
#include "statemachineextended_p.h"
#include "invokedservices_p.h"

#include <QQmlExtensionPlugin>
#include <qqml.h>

QT_BEGIN_NAMESPACE

class QScxmlStateMachinePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.Scxml/1.0")

public:
    void registerTypes(const char *uri)
    {
        // @uri QtScxml
        Q_ASSERT(uri == QStringLiteral("QtScxml"));

        int major = 5;
        int minor = 8;
        // Do not rely on RegisterMethodArgumentMetaType meta-call to register the QScxmlEvent type.
        // This registration is required for the receiving end of the signal emission that carries
        // parameters of this type to be able to treat them correctly as a gadget. This is because the
        // receiving end of the signal is a generic method in the QML engine, at which point it's too late
        // to do a meta-type registration.
        static const int qScxmlEventMetaTypeId = qMetaTypeId<QScxmlEvent>();
        Q_UNUSED(qScxmlEventMetaTypeId)
        qmlRegisterType<QScxmlStateMachineLoader>(uri, major, minor, "StateMachineLoader");
        qmlRegisterType<QScxmlEventConnection>(uri, major, minor, "EventConnection");
        qmlRegisterType<QScxmlInvokedServices>(uri, major, minor, "InvokedServices");
        qmlRegisterExtendedUncreatableType<QScxmlStateMachine, QScxmlStateMachineExtended>(
                    uri, major, minor, "StateMachine", "Only created through derived types");
        qmlProtectModule(uri, 1);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
