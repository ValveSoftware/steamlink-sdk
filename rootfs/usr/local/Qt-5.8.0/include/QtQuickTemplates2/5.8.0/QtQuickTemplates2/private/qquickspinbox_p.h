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

#ifndef QQUICKSPINBOX_P_H
#define QQUICKSPINBOX_P_H

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

#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtQml/qjsvalue.h>

QT_BEGIN_NAMESPACE

class QValidator;
class QQuickSpinButton;
class QQuickSpinButtonPrivate;
class QQuickSpinBoxPrivate;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickSpinBox : public QQuickControl
{
    Q_OBJECT
    Q_PROPERTY(int from READ from WRITE setFrom NOTIFY fromChanged FINAL)
    Q_PROPERTY(int to READ to WRITE setTo NOTIFY toChanged FINAL)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged FINAL)
    Q_PROPERTY(int stepSize READ stepSize WRITE setStepSize NOTIFY stepSizeChanged FINAL)
    Q_PROPERTY(bool editable READ isEditable WRITE setEditable NOTIFY editableChanged FINAL)
    Q_PROPERTY(QValidator *validator READ validator WRITE setValidator NOTIFY validatorChanged FINAL)
    Q_PROPERTY(QJSValue textFromValue READ textFromValue WRITE setTextFromValue NOTIFY textFromValueChanged FINAL)
    Q_PROPERTY(QJSValue valueFromText READ valueFromText WRITE setValueFromText NOTIFY valueFromTextChanged FINAL)
    Q_PROPERTY(QQuickSpinButton *up READ up CONSTANT FINAL)
    Q_PROPERTY(QQuickSpinButton *down READ down CONSTANT FINAL)

public:
    explicit QQuickSpinBox(QQuickItem *parent = nullptr);

    int from() const;
    void setFrom(int from);

    int to() const;
    void setTo(int to);

    int value() const;
    void setValue(int value);

    int stepSize() const;
    void setStepSize(int step);

    bool isEditable() const;
    void setEditable(bool editable);

    QValidator *validator() const;
    void setValidator(QValidator *validator);

    QJSValue textFromValue() const;
    void setTextFromValue(const QJSValue &callback);

    QJSValue valueFromText() const;
    void setValueFromText(const QJSValue &callback);

    QQuickSpinButton *up() const;
    QQuickSpinButton *down() const;

public Q_SLOTS:
    void increase();
    void decrease();

Q_SIGNALS:
    void fromChanged();
    void toChanged();
    void valueChanged();
    void stepSizeChanged();
    void editableChanged();
    void validatorChanged();
    void textFromValueChanged();
    void valueFromTextChanged();

protected:
    bool childMouseEventFilter(QQuickItem *child, QEvent *event) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void timerEvent(QTimerEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    void componentComplete() override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;
    void contentItemChange(QQuickItem *newItem, QQuickItem *oldItem) override;

    QFont defaultFont() const override;

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::Role accessibleRole() const override;
#endif

private:
    Q_DISABLE_COPY(QQuickSpinBox)
    Q_DECLARE_PRIVATE(QQuickSpinBox)
};

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickSpinButton : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool pressed READ isPressed WRITE setPressed NOTIFY pressedChanged FINAL)
    Q_PROPERTY(bool hovered READ isHovered WRITE setHovered NOTIFY hoveredChanged FINAL REVISION 1)
    Q_PROPERTY(QQuickItem *indicator READ indicator WRITE setIndicator NOTIFY indicatorChanged FINAL)

public:
    explicit QQuickSpinButton(QQuickSpinBox *parent);

    bool isPressed() const;
    void setPressed(bool pressed);

    bool isHovered() const;
    void setHovered(bool hovered);

    QQuickItem *indicator() const;
    void setIndicator(QQuickItem *indicator);

Q_SIGNALS:
    void pressedChanged();
    Q_REVISION(1) void hoveredChanged();
    void indicatorChanged();

private:
    Q_DISABLE_COPY(QQuickSpinButton)
    Q_DECLARE_PRIVATE(QQuickSpinButton)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickSpinBox)

#endif // QQUICKSPINBOX_P_H
