/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "abstractviewinspector.h"

#include "abstracttool.h"

#include <QtCore/QDebug>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtCore/private/qabstractanimation_p.h>
#include <QtQml/private/qqmlinspectorservice_p.h>
#include <QtQml/private/qqmlcontext_p.h>

#include <QtGui/QMouseEvent>
#include <QtGui/QTouchEvent>

//INSPECTOR SERVICE PROTOCOL
// <HEADER><COMMAND><DATA>
// <HEADER> : <type{request, response, event}><requestId/eventId>[<response_success_bool>]
// <COMMAND> : {"enable", "disable", "select", "reload", "setAnimationSpeed",
//              "showAppOnTop", "createObject", "destroyObject", "moveObject",
//              "clearCache"}
// <DATA> : select: <debugIds_int_list>
//          reload: <hash<changed_filename_string, filecontents_bytearray>>
//          setAnimationSpeed: <speed_real>
//          showAppOnTop: <set_bool>
//          createObject: <qml_string><parentId_int><imports_string_list><filename_string>
//          destroyObject: <debugId_int>
//          moveObject: <debugId_int><newParentId_int>
//          clearCache: void
// Response for "destroyObject" carries the <debugId_int> of the destroyed object.

const char REQUEST[] = "request";
const char RESPONSE[] = "response";
const char EVENT[] = "event";
const char ENABLE[] = "enable";
const char DISABLE[] = "disable";
const char SELECT[] = "select";
const char RELOAD[] = "reload";
const char SET_ANIMATION_SPEED[] = "setAnimationSpeed";
const char SHOW_APP_ON_TOP[] = "showAppOnTop";
const char CREATE_OBJECT[] = "createObject";
const char DESTROY_OBJECT[] = "destroyObject";
const char MOVE_OBJECT[] = "moveObject";
const char CLEAR_CACHE[] = "clearCache";

namespace QmlJSDebugger {


AbstractViewInspector::AbstractViewInspector(QObject *parent) :
    QObject(parent),
    m_enabled(false),
    m_debugService(QQmlInspectorService::instance()),
    m_eventId(0),
    m_reloadEventId(-1)
{
}

void AbstractViewInspector::createQmlObject(const QString &qml, QObject *parent,
                                            const QStringList &importList,
                                            const QString &filename)
{
    if (!parent)
        return;

    QString imports;
    foreach (const QString &s, importList) {
        imports += s;
        imports += QLatin1Char('\n');
    }

    QQmlContext *parentContext = declarativeEngine()->contextForObject(parent);
    QQmlComponent component(declarativeEngine());
    QByteArray constructedQml = QString(imports + qml).toLatin1();

    component.setData(constructedQml, QUrl::fromLocalFile(filename));
    QObject *newObject = component.create(parentContext);
    if (newObject)
        reparentQmlObject(newObject, parent);
}

void AbstractViewInspector::clearComponentCache()
{
    declarativeEngine()->clearComponentCache();
}

void AbstractViewInspector::setEnabled(bool value)
{
    if (m_enabled == value)
        return;

    m_enabled = value;
    foreach (AbstractTool *tool, m_tools)
        tool->enable(m_enabled);
}

void AbstractViewInspector::setAnimationSpeed(qreal slowDownFactor)
{
    QUnifiedTimer::instance()->setSlowModeEnabled(slowDownFactor != 1.0);
    QUnifiedTimer::instance()->setSlowdownFactor(slowDownFactor);
}

bool AbstractViewInspector::eventFilter(QObject *obj, QEvent *event)
{
    if (!enabled())
        return QObject::eventFilter(obj, event);

    switch (event->type()) {
    case QEvent::Leave:
        if (leaveEvent(event))
            return true;
        break;
    case QEvent::MouseButtonPress:
        if (mousePressEvent(static_cast<QMouseEvent*>(event)))
            return true;
        break;
    case QEvent::MouseMove:
        if (mouseMoveEvent(static_cast<QMouseEvent*>(event)))
            return true;
        break;
    case QEvent::MouseButtonRelease:
        if (mouseReleaseEvent(static_cast<QMouseEvent*>(event)))
            return true;
        break;
    case QEvent::KeyPress:
        if (keyPressEvent(static_cast<QKeyEvent*>(event)))
            return true;
        break;
    case QEvent::KeyRelease:
        if (keyReleaseEvent(static_cast<QKeyEvent*>(event)))
            return true;
        break;
    case QEvent::MouseButtonDblClick:
        if (mouseDoubleClickEvent(static_cast<QMouseEvent*>(event)))
            return true;
        break;
#ifndef QT_NO_WHEELEVENT
    case QEvent::Wheel:
        if (wheelEvent(static_cast<QWheelEvent*>(event)))
            return true;
        break;
#endif
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
        if (touchEvent(static_cast<QTouchEvent*>(event)))
            return true;
        break;
    default:
        break;
    }

    return QObject::eventFilter(obj, event);
}

bool AbstractViewInspector::leaveEvent(QEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->leaveEvent(event);
    return true;
}

bool AbstractViewInspector::mousePressEvent(QMouseEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->mousePressEvent(event);
    return true;
}

bool AbstractViewInspector::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons()) {
        foreach (AbstractTool *tool, m_tools)
            tool->mouseMoveEvent(event);
    } else {
        foreach (AbstractTool *tool, m_tools)
            tool->hoverMoveEvent(event);
    }
    return true;
}

bool AbstractViewInspector::mouseReleaseEvent(QMouseEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->mouseReleaseEvent(event);
    return true;
}

bool AbstractViewInspector::keyPressEvent(QKeyEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->keyPressEvent(event);
    return true;
}

bool AbstractViewInspector::keyReleaseEvent(QKeyEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->keyReleaseEvent(event);
    return true;
}

bool AbstractViewInspector::mouseDoubleClickEvent(QMouseEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->mouseDoubleClickEvent(event);
    return true;
}

#ifndef QT_NO_WHEELEVENT
bool AbstractViewInspector::wheelEvent(QWheelEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->wheelEvent(event);
    return true;
}
#endif

bool AbstractViewInspector::touchEvent(QTouchEvent *event)
{
    foreach (AbstractTool *tool, m_tools)
        tool->touchEvent(event);
    return true;
}

void AbstractViewInspector::onQmlObjectDestroyed(QObject *object)
{
    if (!m_hashObjectsTobeDestroyed.contains(object))
        return;

    QPair<int, int> ids = m_hashObjectsTobeDestroyed.take(object);
    QQmlDebugService::removeInvalidObjectsFromHash();

    QByteArray response;

    QQmlDebugStream rs(&response, QIODevice::WriteOnly);
    rs << QByteArray(RESPONSE) << ids.first << true << ids.second;

    m_debugService->sendMessage(response);
}

void AbstractViewInspector::handleMessage(const QByteArray &message)
{
    bool success = true;
    QQmlDebugStream ds(message);

    QByteArray type;
    ds >> type;

    int requestId = -1;
    if (type == REQUEST) {
        QByteArray command;
        ds >> requestId >> command;

        if (command == ENABLE) {
            setEnabled(true);

        } else if (command == DISABLE) {
            setEnabled(false);

        } else if (command == SELECT) {
            QList<int> debugIds;
            ds >> debugIds;

            QList<QObject*> selectedObjects;
            foreach (int debugId, debugIds) {
                if (QObject *obj = QQmlDebugService::objectForId(debugId))
                    selectedObjects << obj;
            }
            if (m_enabled)
                changeCurrentObjects(selectedObjects);

        } else if (command == RELOAD) {
            QHash<QString, QByteArray> changesHash;
            ds >> changesHash;
            m_reloadEventId = requestId;
            reloadQmlFile(changesHash);
            return;

        } else if (command == SET_ANIMATION_SPEED) {
            qreal speed;
            ds >> speed;
            setAnimationSpeed(speed);

        } else if (command == SHOW_APP_ON_TOP) {
            bool showOnTop;
            ds >> showOnTop;
            setShowAppOnTop(showOnTop);

        } else if (command == CREATE_OBJECT) {
            QString qml;
            int parentId;
            QString filename;
            QStringList imports;
            ds >> qml >> parentId >> imports >> filename;
            createQmlObject(qml, QQmlDebugService::objectForId(parentId),
                            imports, filename);

        } else if (command == DESTROY_OBJECT) {
            int debugId;
            ds >> debugId;
            if (QObject *obj = QQmlDebugService::objectForId(debugId)) {
                QPair<int, int> ids(requestId, debugId);
                m_hashObjectsTobeDestroyed.insert(obj, ids);
                connect(obj, SIGNAL(destroyed(QObject*)), SLOT(onQmlObjectDestroyed(QObject*)));
                obj->deleteLater();
            }
            return;

        } else if (command == MOVE_OBJECT) {
            int debugId, newParent;
            ds >> debugId >> newParent;
            reparentQmlObject(QQmlDebugService::objectForId(debugId),
                              QQmlDebugService::objectForId(newParent));

        } else if (command == CLEAR_CACHE) {
            clearComponentCache();

        } else {
            qWarning() << "Warning: Not handling command:" << command;
            success = false;

        }
    } else {
        qWarning() << "Warning: Not handling type:" << type << REQUEST;
        success = false;

    }

    QByteArray response;
    QQmlDebugStream rs(&response, QIODevice::WriteOnly);
    rs << QByteArray(RESPONSE) << requestId << success;
    m_debugService->sendMessage(response);
}

void AbstractViewInspector::sendCurrentObjects(const QList<QObject*> &objects)
{
    QByteArray message;
    QQmlDebugStream ds(&message, QIODevice::WriteOnly);

    ds << QByteArray(EVENT) << m_eventId++ << QByteArray(SELECT);

    QList<int> debugIds;
    foreach (QObject *object, objects)
        debugIds << QQmlDebugService::idForObject(object);
    ds << debugIds;

    m_debugService->sendMessage(message);
}

void AbstractViewInspector::sendQmlFileReloaded(bool success)
{
    if (m_reloadEventId == -1)
        return;

    QByteArray response;

    QQmlDebugStream rs(&response, QIODevice::WriteOnly);
    rs << QByteArray(RESPONSE) << m_reloadEventId << success;

    m_debugService->sendMessage(response);
}

QString AbstractViewInspector::idStringForObject(QObject *obj) const
{
    QQmlContext *context = qmlContext(obj);
    if (context) {
        QQmlContextData *cdata = QQmlContextData::get(context);
        if (cdata)
            return cdata->findObjectId(obj);
    }
    return QString();
}

void AbstractViewInspector::appendTool(AbstractTool *tool)
{
    m_tools.append(tool);
}

void AbstractViewInspector::removeTool(AbstractTool *tool)
{
    m_tools.removeOne(tool);
}

} // namespace QmlJSDebugger
