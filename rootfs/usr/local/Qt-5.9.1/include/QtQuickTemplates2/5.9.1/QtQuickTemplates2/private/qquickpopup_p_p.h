/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKPOPUP_P_P_H
#define QQUICKPOPUP_P_P_H

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

#include "qquickpopup_p.h"
#include "qquickcontrol_p.h"

#include <QtCore/private/qobject_p.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>
#include <QtQuick/private/qquicktransitionmanager_p_p.h>

QT_BEGIN_NAMESPACE

class QQuickTransition;
class QQuickTransitionManager;
class QQuickPopup;
class QQuickPopupItem;
class QQuickPopupPrivate;
class QQuickPopupPositioner;

class QQuickPopupTransitionManager : public QQuickTransitionManager
{
public:
    QQuickPopupTransitionManager(QQuickPopupPrivate *popup);

    void transitionEnter();
    void transitionExit();

protected:
    void finished() override;

private:
    QQuickPopupPrivate *popup;
};

class Q_AUTOTEST_EXPORT QQuickPopupPrivate : public QObjectPrivate, public QQuickItemChangeListener
{
    Q_DECLARE_PUBLIC(QQuickPopup)

public:
    QQuickPopupPrivate();
    ~QQuickPopupPrivate();

    static QQuickPopupPrivate *get(QQuickPopup *popup)
    {
        return popup->d_func();
    }

    void init();
    void closeOrReject();
    bool tryClose(const QPointF &pos, QQuickPopup::ClosePolicy flags);

    bool contains(const QPointF &scenePos) const;

#if QT_CONFIG(quicktemplates2_multitouch)
    virtual bool acceptTouch(const QTouchEvent::TouchPoint &point);
#endif
    virtual bool blockInput(QQuickItem *item, const QPointF &point) const;

    virtual bool handlePress(QQuickItem* item, const QPointF &point, ulong timestamp);
    virtual bool handleMove(QQuickItem* item, const QPointF &point, ulong timestamp);
    virtual bool handleRelease(QQuickItem* item, const QPointF &point, ulong timestamp);
    virtual void handleUngrab();

    bool handleMouseEvent(QQuickItem *item, QMouseEvent *event);
#if QT_CONFIG(quicktemplates2_multitouch)
    bool handleTouchEvent(QQuickItem *item, QTouchEvent *event);
#endif

    virtual void reposition();
    virtual void resizeOverlay();

    virtual bool prepareEnterTransition();
    virtual bool prepareExitTransition();
    virtual void finalizeEnterTransition();
    virtual void finalizeExitTransition();

    QMarginsF getMargins() const;

    void setTopMargin(qreal value, bool reset = false);
    void setLeftMargin(qreal value, bool reset = false);
    void setRightMargin(qreal value, bool reset = false);
    void setBottomMargin(qreal value, bool reset = false);

    void setWindow(QQuickWindow *window);
    void itemDestroyed(QQuickItem *item) override;

    enum TransitionState {
        NoTransition, EnterTransition, ExitTransition
    };

    bool focus;
    bool modal;
    bool dim;
    bool hasDim;
    bool visible;
    bool complete;
    bool positioning;
    bool hasWidth;
    bool hasHeight;
    bool hasTopMargin;
    bool hasLeftMargin;
    bool hasRightMargin;
    bool hasBottomMargin;
    bool allowVerticalFlip;
    bool allowHorizontalFlip;
    bool allowVerticalMove;
    bool allowHorizontalMove;
    bool allowVerticalResize;
    bool allowHorizontalResize;
    bool hadActiveFocusBeforeExitTransition;
    bool interactive;
    int touchId;
    qreal x;
    qreal y;
    qreal effectiveX;
    qreal effectiveY;
    qreal margins;
    qreal topMargin;
    qreal leftMargin;
    qreal rightMargin;
    qreal bottomMargin;
    qreal contentWidth;
    qreal contentHeight;
    QPointF pressPoint;
    TransitionState transitionState;
    QQuickPopup::ClosePolicy closePolicy;
    QQuickItem *parentItem;
    QQuickItem *dimmer;
    QQuickWindow *window;
    QQuickTransition *enter;
    QQuickTransition *exit;
    QQuickPopupItem *popupItem;
    QQuickPopupPositioner *positioner;
    QList<QQuickStateAction> enterActions;
    QList<QQuickStateAction> exitActions;
    QQuickPopupTransitionManager transitionManager;

    friend class QQuickPopupTransitionManager;
};

QT_END_NAMESPACE

#endif // QQUICKPOPUP_P_P_H
