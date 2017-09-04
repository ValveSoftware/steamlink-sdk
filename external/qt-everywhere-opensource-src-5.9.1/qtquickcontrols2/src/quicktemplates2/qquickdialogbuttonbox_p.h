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

#ifndef QQUICKDIALOGBUTTONBOX_P_H
#define QQUICKDIALOGBUTTONBOX_P_H

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

#include <QtQuickTemplates2/private/qquickcontainer_p.h>
#include <QtGui/qpa/qplatformdialoghelper.h>

QT_BEGIN_NAMESPACE

class QQmlComponent;
class QQuickAbstractButton;
class QQuickDialogButtonBoxPrivate;
class QQuickDialogButtonBoxAttached;
class QQuickDialogButtonBoxAttachedPrivate;

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickDialogButtonBox : public QQuickContainer
{
    Q_OBJECT
    Q_PROPERTY(Position position READ position WRITE setPosition NOTIFY positionChanged FINAL)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment RESET resetAlignment NOTIFY alignmentChanged FINAL)
    Q_PROPERTY(QPlatformDialogHelper::StandardButtons standardButtons READ standardButtons WRITE setStandardButtons NOTIFY standardButtonsChanged FINAL)
    Q_PROPERTY(QQmlComponent *delegate READ delegate WRITE setDelegate NOTIFY delegateChanged FINAL)
    Q_FLAGS(QPlatformDialogHelper::StandardButtons)

public:
    explicit QQuickDialogButtonBox(QQuickItem *parent = nullptr);
    ~QQuickDialogButtonBox();

    enum Position {
        Header,
        Footer
    };
    Q_ENUM(Position)

    Position position() const;
    void setPosition(Position position);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);
    void resetAlignment();

    QPlatformDialogHelper::StandardButtons standardButtons() const;
    void setStandardButtons(QPlatformDialogHelper::StandardButtons buttons);
    Q_INVOKABLE QQuickAbstractButton *standardButton(QPlatformDialogHelper::StandardButton button) const;

    QQmlComponent *delegate() const;
    void setDelegate(QQmlComponent *delegate);

    static QQuickDialogButtonBoxAttached *qmlAttachedProperties(QObject *object);

Q_SIGNALS:
    void accepted();
    void rejected();
    void helpRequested();
    void clicked(QQuickAbstractButton *button);

    void positionChanged();
    void alignmentChanged();
    void standardButtonsChanged();
    void delegateChanged();

protected:
    void updatePolish() override;
    void componentComplete() override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void contentItemChange(QQuickItem *newItem, QQuickItem *oldItem) override;
    bool isContent(QQuickItem *item) const override;
    void itemAdded(int index, QQuickItem *item) override;
    void itemRemoved(int index, QQuickItem *item) override;

#if QT_CONFIG(accessibility)
    QAccessible::Role accessibleRole() const override;
#endif

private:
    Q_DISABLE_COPY(QQuickDialogButtonBox)
    Q_DECLARE_PRIVATE(QQuickDialogButtonBox)
};

class Q_QUICKTEMPLATES2_PRIVATE_EXPORT QQuickDialogButtonBoxAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickDialogButtonBox *buttonBox READ buttonBox NOTIFY buttonBoxChanged FINAL)
    Q_PROPERTY(QPlatformDialogHelper::ButtonRole buttonRole READ buttonRole WRITE setButtonRole NOTIFY buttonRoleChanged FINAL)
    Q_ENUMS(QPlatformDialogHelper::ButtonRole)

public:
    explicit QQuickDialogButtonBoxAttached(QObject *parent = nullptr);

    QQuickDialogButtonBox *buttonBox() const;

    QPlatformDialogHelper::ButtonRole buttonRole() const;
    void setButtonRole(QPlatformDialogHelper::ButtonRole role);

Q_SIGNALS:
    void buttonBoxChanged();
    void buttonRoleChanged();

private:
    Q_DISABLE_COPY(QQuickDialogButtonBoxAttached)
    Q_DECLARE_PRIVATE(QQuickDialogButtonBoxAttached)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickDialogButtonBox)
QML_DECLARE_TYPEINFO(QQuickDialogButtonBox, QML_HAS_ATTACHED_PROPERTIES)

#endif // QQUICKDIALOGBUTTONBOX_P_H
