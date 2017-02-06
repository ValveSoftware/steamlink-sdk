/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
class QQuickPopupPrivate;
class QQuickPopupItemPrivate;

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

class QQuickPopupItem : public QQuickControl
{
    Q_OBJECT

public:
    explicit QQuickPopupItem(QQuickPopup *popup);

    void grabShortcut();
    void ungrabShortcut();

protected:
    void updatePolish() override;

    bool event(QEvent *event) override;
    bool childMouseEventFilter(QQuickItem *child, QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void wheelEvent(QWheelEvent *event) override;

    void contentItemChange(QQuickItem *newItem, QQuickItem *oldItem) override;
    void fontChange(const QFont &newFont, const QFont &oldFont) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void localeChange(const QLocale &newLocale, const QLocale &oldLocale) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void paddingChange(const QMarginsF &newPadding, const QMarginsF &oldPadding) override;
    void spacingChange(qreal newSpacing, qreal oldSpacing) override;

    QFont defaultFont() const override;

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::Role accessibleRole() const override;
    void accessibilityActiveChanged(bool active) override;
#endif

private:
    Q_DECLARE_PRIVATE(QQuickPopupItem)

    friend class QQuickPopup;
};

class QQuickPopupPositioner : public QQuickItemChangeListener
{
public:
    explicit QQuickPopupPositioner(QQuickPopupPrivate *popup);
    ~QQuickPopupPositioner();

    QQuickItem *parentItem() const;
    void setParentItem(QQuickItem *parent);

protected:
    void itemGeometryChanged(QQuickItem *, QQuickGeometryChange, const QRectF &) override;
    void itemParentChanged(QQuickItem *, QQuickItem *parent) override;
    void itemChildRemoved(QQuickItem *, QQuickItem *child) override;

private:
    void removeAncestorListeners(QQuickItem *item);
    void addAncestorListeners(QQuickItem *item);

    bool isAncestor(QQuickItem *item) const;

    QQuickItem *m_parentItem;
    QQuickPopupPrivate *m_popup;
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
    bool tryClose(QQuickItem *item, QMouseEvent *event);
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
    TransitionState transitionState;
    QQuickPopup::ClosePolicy closePolicy;
    QQuickItem *parentItem;
    QQuickItem *dimmer;
    QQuickWindow *window;
    QQuickTransition *enter;
    QQuickTransition *exit;
    QQuickPopupItem *popupItem;
    QQuickPopupPositioner positioner;
    QList<QQuickStateAction> enterActions;
    QList<QQuickStateAction> exitActions;
    QQuickPopupTransitionManager transitionManager;

    friend class QQuickPopupTransitionManager;
};

QT_END_NAMESPACE

#endif // QQUICKPOPUP_P_P_H
