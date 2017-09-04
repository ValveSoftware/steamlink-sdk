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

#include "qqmlinspectorservicefactory.h"
#include "globalinspector.h"
#include "qquickwindowinspector.h"

#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

class QQmlInspectorServiceImpl : public QQmlInspectorService
{
    Q_OBJECT
public:
    QQmlInspectorServiceImpl(QObject *parent = 0);

    void addWindow(QQuickWindow *window) Q_DECL_OVERRIDE;
    void setParentWindow(QQuickWindow *window, QWindow *parent) Q_DECL_OVERRIDE;
    void removeWindow(QQuickWindow *window) Q_DECL_OVERRIDE;

signals:
    void scheduleMessage(const QByteArray &message);

protected:
    virtual void messageReceived(const QByteArray &) Q_DECL_OVERRIDE;

private:
    friend class QQmlInspectorServiceFactory;

    QmlJSDebugger::GlobalInspector *checkInspector();
    QmlJSDebugger::GlobalInspector *m_globalInspector;
    QHash<QQuickWindow *, QWindow *> m_waitingWindows;

    void messageFromClient(const QByteArray &message);
};

QQmlInspectorServiceImpl::QQmlInspectorServiceImpl(QObject *parent):
    QQmlInspectorService(1, parent), m_globalInspector(0)
{
    connect(this, &QQmlInspectorServiceImpl::scheduleMessage,
            this, &QQmlInspectorServiceImpl::messageFromClient, Qt::QueuedConnection);
}

QmlJSDebugger::GlobalInspector *QQmlInspectorServiceImpl::checkInspector()
{
    if (state() == Enabled) {
        if (!m_globalInspector) {
            m_globalInspector = new QmlJSDebugger::GlobalInspector(this);
            connect(m_globalInspector, &QmlJSDebugger::GlobalInspector::messageToClient,
                    this, &QQmlDebugService::messageToClient);
            for (QHash<QQuickWindow *, QWindow *>::ConstIterator i = m_waitingWindows.constBegin();
                 i != m_waitingWindows.constEnd(); ++i) {
                m_globalInspector->addWindow(i.key());
                if (i.value() != 0)
                    m_globalInspector->setParentWindow(i.key(), i.value());
            }
            m_waitingWindows.clear();
        }
    } else if (m_globalInspector) {
        delete m_globalInspector;
        m_globalInspector = 0;
    }
    return m_globalInspector;
}

void QQmlInspectorServiceImpl::addWindow(QQuickWindow *window)
{
    if (QmlJSDebugger::GlobalInspector *inspector = checkInspector())
        inspector->addWindow(window);
    else
        m_waitingWindows.insert(window, 0);
}

void QQmlInspectorServiceImpl::removeWindow(QQuickWindow *window)
{
    if (QmlJSDebugger::GlobalInspector *inspector = checkInspector())
        inspector->removeWindow(window);
    else
        m_waitingWindows.remove(window);
}

void QQmlInspectorServiceImpl::setParentWindow(QQuickWindow *window, QWindow *parent)
{
    if (QmlJSDebugger::GlobalInspector *inspector = checkInspector())
        inspector->setParentWindow(window, parent);
    else
        m_waitingWindows[window] = parent;
}

void QQmlInspectorServiceImpl::messageReceived(const QByteArray &message)
{
    // Move the message to the right thread via queued signal
    emit scheduleMessage(message);
}

void QQmlInspectorServiceImpl::messageFromClient(const QByteArray &message)
{
    if (QmlJSDebugger::GlobalInspector *inspector = checkInspector())
        inspector->processMessage(message);
}

QQmlDebugService *QQmlInspectorServiceFactory::create(const QString &key)
{
    return key == QQmlInspectorServiceImpl::s_key ? new QQmlInspectorServiceImpl(this) : 0;
}

QT_END_NAMESPACE

#include "qqmlinspectorservice.moc"
