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

#include "qquickwindowmodule_p.h"
#include "qquickwindowattached_p.h"
#include "qquickscreen_p.h"
#include "qquickview_p.h"
#include <QtQuick/QQuickWindow>
#include <QtCore/QCoreApplication>
#include <QtQml/QQmlEngine>

#include <private/qguiapplication_p.h>
#include <private/qqmlengine_p.h>
#include <qpa/qplatformintegration.h>

QT_BEGIN_NAMESPACE

class QQuickWindowQmlImpl : public QQuickWindow, public QQmlParserStatus
{
    Q_INTERFACES(QQmlParserStatus)
    Q_OBJECT

    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(Visibility visibility READ visibility WRITE setVisibility NOTIFY visibilityChanged)

public:
    QQuickWindowQmlImpl(QWindow *parent = 0)
        : QQuickWindow(parent)
        , m_complete(false)
        , m_visible(isVisible())
        , m_visibility(AutomaticVisibility)
    {
        connect(this, &QWindow::visibleChanged, this, &QQuickWindowQmlImpl::visibleChanged);
        connect(this, &QWindow::visibilityChanged, this, &QQuickWindowQmlImpl::visibilityChanged);
    }

    void setVisible(bool visible) {
        if (!m_complete)
            m_visible = visible;
        else if (!transientParent() || transientParent()->isVisible())
            QQuickWindow::setVisible(visible);
    }

    void setVisibility(Visibility visibility)
    {
        if (!m_complete)
            m_visibility = visibility;
        else
            QQuickWindow::setVisibility(visibility);
    }

    static QQuickWindowAttached *qmlAttachedProperties(QObject *object)
    {
        return new QQuickWindowAttached(object);
    }

Q_SIGNALS:
    void visibleChanged(bool arg);
    void visibilityChanged(QWindow::Visibility visibility);

protected:
    void classBegin() {
        QQmlEngine* e = qmlEngine(this);
        //Give QQuickView behavior when created from QML with QQmlApplicationEngine
        if (QCoreApplication::instance()->property("__qml_using_qqmlapplicationengine") == QVariant(true)) {
            if (e && !e->incubationController())
                e->setIncubationController(incubationController());
        }
        Q_ASSERT(e);
        {
            QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(e);
            QV4::Scope scope(v4);
            QV4::ScopedObject v(scope, QQuickRootItemMarker::create(e, this));
            rootItemMarker = v;
        }
    }

    void componentComplete() {
        m_complete = true;
        if (transientParent() && !transientParent()->isVisible()) {
            connect(transientParent(), &QQuickWindow::visibleChanged, this,
                    &QQuickWindowQmlImpl::setWindowVisibility, Qt::QueuedConnection);
        } else {
            setWindowVisibility();
        }
    }

private Q_SLOTS:
    void setWindowVisibility()
    {
        if (transientParent() && !transientParent()->isVisible())
            return;

        if (sender()) {
            disconnect(transientParent(), &QWindow::visibleChanged, this,
                       &QQuickWindowQmlImpl::setWindowVisibility);
        }

        // We have deferred window creation until we have the full picture of what
        // the user wanted in terms of window state, geometry, visibility, etc.

        if ((m_visibility == Hidden && m_visible) || (m_visibility > AutomaticVisibility && !m_visible)) {
            QQmlData *data = QQmlData::get(this);
            Q_ASSERT(data && data->context);

            QQmlError error;
            error.setObject(this);

            const QQmlContextData* urlContext = data->context;
            while (urlContext && urlContext->url.isEmpty())
                urlContext = urlContext->parent;
            error.setUrl(urlContext ? urlContext->url : QUrl());

            QString objectId = data->context->findObjectId(this);
            if (!objectId.isEmpty())
                error.setDescription(QCoreApplication::translate("QQuickWindowQmlImpl",
                    "Conflicting properties 'visible' and 'visibility' for Window '%1'").arg(objectId));
            else
                error.setDescription(QCoreApplication::translate("QQuickWindowQmlImpl",
                    "Conflicting properties 'visible' and 'visibility'"));

            QQmlEnginePrivate::get(data->context->engine)->warning(error);
        }

        if (m_visibility == AutomaticVisibility) {
            setWindowState(QGuiApplicationPrivate::platformIntegration()->defaultWindowState(flags()));
            setVisible(m_visible);
        } else {
            setVisibility(m_visibility);
        }
    }

private:
    bool m_complete;
    bool m_visible;
    Visibility m_visibility;
    QV4::PersistentValue rootItemMarker;
};

void QQuickWindowModule::defineModule()
{
    const char uri[] = "QtQuick.Window";

    qmlRegisterType<QQuickWindow>(uri, 2, 0, "Window");
    qmlRegisterRevision<QWindow,1>(uri, 2, 1);
    qmlRegisterRevision<QWindow,2>(uri, 2, 2);
    qmlRegisterRevision<QQuickWindow,1>(uri, 2, 1);//Type moved to a subclass, but also has new members
    qmlRegisterRevision<QQuickWindow,2>(uri, 2, 2);
    qmlRegisterType<QQuickWindowQmlImpl>(uri, 2, 1, "Window");
    qmlRegisterType<QQuickWindowQmlImpl,1>(uri, 2, 2, "Window");
    qmlRegisterUncreatableType<QQuickScreen>(uri, 2, 0, "Screen", QStringLiteral("Screen can only be used via the attached property."));
}

#include "qquickwindowmodule.moc"

QT_END_NAMESPACE

QML_DECLARE_TYPEINFO(QQuickWindowQmlImpl, QML_HAS_ATTACHED_PROPERTIES)
