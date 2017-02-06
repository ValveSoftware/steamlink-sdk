/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQML_NATIVE_DEBUG_SERVICE_H
#define QQML_NATIVE_DEBUG_SERVICE_H

#include <private/qqmldebugconnector_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv8engine_p.h>
#include <private/qv4engine_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv4script_p.h>
#include <private/qv4string_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4identifier_p.h>
#include <private/qv4runtime_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>

#include <QtCore/qjsonarray.h>

#include <qqmlengine.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>
#include <QPointer>

QT_BEGIN_NAMESPACE

class NativeDebugger;
class BreakPointHandler;
class QQmlDebuggerServiceFactory;

class QQmlNativeDebugServiceImpl : public QQmlNativeDebugService
{
public:
    QQmlNativeDebugServiceImpl(QObject *parent);

    ~QQmlNativeDebugServiceImpl() Q_DECL_OVERRIDE;

    void engineAboutToBeAdded(QJSEngine *engine) Q_DECL_OVERRIDE;
    void engineAboutToBeRemoved(QJSEngine *engine) Q_DECL_OVERRIDE;

    void stateAboutToBeChanged(State state) Q_DECL_OVERRIDE;

    void messageReceived(const QByteArray &message) Q_DECL_OVERRIDE;

    void emitAsynchronousMessageToClient(const QJsonObject &message);

private:
    friend class QQmlDebuggerServiceFactory;
    friend class NativeDebugger;

    QList<QPointer<NativeDebugger> > m_debuggers;
    BreakPointHandler *m_breakHandler;
};

QT_END_NAMESPACE

#endif // QQML_NATIVE_DEBUG_SERVICE_H
