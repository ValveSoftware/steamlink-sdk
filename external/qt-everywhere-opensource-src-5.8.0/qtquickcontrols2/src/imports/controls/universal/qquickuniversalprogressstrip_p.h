/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
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

#ifndef QQUICKUNIVERSALPROGRESSSTRIP_P_H
#define QQUICKUNIVERSALPROGRESSSTRIP_P_H

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

#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquickanimator_p.h>

QT_BEGIN_NAMESPACE

class QQuickUniversalProgressStrip : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor FINAL)
    Q_PROPERTY(qreal progress READ progress WRITE setProgress FINAL)
    Q_PROPERTY(bool indeterminate READ isIndeterminate WRITE setIndeterminate FINAL)

public:
    QQuickUniversalProgressStrip(QQuickItem *parent = nullptr);

    QColor color() const;
    void setColor(const QColor &color);

    qreal progress() const;
    void setProgress(qreal progress);

    bool isIndeterminate() const;
    void setIndeterminate(bool indeterminate);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QColor m_color;
    qreal m_progress;
    bool m_indeterminate;
};

class QQuickUniversalProgressStripAnimator : public QQuickAnimator
{
    Q_OBJECT

public:
    QQuickUniversalProgressStripAnimator(QObject *parent = nullptr);

protected:
    QString propertyName() const override;
    QQuickAnimatorJob *createJob() const override;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickUniversalProgressStrip)
QML_DECLARE_TYPE(QQuickUniversalProgressStripAnimator)

#endif // QQUICKUNIVERSALPROGRESSSTRIP_P_H
