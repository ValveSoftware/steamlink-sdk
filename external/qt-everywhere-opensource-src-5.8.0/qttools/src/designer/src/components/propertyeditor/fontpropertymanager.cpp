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

#include "fontpropertymanager.h"
#include "qtpropertymanager.h"
#include "designerpropertymanager.h"
#include "qtpropertybrowserutils_p.h"

#include <qdesigner_utils_p.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariant>
#include <QtCore/QString>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QXmlStreamReader>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

    static const char *aliasingC[] = {
        QT_TRANSLATE_NOOP("FontPropertyManager", "PreferDefault"),
        QT_TRANSLATE_NOOP("FontPropertyManager", "NoAntialias"),
        QT_TRANSLATE_NOOP("FontPropertyManager", "PreferAntialias")
    };

    FontPropertyManager::FontPropertyManager() :
        m_createdFontProperty(0)
    {
        const int nameCount = sizeof(aliasingC)/sizeof(const char *);
        for (int  i = 0; i < nameCount; i++)
            m_aliasingEnumNames.push_back(QCoreApplication::translate("FontPropertyManager", aliasingC[i]));

        QString errorMessage;
        if (!readFamilyMapping(&m_familyMappings, &errorMessage)) {
            designerWarning(errorMessage);
        }

    }

    void FontPropertyManager::preInitializeProperty(QtProperty *property,
                                                    int type,
                                                    ResetMap &resetMap)
    {
        if (m_createdFontProperty) {
            PropertyToSubPropertiesMap::iterator it = m_propertyToFontSubProperties.find(m_createdFontProperty);
            if (it == m_propertyToFontSubProperties.end())
                it = m_propertyToFontSubProperties.insert(m_createdFontProperty, PropertyList());
            const int index = it.value().size();
            m_fontSubPropertyToFlag.insert(property, index);
            it.value().push_back(property);
            m_fontSubPropertyToProperty[property] = m_createdFontProperty;
            resetMap[property] = true;
        }

        if (type == QVariant::Font)
            m_createdFontProperty = property;
    }

    // Map the font family names to display names retrieved from the XML configuration
    static QStringList designerFamilyNames(QStringList families, const FontPropertyManager::NameMap &nm)
    {
        if (nm.empty())
            return families;

        const auto ncend = nm.constEnd();
        for (auto it = families.begin(), end = families.end(); it != end; ++it) {
            const auto nit = nm.constFind(*it);
            if (nit != ncend)
                *it = nit.value();
        }
        return families;
    }

    void FontPropertyManager::postInitializeProperty(QtVariantPropertyManager *vm,
                                                     QtProperty *property,
                                                     int type,
                                                     int enumTypeId)
    {
        if (type != QVariant::Font)
            return;

        // This will cause a recursion
        QtVariantProperty *antialiasing = vm->addProperty(enumTypeId, QCoreApplication::translate("FontPropertyManager", "Antialiasing"));
        const QFont font = qvariant_cast<QFont>(vm->variantProperty(property)->value());

        antialiasing->setAttribute(QStringLiteral("enumNames"), m_aliasingEnumNames);
        antialiasing->setValue(antialiasingToIndex(font.styleStrategy()));
        property->addSubProperty(antialiasing);

        m_propertyToAntialiasing[property] = antialiasing;
        m_antialiasingToProperty[antialiasing] = property;
        // Fiddle family names
        if (!m_familyMappings.empty()) {
            const PropertyToSubPropertiesMap::iterator it = m_propertyToFontSubProperties.find(m_createdFontProperty);
            QtVariantProperty *familyProperty = vm->variantProperty(it.value().front());
            const QString enumNamesAttribute = QStringLiteral("enumNames");
            QStringList plainFamilyNames = familyProperty->attributeValue(enumNamesAttribute).toStringList();
            // Did someone load fonts or something?
            if (m_designerFamilyNames.size() != plainFamilyNames.size())
                m_designerFamilyNames = designerFamilyNames(plainFamilyNames, m_familyMappings);
            familyProperty->setAttribute(enumNamesAttribute, m_designerFamilyNames);
        }
        // Next
        m_createdFontProperty = 0;
    }

    bool FontPropertyManager::uninitializeProperty(QtProperty *property)
    {
        const PropertyToPropertyMap::iterator ait =  m_propertyToAntialiasing.find(property);
        if (ait != m_propertyToAntialiasing.end()) {
            QtProperty *antialiasing = ait.value();
            m_antialiasingToProperty.remove(antialiasing);
            m_propertyToAntialiasing.erase(ait);
            delete antialiasing;
        }

        PropertyToSubPropertiesMap::iterator sit = m_propertyToFontSubProperties.find(property);
        if (sit == m_propertyToFontSubProperties.end())
            return false;

        m_propertyToFontSubProperties.erase(sit);
        m_fontSubPropertyToFlag.remove(property);
        m_fontSubPropertyToProperty.remove(property);

        return true;
    }

    void FontPropertyManager::slotPropertyDestroyed(QtProperty *property)
    {
        removeAntialiasingProperty(property);
    }

    void FontPropertyManager::removeAntialiasingProperty(QtProperty *property)
    {
        const PropertyToPropertyMap::iterator ait =  m_antialiasingToProperty.find(property);
        if (ait == m_antialiasingToProperty.end())
            return;
        m_propertyToAntialiasing[ait.value()] = 0;
        m_antialiasingToProperty.erase(ait);
    }

    bool FontPropertyManager::resetFontSubProperty(QtVariantPropertyManager *vm, QtProperty *property)
    {
        const PropertyToPropertyMap::iterator it = m_fontSubPropertyToProperty.find(property);
        if (it == m_fontSubPropertyToProperty.end())
            return false;

        QtVariantProperty *fontProperty = vm->variantProperty(it.value());

        QVariant v = fontProperty->value();
        QFont font = qvariant_cast<QFont>(v);
        unsigned mask = font.resolve();
        const unsigned flag = fontFlag(m_fontSubPropertyToFlag.value(property));

        mask &= ~flag;
        font.resolve(mask);
        v.setValue(font);
        fontProperty->setValue(v);
        return true;
    }

    int FontPropertyManager::antialiasingToIndex(QFont::StyleStrategy antialias)
    {
        switch (antialias) {
        case QFont::PreferDefault:   return 0;
        case QFont::NoAntialias:     return 1;
        case QFont::PreferAntialias: return 2;
        default: break;
        }
        return 0;
    }

    QFont::StyleStrategy FontPropertyManager::indexToAntialiasing(int idx)
    {
        switch (idx) {
        case 0: return QFont::PreferDefault;
        case 1: return QFont::NoAntialias;
        case 2: return QFont::PreferAntialias;
        }
        return QFont::PreferDefault;
    }

    unsigned FontPropertyManager::fontFlag(int idx)
    {
        switch (idx) {
        case 0: return QFont::FamilyResolved;
        case 1: return QFont::SizeResolved;
        case 2: return QFont::WeightResolved;
        case 3: return QFont::StyleResolved;
        case 4: return QFont::UnderlineResolved;
        case 5: return QFont::StrikeOutResolved;
        case 6: return QFont::KerningResolved;
        case 7: return QFont::StyleStrategyResolved;
        }
        return 0;
    }

    int FontPropertyManager::valueChanged(QtVariantPropertyManager *vm, QtProperty *property, const QVariant &value)
    {
        QtProperty *antialiasingProperty = m_antialiasingToProperty.value(property, 0);
        if (!antialiasingProperty) {
            if (m_propertyToFontSubProperties.contains(property)) {
                updateModifiedState(property, value);
            }
            return DesignerPropertyManager::NoMatch;
        }

        QtVariantProperty *fontProperty = vm->variantProperty(antialiasingProperty);
        const QFont::StyleStrategy newValue = indexToAntialiasing(value.toInt());

        QFont font = qvariant_cast<QFont>(fontProperty->value());
        const QFont::StyleStrategy oldValue = font.styleStrategy();
        if (newValue == oldValue)
            return DesignerPropertyManager::Unchanged;

        font.setStyleStrategy(newValue);
        fontProperty->setValue(QVariant::fromValue(font));
        return DesignerPropertyManager::Changed;
    }

    void FontPropertyManager::updateModifiedState(QtProperty *property, const QVariant &value)
    {
        const PropertyToSubPropertiesMap::iterator it = m_propertyToFontSubProperties.find(property);
        if (it == m_propertyToFontSubProperties.end())
            return;

        const PropertyList &subProperties = it.value();

        QFont font = qvariant_cast<QFont>(value);
        const unsigned mask = font.resolve();

        const int count = subProperties.size();
        for (int index = 0; index < count; index++) {
             const unsigned flag = fontFlag(index);
             subProperties.at(index)->setModified(mask & flag);
        }
    }

    void FontPropertyManager::setValue(QtVariantPropertyManager *vm, QtProperty *property, const QVariant &value)
    {
        updateModifiedState(property, value);

        if (QtProperty *antialiasingProperty = m_propertyToAntialiasing.value(property, 0)) {
            QtVariantProperty *antialiasing = vm->variantProperty(antialiasingProperty);
            if (antialiasing) {
                QFont font = qvariant_cast<QFont>(value);
                antialiasing->setValue(antialiasingToIndex(font.styleStrategy()));
            }
        }
    }

    /* Parse a mappings file of the form:
     * <fontmappings>
     * <mapping><family>DejaVu Sans</family><display>DejaVu Sans [CE]</display></mapping>
     * ... which is used to display on which platforms fonts are available.*/

    static const char *rootTagC = "fontmappings";
    static const char *mappingTagC = "mapping";
    static const char *familyTagC = "family";
    static const char *displayTagC = "display";

    static QString msgXmlError(const QXmlStreamReader &r, const QString& fileName)
    {
        return QString::fromUtf8("An error has been encountered at line %1 of %2: %3:").arg(r.lineNumber()).arg(fileName, r.errorString());
    }

    /* Switch stages when encountering a start element (state table) */
    enum ParseStage { ParseBeginning, ParseWithinRoot, ParseWithinMapping, ParseWithinFamily,
                      ParseWithinDisplay, ParseError };

    static ParseStage nextStage(ParseStage currentStage, const QStringRef &startElement)
    {
        switch (currentStage) {
        case ParseBeginning:
            return startElement == QLatin1String(rootTagC) ? ParseWithinRoot : ParseError;
        case ParseWithinRoot:
        case ParseWithinDisplay: // Next mapping, was in <display>
            return startElement == QLatin1String(mappingTagC) ? ParseWithinMapping : ParseError;
        case ParseWithinMapping:
            return startElement == QLatin1String(familyTagC) ? ParseWithinFamily : ParseError;
        case ParseWithinFamily:
            return startElement == QLatin1String(displayTagC) ? ParseWithinDisplay : ParseError;
        case ParseError:
            break;
        }
        return  ParseError;
    }

    bool FontPropertyManager::readFamilyMapping(NameMap *rc, QString *errorMessage)
    {
        rc->clear();
        const QString fileName = QStringLiteral(":/qt-project.org/propertyeditor/fontmapping.xml");
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            *errorMessage = QString::fromUtf8("Unable to open %1: %2").arg(fileName, file.errorString());
            return false;
        }

        QXmlStreamReader reader(&file);
        QXmlStreamReader::TokenType token;

        QString family;
        ParseStage stage = ParseBeginning;
        do {
            token = reader.readNext();
            switch (token) {
            case QXmlStreamReader::Invalid:
                *errorMessage = msgXmlError(reader, fileName);
                 return false;
            case QXmlStreamReader::StartElement:
                stage = nextStage(stage, reader.name());
                switch (stage) {
                case ParseError:
                    reader.raiseError(QString::fromUtf8("Unexpected element <%1>.").arg(reader.name().toString()));
                    *errorMessage = msgXmlError(reader, fileName);
                    return false;
                case ParseWithinFamily:
                    family = reader.readElementText();
                    break;
                case ParseWithinDisplay:
                    rc->insert(family, reader.readElementText());
                    break;
                default:
                    break;
                }
            default:
                break;
            }
        } while (token != QXmlStreamReader::EndDocument);
        return true;
    }

}

QT_END_NAMESPACE
