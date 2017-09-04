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

#ifndef QQMLPROFILERSERVICE_P_H
#define QQMLPROFILERSERVICE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qqmlconfigurabledebugservice.h"
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmlprofilerdefinitions_p.h>
#include <private/qqmlabstractprofileradapter_p.h>
#include <private/qqmlboundsignal_p.h>

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qvector.h>
#include <QtCore/qstringbuilder.h>
#include <QtCore/qwaitcondition.h>
#include <QtCore/qtimer.h>

#include <limits>

QT_BEGIN_NAMESPACE

class QUrl;

class QQmlProfilerServiceImpl :
        public QQmlConfigurableDebugService<QQmlProfilerService>,
        public QQmlProfilerDefinitions
{
    Q_OBJECT
public:

    void engineAboutToBeAdded(QJSEngine *engine) Q_DECL_OVERRIDE;
    void engineAboutToBeRemoved(QJSEngine *engine) Q_DECL_OVERRIDE;
    void engineAdded(QJSEngine *engine) Q_DECL_OVERRIDE;
    void engineRemoved(QJSEngine *engine) Q_DECL_OVERRIDE;

    void addGlobalProfiler(QQmlAbstractProfilerAdapter *profiler) Q_DECL_OVERRIDE;
    void removeGlobalProfiler(QQmlAbstractProfilerAdapter *profiler) Q_DECL_OVERRIDE;

    void startProfiling(QJSEngine *engine,
                        quint64 features = std::numeric_limits<quint64>::max()) Q_DECL_OVERRIDE;
    void stopProfiling(QJSEngine *engine) Q_DECL_OVERRIDE;

    QQmlProfilerServiceImpl(QObject *parent = 0);
    ~QQmlProfilerServiceImpl() Q_DECL_OVERRIDE;

    void dataReady(QQmlAbstractProfilerAdapter *profiler) Q_DECL_OVERRIDE;

signals:
    void startFlushTimer();
    void stopFlushTimer();

protected:
    virtual void stateAboutToBeChanged(State state) Q_DECL_OVERRIDE;
    virtual void messageReceived(const QByteArray &) Q_DECL_OVERRIDE;

private:
    friend class QQmlProfilerServiceFactory;

    void sendMessages();
    void addEngineProfiler(QQmlAbstractProfilerAdapter *profiler, QJSEngine *engine);
    void removeProfilerFromStartTimes(const QQmlAbstractProfilerAdapter *profiler);
    void flush();

    QElapsedTimer m_timer;
    QTimer m_flushTimer;
    bool m_waitingForStop;
    bool m_useMessageTypes;

    QList<QQmlAbstractProfilerAdapter *> m_globalProfilers;
    QMultiHash<QJSEngine *, QQmlAbstractProfilerAdapter *> m_engineProfilers;
    QList<QJSEngine *> m_stoppingEngines;
    QMultiMap<qint64, QQmlAbstractProfilerAdapter *> m_startTimes;
};

QT_END_NAMESPACE

#endif // QQMLPROFILERSERVICE_P_H

