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

#ifndef QQUICKCONTROL_P_P_H
#define QQUICKCONTROL_P_P_H

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

#include "qquickcontrol_p.h"

#include <QtQuick/private/qquickitem_p.h>
#include <QtQml/private/qlazilyallocated_p.h>
#include <qpa/qplatformtheme.h>

#if QT_CONFIG(accessibility)
#include <QtGui/qaccessible.h>
#endif

QT_BEGIN_NAMESPACE

class QQuickAccessibleAttached;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickControlPrivate : public QQuickItemPrivate
#if QT_CONFIG(accessibility)
    , public QAccessible::ActivationObserver
#endif
{
    Q_DECLARE_PUBLIC(QQuickControl)

public:
    QQuickControlPrivate();
    virtual ~QQuickControlPrivate();

    static QQuickControlPrivate *get(QQuickControl *control)
    {
        return control->d_func();
    }

#if QT_CONFIG(quicktemplates2_multitouch)
    virtual bool acceptTouch(const QTouchEvent::TouchPoint &point);
#endif
    virtual void handlePress(const QPointF &point);
    virtual void handleMove(const QPointF &point);
    virtual void handleRelease(const QPointF &point);
    virtual void handleUngrab();

    void mirrorChange() override;

    void setTopPadding(qreal value, bool reset = false);
    void setLeftPadding(qreal value, bool reset = false);
    void setRightPadding(qreal value, bool reset = false);
    void setBottomPadding(qreal value, bool reset = false);

    void resizeBackground();
    virtual void resizeContent();

    virtual QQuickItem *getContentItem();
    void setContentItem_helper(QQuickItem *item, bool notify = true);

#if QT_CONFIG(accessibility)
    void accessibilityActiveChanged(bool active) override;
    QAccessible::Role accessibleRole() const override;
#endif

    void updateFont(const QFont &f);
    static void updateFontRecur(QQuickItem *item, const QFont &f);
    inline void setFont_helper(const QFont &f) {
        if (resolvedFont.resolve() == f.resolve() && resolvedFont == f)
            return;
        updateFont(f);
    }
    virtual void resolveFont();
    void inheritFont(const QFont &f);
    static QFont parentFont(const QQuickItem *item);
    static QFont themeFont(QPlatformTheme::Font type);

    void updateLocale(const QLocale &l, bool e);
    static void updateLocaleRecur(QQuickItem *item, const QLocale &l);
    static QLocale calcLocale(const QQuickItem *item);

#if QT_CONFIG(quicktemplates2_hover)
    void updateHoverEnabled(bool enabled, bool xplicit);
    static void updateHoverEnabledRecur(QQuickItem *item, bool enabled);
    static bool calcHoverEnabled(const QQuickItem *item);
#endif

    static void destroyDelegate(QObject *object, QObject *parent);

    struct ExtraData {
        ExtraData();
        QFont font;
    };
    QLazilyAllocated<ExtraData> extra;

    QFont resolvedFont;
    bool hasTopPadding;
    bool hasLeftPadding;
    bool hasRightPadding;
    bool hasBottomPadding;
    bool hasLocale;
    bool wheelEnabled;
#if QT_CONFIG(quicktemplates2_hover)
    bool hovered;
    bool explicitHoverEnabled;
#endif
    int touchId;
    qreal padding;
    qreal topPadding;
    qreal leftPadding;
    qreal rightPadding;
    qreal bottomPadding;
    qreal spacing;
    QLocale locale;
    Qt::FocusPolicy focusPolicy;
    Qt::FocusReason focusReason;
    QQuickItem *background;
    QQuickItem *contentItem;
    QQuickAccessibleAttached *accessibleAttached;
};

QT_END_NAMESPACE

#endif // QQUICKCONTROL_P_P_H
