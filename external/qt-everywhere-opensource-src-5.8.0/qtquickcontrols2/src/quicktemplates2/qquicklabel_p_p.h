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

#ifndef QQUICKLABEL_P_P_H
#define QQUICKLABEL_P_P_H

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

#include <QtQuick/private/qquicktext_p_p.h>

#ifndef QT_NO_ACCESSIBILITY
#include <QtGui/qaccessible.h>
#endif

QT_BEGIN_NAMESPACE

class QQuickAccessibleAttached;

class QQuickLabelPrivate : public QQuickTextPrivate
#ifndef QT_NO_ACCESSIBILITY
    , public QAccessible::ActivationObserver
#endif
{
    Q_DECLARE_PUBLIC(QQuickLabel)

public:
    QQuickLabelPrivate();
    ~QQuickLabelPrivate();

    static QQuickLabelPrivate *get(QQuickLabel *item) {
        return static_cast<QQuickLabelPrivate *>(QObjectPrivate::get(item)); }

    void resizeBackground();
    void resolveFont();
    void inheritFont(const QFont &f);

    void _q_textChanged(const QString &text);

#ifndef QT_NO_ACCESSIBILITY
    void accessibilityActiveChanged(bool active) override;
    QAccessible::Role accessibleRole() const override;
#endif

    void deleteDelegate(QObject *object);

    QFont font;
    QQuickItem *background;
    QQuickAccessibleAttached *accessibleAttached;
    // This list contains the default delegates which were
    // replaced with custom ones via declarative assignments
    // before Component.completed() was emitted. See QTBUG-50992.
    QVector<QObject *> pendingDeletions;
};

QT_END_NAMESPACE

#endif // QQUICKLABEL_P_P_H
