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

#include "previewactiongroup.h"

#include <deviceprofile_p.h>
#include <shared_settings_p.h>

#include <QtWidgets/QStyleFactory>
#include <QtCore/QVariant>

QT_BEGIN_NAMESPACE

enum { MaxDeviceActions = 20 };

namespace qdesigner_internal {

PreviewActionGroup::PreviewActionGroup(QDesignerFormEditorInterface *core, QObject *parent) :
    QActionGroup(parent),
    m_core(core)
{
    /* Create a list of up to MaxDeviceActions invisible actions to be
     * populated with device profiles (actiondata: index) followed by the
     * standard style actions (actiondata: style name). */
    connect(this, &PreviewActionGroup::triggered, this, &PreviewActionGroup::slotTriggered);
    setExclusive(true);

    const QString objNamePostfix = QStringLiteral("_action");
    // Create invisible actions for devices. Set index as action data.
    QString objNamePrefix = QStringLiteral("__qt_designer_device_");
    for (int i = 0; i < MaxDeviceActions; i++) {
        QAction *a = new QAction(this);
        QString objName = objNamePrefix;
        objName += QString::number(i);
        objName += objNamePostfix;
        a->setObjectName(objName);
        a->setVisible(false);
        a->setData(i);
        addAction(a);
    }
    // Create separator at index MaxDeviceActions
    QAction *sep = new QAction(this);
    sep->setObjectName(QStringLiteral("__qt_designer_deviceseparator"));
    sep->setSeparator(true);
    sep->setVisible(false);
    addAction(sep);
    // Populate devices
    updateDeviceProfiles();

    // Add style actions
    const QStringList styles = QStyleFactory::keys();
    const QStringList::const_iterator cend = styles.constEnd();
    // Make sure ObjectName  is unique in case toolbar solution is used.
    objNamePrefix = QStringLiteral("__qt_designer_style_");
    // Create styles. Set style name string as action data.
    for (QStringList::const_iterator it = styles.constBegin(); it !=  cend ;++it) {
        QAction *a = new QAction(tr("%1 Style").arg(*it), this);
        QString objName = objNamePrefix;
        objName += *it;
        objName += objNamePostfix;
        a->setObjectName(objName);
        a->setData(*it);
        addAction(a);
    }
}

void PreviewActionGroup::updateDeviceProfiles()
{
    typedef QList<DeviceProfile> DeviceProfileList;
    typedef QList<QAction *> ActionList;

    const QDesignerSharedSettings settings(m_core);
    const DeviceProfileList profiles = settings.deviceProfiles();
    const ActionList al = actions();
    // Separator?
    const bool hasProfiles = !profiles.empty();
    al.at(MaxDeviceActions)->setVisible(hasProfiles);
    int index = 0;
    if (hasProfiles) {
        // Make actions visible
        const int maxIndex = qMin(static_cast<int>(MaxDeviceActions), profiles.size());
        for (; index < maxIndex; index++) {
            const QString name = profiles.at(index).name();
            al.at(index)->setText(name);
            al.at(index)->setVisible(true);
        }
    }
    // Hide rest
    for ( ; index < MaxDeviceActions; index++)
        al.at(index)->setVisible(false);
}

void PreviewActionGroup::slotTriggered(QAction *a)
{
    // Device or style according to data.
    const QVariant data = a->data();
    switch (data.type()) {
    case QVariant::String:
        emit preview(data.toString(), -1);
        break;
    case QVariant::Int:
        emit preview(QString(), data.toInt());
        break;
    default:
        break;
    }
}

}

QT_END_NAMESPACE
