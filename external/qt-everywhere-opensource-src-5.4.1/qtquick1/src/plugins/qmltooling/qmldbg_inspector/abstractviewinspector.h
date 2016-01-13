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

#ifndef ABSTRACTVIEWINSPECTOR_H
#define ABSTRACTVIEWINSPECTOR_H

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtGui/QColor>

#include "qdeclarativeinspectorprotocol.h"
#include "qmlinspectorconstants.h"

QT_BEGIN_NAMESPACE
class QDeclarativeEngine;
class QDeclarativeInspectorService;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;
QT_END_NAMESPACE

namespace QmlJSDebugger {

class AbstractTool;

/*
 * The common code between QQuickView and QDeclarativeView inspectors lives here,
 */
class AbstractViewInspector : public QObject
{
    Q_OBJECT

public:
    explicit AbstractViewInspector(QObject *parent = 0);

    void createQmlObject(const QString &qml, QObject *parent,
                         const QStringList &importList,
                         const QString &filename = QString());
    void clearComponentCache();

    bool showAppOnTop() const { return m_showAppOnTop; }
    bool designModeBehavior() const { return m_designModeBehavior; }

    bool animationPaused() const { return m_animationPaused; }
    qreal slowDownFactor() const { return m_slowDownFactor; }

    void sendCurrentObjects(const QList<QObject*> &);
    void sendAnimationSpeed(qreal slowDownFactor);
    void sendAnimationPaused(bool paused);
    void sendCurrentTool(Constants::DesignTool toolId);
    void sendReloaded();
    void sendShowAppOnTop(bool showAppOnTop);

    QString idStringForObject(QObject *obj) const;

    virtual void changeCurrentObjects(const QList<QObject*> &objects) = 0;
    virtual void reloadView() = 0;
    virtual void reparentQmlObject(QObject *object, QObject *newParent) = 0;
    virtual void changeTool(InspectorProtocol::Tool tool) = 0;
    virtual Qt::WindowFlags windowFlags() const = 0;
    virtual void setWindowFlags(Qt::WindowFlags flags) = 0;
    virtual QDeclarativeEngine *declarativeEngine() const = 0;

public slots:
    void handleMessage(const QByteArray &message);

signals:
    void designModeBehaviorChanged(bool inDesignMode);
    void showAppOnTopChanged(bool showAppOnTop);
    void reloadRequested();
    void marqueeSelectToolActivated();
    void selectToolActivated();
    void zoomToolActivated();
    void colorPickerActivated();
    void selectedColorChanged(const QColor &color);

    void animationSpeedChanged(qreal factor);
    void animationPausedChanged(bool paused);

protected:
    AbstractTool *currentTool() const { return m_currentTool; }
    void setCurrentTool(AbstractTool *tool) { m_currentTool = tool; }
    bool eventFilter(QObject *, QEvent *);

    virtual bool leaveEvent(QEvent *);
    virtual bool mousePressEvent(QMouseEvent *event);
    virtual bool mouseMoveEvent(QMouseEvent *event);
    virtual bool mouseReleaseEvent(QMouseEvent *event);
    virtual bool keyPressEvent(QKeyEvent *event);
    virtual bool keyReleaseEvent(QKeyEvent *keyEvent);
    virtual bool mouseDoubleClickEvent(QMouseEvent *event);
    virtual bool wheelEvent(QWheelEvent *event);

private slots:
    void sendColorChanged(const QColor &color);

private:
    void sendDesignModeBehavior(bool inDesignMode);

    void changeToColorPickerTool();
    void changeToZoomTool();
    void changeToSingleSelectTool();
    void changeToMarqueeSelectTool();

    virtual void setDesignModeBehavior(bool value);

    void setShowAppOnTop(bool appOnTop);

    void setAnimationSpeed(qreal factor);
    void setAnimationPaused(bool paused);

    void animationSpeedChangeRequested(qreal factor);
    void animationPausedChangeRequested(bool paused);

    AbstractTool *m_currentTool;

    bool m_showAppOnTop;
    bool m_designModeBehavior;

    bool m_animationPaused;
    qreal m_slowDownFactor;

    QHash<int, QString> m_stringIdForObjectId;
    QDeclarativeInspectorService *m_debugService;
};

} // namespace QmlJSDebugger

#endif // ABSTRACTVIEWINSPECTOR_H
