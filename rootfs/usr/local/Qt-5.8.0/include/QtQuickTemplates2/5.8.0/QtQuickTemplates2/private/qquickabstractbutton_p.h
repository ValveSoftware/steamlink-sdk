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

#ifndef QQUICKABSTRACTBUTTON_P_H
#define QQUICKABSTRACTBUTTON_P_H

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

QT_BEGIN_NAMESPACE

class QQuickAbstractButtonPrivate;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickAbstractButton : public QQuickControl
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged FINAL)
    Q_PROPERTY(bool down READ isDown WRITE setDown NOTIFY downChanged RESET resetDown FINAL)
    Q_PROPERTY(bool pressed READ isPressed NOTIFY pressedChanged FINAL)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY checkedChanged FINAL)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY checkableChanged FINAL)
    Q_PROPERTY(bool autoExclusive READ autoExclusive WRITE setAutoExclusive NOTIFY autoExclusiveChanged FINAL)
    Q_PROPERTY(QQuickItem *indicator READ indicator WRITE setIndicator NOTIFY indicatorChanged FINAL)

public:
    explicit QQuickAbstractButton(QQuickItem *parent = nullptr);
    ~QQuickAbstractButton();

    QString text() const;
    void setText(const QString &text);

    bool isDown() const;
    void setDown(bool down);
    void resetDown();

    bool isPressed() const;
    void setPressed(bool pressed);

    bool isChecked() const;
    void setChecked(bool checked);

    bool isCheckable() const;
    void setCheckable(bool checkable);

    bool autoExclusive() const;
    void setAutoExclusive(bool exclusive);

    bool autoRepeat() const;
    void setAutoRepeat(bool repeat);

    QQuickItem *indicator() const;
    void setIndicator(QQuickItem *indicator);

public Q_SLOTS:
    void toggle();

Q_SIGNALS:
    void pressed();
    void released();
    void canceled();
    void clicked();
    void pressAndHold();
    void doubleClicked();
    void textChanged();
    void downChanged();
    void pressedChanged();
    void checkedChanged();
    void checkableChanged();
    void autoExclusiveChanged();
    void indicatorChanged();

protected:
    QQuickAbstractButton(QQuickAbstractButtonPrivate &dd, QQuickItem *parent);

    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseUngrabEvent() override;
    void timerEvent(QTimerEvent *event) override;

    virtual void checkStateSet();
    virtual void nextCheckState();

    virtual void checkableChange();
    virtual void autoRepeatChange();

#ifndef QT_NO_ACCESSIBILITY
    void accessibilityActiveChanged(bool active) override;
    QAccessible::Role accessibleRole() const override;
#endif

private:
    Q_DISABLE_COPY(QQuickAbstractButton)
    Q_DECLARE_PRIVATE(QQuickAbstractButton)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickAbstractButton)

#endif // QQUICKABSTRACTBUTTON_P_H
