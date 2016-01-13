/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#ifndef FONTPROPERTYMANAGER_H
#define FONTPROPERTYMANAGER_H

#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class QtProperty;
class QtVariantPropertyManager;

class QString;
class QVariant;

namespace qdesigner_internal {

/* FontPropertyManager: A mixin for DesignerPropertyManager that manages font
 * properties. Adds an antialiasing subproperty and reset flags/mask handling
 * for the other subproperties. It also modifies the font family
 * enumeration names, which it reads from an XML mapping file that
 * contains annotations indicating the platform the font is available on. */

class FontPropertyManager {
    Q_DISABLE_COPY(FontPropertyManager)

public:
    FontPropertyManager();

    typedef QMap<QtProperty *, bool> ResetMap;
    typedef QMap<QString, QString> NameMap;

    // Call before QtVariantPropertyManager::initializeProperty.
    void preInitializeProperty(QtProperty *property, int type, ResetMap &resetMap);
    // Call after QtVariantPropertyManager::initializeProperty. This will trigger
    // a recursion for the sub properties
    void postInitializeProperty(QtVariantPropertyManager *vm, QtProperty *property, int type, int enumTypeId);

    bool uninitializeProperty(QtProperty *property);

    // Call from  QtPropertyManager's propertyDestroyed signal
    void slotPropertyDestroyed(QtProperty *property);

    bool resetFontSubProperty(QtVariantPropertyManager *vm, QtProperty *subProperty);

    // Call from slotValueChanged(), returns DesignerPropertyManager::ValueChangedResult
    int valueChanged(QtVariantPropertyManager *vm, QtProperty *property, const QVariant &value);

    // Call from setValue() before calling setValue() on  QtVariantPropertyManager.
    void setValue(QtVariantPropertyManager *vm, QtProperty *property, const QVariant &value);

    static bool readFamilyMapping(NameMap *rc, QString *errorMessage);

private:
    typedef QMap<QtProperty *, QtProperty *> PropertyToPropertyMap;
    typedef QList<QtProperty *> PropertyList;
    typedef QMap<QtProperty *, PropertyList>  PropertyToSubPropertiesMap;

    void removeAntialiasingProperty(QtProperty *);
    void updateModifiedState(QtProperty *property, const QVariant &value);
    static int antialiasingToIndex(QFont::StyleStrategy antialias);
    static QFont::StyleStrategy indexToAntialiasing(int idx);
    static unsigned fontFlag(int idx);

    PropertyToPropertyMap m_propertyToAntialiasing;
    PropertyToPropertyMap m_antialiasingToProperty;

    PropertyToSubPropertiesMap m_propertyToFontSubProperties;
    QMap<QtProperty *, int> m_fontSubPropertyToFlag;
    PropertyToPropertyMap m_fontSubPropertyToProperty;
    QtProperty *m_createdFontProperty;
    QStringList m_aliasingEnumNames;
    // Font families with Designer annotations
    QStringList m_designerFamilyNames;
    NameMap m_familyMappings;
};

}

QT_END_NAMESPACE

#endif // FONTPROPERTYMANAGER_H
