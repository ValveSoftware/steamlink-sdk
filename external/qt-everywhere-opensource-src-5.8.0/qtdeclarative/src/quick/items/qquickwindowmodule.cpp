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

#include "qquickwindowmodule_p.h"
#include "qquickwindowattached_p.h"
#include "qquickscreen_p.h"
#include "qquickview_p.h"
#include <QtQuick/QQuickWindow>
#include <QtCore/QCoreApplication>
#include <QtQml/QQmlEngine>

#include <private/qguiapplication_p.h>
#include <private/qqmlengine_p.h>
#include <private/qv4qobjectwrapper_p.h>
#include <qpa/qplatformintegration.h>

QT_BEGIN_NAMESPACE

class QQuickWindowQmlImplPrivate : public QQuickWindowPrivate
{
public:
    QQuickWindowQmlImplPrivate()
        : complete(false)
        , visible(false)
        , visibility(QQuickWindow::AutomaticVisibility)
    {
    }

    bool complete;
    bool visible;
    QQuickWindow::Visibility visibility;
    QV4::PersistentValue rootItemMarker;
};

QQuickWindowQmlImpl::QQuickWindowQmlImpl(QWindow *parent)
    : QQuickWindow(*(new QQuickWindowQmlImplPrivate), parent)
{
    connect(this, &QWindow::visibleChanged, this, &QQuickWindowQmlImpl::visibleChanged);
    connect(this, &QWindow::visibilityChanged, this, &QQuickWindowQmlImpl::visibilityChanged);
}

void QQuickWindowQmlImpl::setVisible(bool visible)
{
    Q_D(QQuickWindowQmlImpl);
    d->visible = visible;
    if (d->complete && (!transientParent() || transientParent()->isVisible()))
        QQuickWindow::setVisible(visible);
}

void QQuickWindowQmlImpl::setVisibility(Visibility visibility)
{
    Q_D(QQuickWindowQmlImpl);
    d->visibility = visibility;
    if (d->complete)
        QQuickWindow::setVisibility(visibility);
}

QQuickWindowAttached *QQuickWindowQmlImpl::qmlAttachedProperties(QObject *object)
{
    return new QQuickWindowAttached(object);
}

void QQuickWindowQmlImpl::classBegin()
{
    Q_D(QQuickWindowQmlImpl);
    QQmlEngine* e = qmlEngine(this);
    //Give QQuickView behavior when created from QML with QQmlApplicationEngine
    if (QCoreApplication::instance()->property("__qml_using_qqmlapplicationengine") == QVariant(true)) {
        if (e && !e->incubationController())
            e->setIncubationController(incubationController());
    }
    {
        // The content item has CppOwnership policy (set in QQuickWindow). Ensure the presence of a JS
        // wrapper so that the garbage collector can see the policy.
        QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(e);
        QV4::QObjectWrapper::wrap(v4, d->contentItem);
    }
}

void QQuickWindowQmlImpl::componentComplete()
{
    Q_D(QQuickWindowQmlImpl);
    d->complete = true;
    if (transientParent() && !transientParent()->isVisible()) {
        connect(transientParent(), &QQuickWindow::visibleChanged, this,
                &QQuickWindowQmlImpl::setWindowVisibility, Qt::QueuedConnection);
    } else {
        setWindowVisibility();
    }
}

void QQuickWindowQmlImpl::setWindowVisibility()
{
    Q_D(QQuickWindowQmlImpl);
    if (transientParent() && !transientParent()->isVisible())
        return;

    if (sender()) {
        disconnect(transientParent(), &QWindow::visibleChanged, this,
                   &QQuickWindowQmlImpl::setWindowVisibility);
    }

    // We have deferred window creation until we have the full picture of what
    // the user wanted in terms of window state, geometry, visibility, etc.

    if ((d->visibility == Hidden && d->visible) || (d->visibility > AutomaticVisibility && !d->visible)) {
        QQmlData *data = QQmlData::get(this);
        Q_ASSERT(data && data->context);

        QQmlError error;
        error.setObject(this);

        const QQmlContextData* urlContext = data->context;
        while (urlContext && urlContext->url().isEmpty())
            urlContext = urlContext->parent;
        error.setUrl(urlContext ? urlContext->url() : QUrl());

        QString objectId = data->context->findObjectId(this);
        if (!objectId.isEmpty())
            error.setDescription(QCoreApplication::translate("QQuickWindowQmlImpl",
                "Conflicting properties 'visible' and 'visibility' for Window '%1'").arg(objectId));
        else
            error.setDescription(QCoreApplication::translate("QQuickWindowQmlImpl",
                "Conflicting properties 'visible' and 'visibility'"));

        QQmlEnginePrivate::get(data->context->engine)->warning(error);
    }

    if (d->visibility == AutomaticVisibility) {
        setWindowState(QGuiApplicationPrivate::platformIntegration()->defaultWindowState(flags()));
        setVisible(d->visible);
    } else {
        setVisibility(d->visibility);
    }
}

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

QT_END_NAMESPACE

QML_DECLARE_TYPEINFO(QQuickWindowQmlImpl, QML_HAS_ATTACHED_PROPERTIES)
