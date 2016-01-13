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

#ifndef ABSTRACTVIEWINSPECTOR_H
#define ABSTRACTVIEWINSPECTOR_H

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>

#include "qmlinspectorconstants.h"

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQmlInspectorService;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
class QTouchEvent;

QT_END_NAMESPACE

namespace QmlJSDebugger {

class AbstractTool;

/*
 * The common code between QQuickView and QQuickView inspectors lives here,
 */
class AbstractViewInspector : public QObject
{
    Q_OBJECT

public:
    explicit AbstractViewInspector(QObject *parent = 0);

    void handleMessage(const QByteArray &message);

    void createQmlObject(const QString &qml, QObject *parent,
                         const QStringList &importList,
                         const QString &filename = QString());
    void clearComponentCache();

    bool enabled() const { return m_enabled; }

    void sendCurrentObjects(const QList<QObject*> &);

    void sendQmlFileReloaded(bool success);

    QString idStringForObject(QObject *obj) const;

    virtual void changeCurrentObjects(const QList<QObject*> &objects) = 0;
    virtual void reparentQmlObject(QObject *object, QObject *newParent) = 0;
    virtual Qt::WindowFlags windowFlags() const = 0;
    virtual void setWindowFlags(Qt::WindowFlags flags) = 0;
    virtual QQmlEngine *declarativeEngine() const = 0;
    virtual void reloadQmlFile(const QHash<QString, QByteArray> &changesHash) = 0;

    void appendTool(AbstractTool *tool);
    void removeTool(AbstractTool *tool);

protected:
    bool eventFilter(QObject *, QEvent *);

    virtual bool leaveEvent(QEvent *);
    virtual bool mousePressEvent(QMouseEvent *event);
    virtual bool mouseMoveEvent(QMouseEvent *event);
    virtual bool mouseReleaseEvent(QMouseEvent *event);
    virtual bool keyPressEvent(QKeyEvent *event);
    virtual bool keyReleaseEvent(QKeyEvent *keyEvent);
    virtual bool mouseDoubleClickEvent(QMouseEvent *event);
#ifndef QT_NO_WHEELEVENT
    virtual bool wheelEvent(QWheelEvent *event);
#endif
    virtual bool touchEvent(QTouchEvent *event);
    virtual void setShowAppOnTop(bool) = 0;

private slots:
    void onQmlObjectDestroyed(QObject *object);

private:
    void setEnabled(bool value);

    void setAnimationSpeed(qreal factor);

    bool m_enabled;

    QQmlInspectorService *m_debugService;
    QList<AbstractTool *> m_tools;
    int m_eventId;
    int m_reloadEventId;
    // Hash< object to be destroyed, QPair<destroy eventId, object debugId> >
    QHash<QObject *, QPair<int, int> > m_hashObjectsTobeDestroyed;
};

} // namespace QmlJSDebugger

#endif // ABSTRACTVIEWINSPECTOR_H
