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

#include "qaxwidgetpropertysheet.h"
#include "qdesigneraxwidget.h"

#include <QtDesigner/QDesignerMemberSheetExtension>
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerPropertyEditorInterface>

#include <QtDesigner/QExtensionManager>
#include <private/qdesigner_utils_p.h>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

static const char *geometryPropertyC = "geometry";

QT_BEGIN_NAMESPACE

const char *QAxWidgetPropertySheet::controlPropertyName = "control";

static QString designerPropertyToString(const QVariant &value)
{
    return value.canConvert<qdesigner_internal::PropertySheetStringValue>() ?
        qvariant_cast<qdesigner_internal::PropertySheetStringValue>(value).value() :
        value.toString();
}

QAxWidgetPropertySheet::QAxWidgetPropertySheet(QDesignerAxWidget *object, QObject *parent) :
    QDesignerPropertySheet(object, parent),
    m_controlProperty(controlPropertyName),
    m_propertyGroup(QStringLiteral("QAxWidget"))
{
     if (!axWidget()->loaded()) { // For some obscure reason....
        const int controlIndex = QDesignerPropertySheet::indexOf(m_controlProperty);
        setPropertyGroup(controlIndex, m_propertyGroup);
    }
}

bool QAxWidgetPropertySheet::isEnabled(int index) const
{
    if (propertyName(index) == m_controlProperty)
        return false;
    return QDesignerPropertySheet::isEnabled(index);
}

bool QAxWidgetPropertySheet::dynamicPropertiesAllowed() const
{
    return false;
}

QDesignerAxWidget *QAxWidgetPropertySheet::axWidget() const
{
    return static_cast<QDesignerAxWidget*>(object());
}

// Reload as the meta object changes.
bool QAxWidgetPropertySheet::reset(int index)
{
    const QString name = propertyName(index);
    QMap<QString, QVariant>::iterator it = m_currentProperties.changedProperties.find(name);
    if (it !=  m_currentProperties.changedProperties.end())
        m_currentProperties.changedProperties.erase(it);
    if (name != m_controlProperty)
        return QDesignerPropertySheet::reset(index);
    axWidget()->resetControl();
    QTimer::singleShot(0, this, &QAxWidgetPropertySheet::updatePropertySheet);
    return true;
}

QVariant QAxWidgetPropertySheet::property(int index) const
{
    // QTBUG-34592, accessing the 'control' property via meta object system
    // may cause crashes during loading for some controls.
    return propertyName(index) == m_controlProperty ?
        QVariant(axWidget()->control()) :
        QDesignerPropertySheet::property(index);
}

void QAxWidgetPropertySheet::setProperty(int index, const QVariant &value)
{

    // take care of all changed properties
    const QString name = propertyName(index);
    m_currentProperties.changedProperties[name] = value;
    if (name != m_controlProperty) {
        QDesignerPropertySheet::setProperty(index, value);
        return;
    }
    // Loading forms: Reload
    if (name == m_controlProperty) {
        const QString clsid = designerPropertyToString(value);
        if (clsid.isEmpty() || !axWidget()->loadControl(clsid))
            reset(index);
        else
            QTimer::singleShot(100, this, &QAxWidgetPropertySheet::updatePropertySheet);
    }
}

int QAxWidgetPropertySheet::indexOf(const QString &name) const
{
    const int index = QDesignerPropertySheet::indexOf(name);
    if (index != -1)
        return index;
    // Loading before recreation of sheet in timer slot: Add a fake property to store the value
    const QVariant dummValue(0);
    QAxWidgetPropertySheet *that = const_cast<QAxWidgetPropertySheet *>(this);
    const int newIndex = that->createFakeProperty(name, dummValue);
    that->setPropertyGroup(newIndex, m_propertyGroup);
    return newIndex;
}

void QAxWidgetPropertySheet::updatePropertySheet()
{
    // refresh the property sheet (we are deleting m_currentProperties)
    struct SavedProperties tmp = m_currentProperties;
    QDesignerAxWidget *axw = axWidget();
    QDesignerFormWindowInterface *formWin = QDesignerFormWindowInterface::findFormWindow(axw);
    Q_ASSERT(formWin != 0);
    tmp.widget = axw;
    tmp.clsid = axw->control();
    // Delete the sheets as they cache the meta object and other information
    delete this;
    delete qt_extension<QDesignerMemberSheetExtension *>(formWin->core()->extensionManager(), axw);
    reloadPropertySheet(tmp, formWin);
}

void QAxWidgetPropertySheet::reloadPropertySheet(const struct SavedProperties &properties, QDesignerFormWindowInterface *formWin)
{
    QDesignerFormEditorInterface *core = formWin->core();
    //Recreation of the property sheet
    QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension *>(core->extensionManager(), properties.widget);

    bool foundGeometry = false;
    const QString geometryProperty = QLatin1String(geometryPropertyC);
    const SavedProperties::NamePropertyMap::const_iterator cend = properties.changedProperties.constEnd();
    for (SavedProperties::NamePropertyMap::const_iterator i = properties.changedProperties.constBegin(); i != cend; ++i) {
        const QString name = i.key();
        const int index = sheet->indexOf(name);
        if (index == -1)
            continue;
        // filter out geometry as this will resize the control
        // to is default size even if it is attached to an layout
        // but set the changed flag to work around preview bug...
        if (name == geometryProperty) {
            sheet->setChanged(index, true);
            foundGeometry = true;
            continue;
        }
        if (name == QLatin1String(controlPropertyName))  {
            sheet->setChanged(index, !designerPropertyToString(i.value()).isEmpty());
            continue;
        }
        sheet->setChanged(index, true);
        sheet->setProperty(index, i.value());
    }

    if (!foundGeometry) // Make sure geometry is always changed in Designer
        sheet->setChanged(sheet->indexOf(geometryProperty), true);

    if (core->propertyEditor()->object() == properties.widget) {
        formWin->clearSelection(true);
        formWin->selectWidget(properties.widget);
    }
}

QT_END_NAMESPACE
