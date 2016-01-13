/****************************************************************************
 **
 ** Copyright (C) 2013 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef QQUICKDWMFEATURES_P_H
#define QQUICKDWMFEATURES_P_H

#include <QQuickItem>
#include <QtWin>
#include <QColor>

#include "qquickwin_p.h"

QT_BEGIN_NAMESPACE

class QQuickDwmFeaturesPrivate;

class QQuickDwmFeatures : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool compositionEnabled READ isCompositionEnabled WRITE setCompositionEnabled NOTIFY compositionEnabledChanged)
    Q_PROPERTY(QColor colorizationColor READ colorizationColor NOTIFY colorizationColorChanged)
    Q_PROPERTY(QColor realColorizationColor READ realColorizationColor NOTIFY realColorizationColorChanged)
    Q_PROPERTY(bool colorizationOpaqueBlend READ colorizationOpaqueBlend NOTIFY colorizationOpaqueBlendChanged)
    Q_PROPERTY(int topGlassMargin READ topGlassMargin WRITE setTopGlassMargin NOTIFY topGlassMarginChanged)
    Q_PROPERTY(int rightGlassMargin READ rightGlassMargin WRITE setRightGlassMargin NOTIFY rightGlassMarginChanged)
    Q_PROPERTY(int bottomGlassMargin READ bottomGlassMargin WRITE setBottomGlassMargin NOTIFY bottomGlassMarginChanged)
    Q_PROPERTY(int leftGlassMargin READ leftGlassMargin WRITE setLeftGlassMargin NOTIFY leftGlassMarginChanged)
    Q_PROPERTY(bool blurBehindEnabled READ isBlurBehindEnabled WRITE setBlurBehindEnabled NOTIFY blurBehindEnabledChanged)
    Q_PROPERTY(bool excludedFromPeek READ isExcludedFromPeek WRITE setExcludedFromPeek NOTIFY excludedFromPeekChanged)
    Q_PROPERTY(bool peekDisallowed READ isPeekDisallowed WRITE setPeekDisallowed NOTIFY peekDisallowedChanged)
    Q_PROPERTY(QQuickWin::WindowFlip3DPolicy flip3DPolicy READ flip3DPolicy WRITE setFlip3DPolicy NOTIFY flip3DPolicyChanged)

public:
    explicit QQuickDwmFeatures(QQuickItem *parent = 0);
    ~QQuickDwmFeatures();

    void setCompositionEnabled(bool enabled);
    bool isCompositionEnabled() const;
    QColor colorizationColor() const;
    QColor realColorizationColor() const;
    bool colorizationOpaqueBlend() const;

    void setTopGlassMargin(int margin);
    void setRightGlassMargin(int margin);
    void setBottomGlassMargin(int margin);
    void setLeftGlassMargin(int margin);
    int topGlassMargin() const;
    int rightGlassMargin() const;
    int bottomGlassMargin() const;
    int leftGlassMargin() const;
    bool isBlurBehindEnabled() const;
    void setBlurBehindEnabled(bool enabled);

    bool isExcludedFromPeek() const;
    void setExcludedFromPeek(bool exclude);
    bool isPeekDisallowed() const;
    void setPeekDisallowed(bool disallow);
    QQuickWin::WindowFlip3DPolicy flip3DPolicy() const;
    void setFlip3DPolicy(QQuickWin::WindowFlip3DPolicy policy);

    bool eventFilter(QObject *, QEvent *) Q_DECL_OVERRIDE;

    static QQuickDwmFeatures *qmlAttachedProperties(QObject *object);

Q_SIGNALS:
    void colorizationColorChanged();
    void realColorizationColorChanged();
    void compositionEnabledChanged();
    void colorizationOpaqueBlendChanged();
    void topGlassMarginChanged();
    void rightGlassMarginChanged();
    void bottomGlassMarginChanged();
    void leftGlassMarginChanged();
    void blurBehindEnabledChanged();
    void excludedFromPeekChanged();
    void peekDisallowedChanged();
    void flip3DPolicyChanged();

protected:
    void itemChange(ItemChange, const ItemChangeData &) Q_DECL_OVERRIDE;

private:
    QScopedPointer<QQuickDwmFeaturesPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QQuickDwmFeatures)
};

QT_END_NAMESPACE

QML_DECLARE_TYPEINFO(QQuickDwmFeatures, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKDWMFEATURES_P_H
