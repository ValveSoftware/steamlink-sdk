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

#ifndef QQUICKTEXTAREA_P_H
#define QQUICKTEXTAREA_P_H

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

#include <QtQuick/private/qquicktextedit_p.h>
#include <QtQuickTemplates2/private/qtquicktemplates2global_p.h>

QT_BEGIN_NAMESPACE

class QQuickText;
class QQuickTextAreaPrivate;
class QQuickTextAreaAttached;
class QQuickMouseEvent;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickTextArea : public QQuickTextEdit
{
    Q_OBJECT
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged) // override
    Q_PROPERTY(qreal implicitWidth READ implicitWidth WRITE setImplicitWidth NOTIFY implicitWidthChanged3 FINAL)
    Q_PROPERTY(qreal implicitHeight READ implicitHeight WRITE setImplicitHeight NOTIFY implicitHeightChanged3 FINAL)
    Q_PROPERTY(QQuickItem *background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged FINAL)
    Q_PROPERTY(Qt::FocusReason focusReason READ focusReason WRITE setFocusReason NOTIFY focusReasonChanged FINAL)
    Q_PROPERTY(bool hovered READ isHovered NOTIFY hoveredChanged FINAL REVISION 1)
    Q_PROPERTY(bool hoverEnabled READ isHoverEnabled WRITE setHoverEnabled RESET resetHoverEnabled NOTIFY hoverEnabledChanged FINAL REVISION 1)

public:
    explicit QQuickTextArea(QQuickItem *parent = nullptr);
    ~QQuickTextArea();

    static QQuickTextAreaAttached *qmlAttachedProperties(QObject *object);

    QFont font() const;
    void setFont(const QFont &font);

    QQuickItem *background() const;
    void setBackground(QQuickItem *background);

    QString placeholderText() const;
    void setPlaceholderText(const QString &text);

    Qt::FocusReason focusReason() const;
    void setFocusReason(Qt::FocusReason reason);

    bool isHovered() const;
    void setHovered(bool hovered);

    bool isHoverEnabled() const;
    void setHoverEnabled(bool enabled);
    void resetHoverEnabled();

    bool contains(const QPointF &point) const override;

Q_SIGNALS:
    void fontChanged();
    void implicitWidthChanged3();
    void implicitHeightChanged3();
    void backgroundChanged();
    void placeholderTextChanged();
    void focusReasonChanged();
    Q_REVISION(1) void hoveredChanged();
    Q_REVISION(1) void hoverEnabledChanged();
    void pressAndHold(QQuickMouseEvent *event);
    Q_REVISION(1) void pressed(QQuickMouseEvent *event);
    Q_REVISION(1) void released(QQuickMouseEvent *event);

protected:
    void classBegin() override;
    void componentComplete() override;

    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    Q_DISABLE_COPY(QQuickTextArea)
    Q_DECLARE_PRIVATE(QQuickTextArea)
};

class QQuickTextAreaAttachedPrivate;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickTextAreaAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextArea *flickable READ flickable WRITE setFlickable NOTIFY flickableChanged FINAL)

public:
    explicit QQuickTextAreaAttached(QObject *parent);
    ~QQuickTextAreaAttached();

    QQuickTextArea *flickable() const;
    void setFlickable(QQuickTextArea *control);

Q_SIGNALS:
    void flickableChanged();

private:
    Q_DISABLE_COPY(QQuickTextAreaAttached)
    Q_DECLARE_PRIVATE(QQuickTextAreaAttached)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickTextArea)
QML_DECLARE_TYPEINFO(QQuickTextArea, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKTEXTAREA_P_H
