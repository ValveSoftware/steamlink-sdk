/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QACTIVEXPROPERTYSHEET_H
#define QACTIVEXPROPERTYSHEET_H

#include <QtDesigner/private/qdesigner_propertysheet_p.h>

QT_BEGIN_NAMESPACE

class QDesignerAxWidget;
class QDesignerFormWindowInterface;

/* The propertysheet has a method to delete itself and repopulate
 * if the "control" property changes. Pre 4.5, the control property
 * might not be the first one, so, the properties are stored and
 * re-applied. If the "control" is the first one, it should be
 * sufficient to reapply the changed flags, however, care must be taken when
 * resetting the control.
 * Resetting a control: The current behaviour is that the modified Active X properties are added again
 * as Fake-Properties, which is a nice side-effect as not cause a loss. */

class QAxWidgetPropertySheet: public QDesignerPropertySheet
{
    Q_OBJECT
    Q_INTERFACES(QDesignerPropertySheetExtension)
public:
    explicit QAxWidgetPropertySheet(QDesignerAxWidget *object, QObject *parent = 0);

    bool isEnabled(int index) const Q_DECL_OVERRIDE;
    QVariant property(int index) const Q_DECL_OVERRIDE;
    void setProperty(int index, const QVariant &value) Q_DECL_OVERRIDE;
    bool reset(int index) Q_DECL_OVERRIDE;
    int indexOf(const QString &name) const Q_DECL_OVERRIDE;
    bool dynamicPropertiesAllowed() const Q_DECL_OVERRIDE;

    static const char *controlPropertyName;

public slots:
    void updatePropertySheet();

private:
    QDesignerAxWidget *axWidget() const;

    const QString m_controlProperty;
    const QString m_propertyGroup;
    int m_controlIndex;
    struct SavedProperties {
        typedef QMap<QString, QVariant> NamePropertyMap;
        NamePropertyMap changedProperties;
        QWidget *widget;
        QString clsid;
    } m_currentProperties;

    static void reloadPropertySheet(const struct SavedProperties &properties, QDesignerFormWindowInterface *formWin);
};

typedef QDesignerPropertySheetFactory<QDesignerAxWidget, QAxWidgetPropertySheet> ActiveXPropertySheetFactory;

QT_END_NAMESPACE

#endif
