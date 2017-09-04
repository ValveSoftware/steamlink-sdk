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

#ifndef DESIGNERPROPERTYMANAGER_H
#define DESIGNERPROPERTYMANAGER_H

#include "qtvariantproperty.h"
#include "brushpropertymanager.h"
#include "fontpropertymanager.h"

#include <qdesigner_utils_p.h>
#include <shared_enums_p.h>

#include <QtCore/QUrl>
#include <QtCore/QMap>
#include <QtGui/QFont>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

typedef QPair<QString, uint> DesignerIntPair;
typedef QList<DesignerIntPair> DesignerFlagList;

class QDesignerFormEditorInterface;
class QLineEdit;
class QUrl;
class QKeySequenceEdit;

namespace qdesigner_internal
{

class ResetWidget;

class TextEditor;
class PaletteEditorButton;
class PixmapEditor;
class StringListEditorButton;
class FormWindowBase;

class ResetDecorator : public QObject
{
    Q_OBJECT
public:
    explicit ResetDecorator(const QDesignerFormEditorInterface *core, QObject *parent = Q_NULLPTR);
    ~ResetDecorator();

    void connectPropertyManager(QtAbstractPropertyManager *manager);
    QWidget *editor(QWidget *subEditor, bool resettable, QtAbstractPropertyManager *manager, QtProperty *property,
                QWidget *parent);
    void disconnectPropertyManager(QtAbstractPropertyManager *manager);
    void setSpacing(int spacing);
signals:
    void resetProperty(QtProperty *property);
private slots:
    void slotPropertyChanged(QtProperty *property);
    void slotEditorDestroyed(QObject *object);
private:
    QMap<QtProperty *, QList<ResetWidget *> > m_createdResetWidgets;
    QMap<ResetWidget *, QtProperty *> m_resetWidgetToProperty;
    int m_spacing;
    const QDesignerFormEditorInterface *m_core;
};

// Helper for handling sub-properties of properties inheriting PropertySheetTranslatableData
// (translatable, disambiguation, comment).
template <class PropertySheetValue>
class TranslatablePropertyManager
{
public:
    void initialize(QtVariantPropertyManager *m, QtProperty *property, const PropertySheetValue &value);
    bool uninitialize(QtProperty *property);
    bool destroy(QtProperty *subProperty);

    bool value(const QtProperty *property, QVariant *rc) const;
    int valueChanged(QtVariantPropertyManager *m, QtProperty *property,
                                    const QVariant &value);

    int setValue(QtVariantPropertyManager *m, QtProperty *property,
                 int expectedTypeId, const QVariant &value);

private:
    QMap<QtProperty *, PropertySheetValue> m_values;
    QMap<QtProperty *, QtProperty *> m_valueToComment;
    QMap<QtProperty *, QtProperty *> m_valueToTranslatable;
    QMap<QtProperty *, QtProperty *> m_valueToDisambiguation;

    QMap<QtProperty *, QtProperty *> m_commentToValue;
    QMap<QtProperty *, QtProperty *> m_translatableToValue;
    QMap<QtProperty *, QtProperty *> m_disambiguationToValue;
};

class DesignerPropertyManager : public QtVariantPropertyManager
{
    Q_OBJECT
public:
    enum ValueChangedResult { NoMatch, Unchanged, Changed };

    explicit DesignerPropertyManager(QDesignerFormEditorInterface *core, QObject *parent = 0);
    ~DesignerPropertyManager();

    QStringList attributes(int propertyType) const Q_DECL_OVERRIDE;
    int attributeType(int propertyType, const QString &attribute) const Q_DECL_OVERRIDE;

    QVariant attributeValue(const QtProperty *property, const QString &attribute) const Q_DECL_OVERRIDE;
    bool isPropertyTypeSupported(int propertyType) const Q_DECL_OVERRIDE;
    QVariant value(const QtProperty *property) const Q_DECL_OVERRIDE;
    int valueType(int propertyType) const Q_DECL_OVERRIDE;
    QString valueText(const QtProperty *property) const Q_DECL_OVERRIDE;
    QIcon valueIcon(const QtProperty *property) const Q_DECL_OVERRIDE;

    bool resetFontSubProperty(QtProperty *property);
    bool resetIconSubProperty(QtProperty *subProperty);

    void reloadResourceProperties();

    static int designerFlagTypeId();
    static int designerFlagListTypeId();
    static int designerAlignmentTypeId();
    static int designerPixmapTypeId();
    static int designerIconTypeId();
    static int designerStringTypeId();
    static int designerStringListTypeId();
    static int designerKeySequenceTypeId();

    void setObject(QObject *object) { m_object = object; }

public Q_SLOTS:
    void setAttribute(QtProperty *property, const QString &attribute, const QVariant &value) Q_DECL_OVERRIDE;
    void setValue(QtProperty *property, const QVariant &value) Q_DECL_OVERRIDE;
Q_SIGNALS:
    // sourceOfChange - a subproperty (or just property) which caused a change
    //void valueChanged(QtProperty *property, const QVariant &value, QtProperty *sourceOfChange);
    void valueChanged(QtProperty *property, const QVariant &value, bool enableSubPropertyHandling);
protected:
    void initializeProperty(QtProperty *property) Q_DECL_OVERRIDE;
    void uninitializeProperty(QtProperty *property) Q_DECL_OVERRIDE;
private Q_SLOTS:
    void slotValueChanged(QtProperty *property, const QVariant &value);
    void slotPropertyDestroyed(QtProperty *property);
private:
    void createIconSubProperty(QtProperty *iconProperty, QIcon::Mode mode, QIcon::State state, const QString &subName);

    typedef QMap<QtProperty *, bool> PropertyBoolMap;
    PropertyBoolMap m_resetMap;

    int bitCount(int mask) const;
    struct FlagData
    {
        uint val{0};
        DesignerFlagList flags;
        QList<uint> values;
    };
    typedef QMap<QtProperty *, FlagData> PropertyFlagDataMap;
    PropertyFlagDataMap m_flagValues;
    typedef  QMap<QtProperty *, QList<QtProperty *> > PropertyToPropertyListMap;
    PropertyToPropertyListMap m_propertyToFlags;
    QMap<QtProperty *, QtProperty *> m_flagToProperty;

    int alignToIndexH(uint align) const;
    int alignToIndexV(uint align) const;
    uint indexHToAlign(int idx) const;
    uint indexVToAlign(int idx) const;
    QString indexHToString(int idx) const;
    QString indexVToString(int idx) const;
    QMap<QtProperty *, uint> m_alignValues;
    typedef QMap<QtProperty *, QtProperty *> PropertyToPropertyMap;
    PropertyToPropertyMap m_propertyToAlignH;
    PropertyToPropertyMap m_propertyToAlignV;
    PropertyToPropertyMap m_alignHToProperty;
    PropertyToPropertyMap m_alignVToProperty;

    QMap<QtProperty *, QMap<QPair<QIcon::Mode, QIcon::State>, QtProperty *> > m_propertyToIconSubProperties;
    QMap<QtProperty *, QPair<QIcon::Mode, QIcon::State> > m_iconSubPropertyToState;
    PropertyToPropertyMap m_iconSubPropertyToProperty;
    PropertyToPropertyMap m_propertyToTheme;

    TranslatablePropertyManager<PropertySheetStringValue> m_stringManager;
    TranslatablePropertyManager<PropertySheetKeySequenceValue> m_keySequenceManager;
    TranslatablePropertyManager<PropertySheetStringListValue> m_stringListManager;

    struct PaletteData
    {
        QPalette val;
        QPalette superPalette;
    };
    typedef QMap<QtProperty *, PaletteData>  PropertyPaletteDataMap;
    PropertyPaletteDataMap m_paletteValues;

    QMap<QtProperty *, qdesigner_internal::PropertySheetPixmapValue> m_pixmapValues;
    QMap<QtProperty *, qdesigner_internal::PropertySheetIconValue> m_iconValues;

    QMap<QtProperty *, uint> m_uintValues;
    QMap<QtProperty *, qlonglong> m_longLongValues;
    QMap<QtProperty *, qulonglong> m_uLongLongValues;
    QMap<QtProperty *, QUrl> m_urlValues;
    QMap<QtProperty *, QByteArray> m_byteArrayValues;

    typedef QMap<QtProperty *, int>  PropertyIntMap;
    PropertyIntMap m_stringAttributes;
    typedef QMap<QtProperty *, QFont>  PropertyFontMap;
    PropertyFontMap m_stringFontAttributes;
    PropertyBoolMap m_stringThemeAttributes;

    BrushPropertyManager m_brushManager;
    FontPropertyManager m_fontManager;

    QMap<QtProperty *, QPixmap> m_defaultPixmaps;
    QMap<QtProperty *, QIcon> m_defaultIcons;

    bool m_changingSubValue;
    QDesignerFormEditorInterface *m_core;

    QObject *m_object;

    QtProperty *m_sourceOfChange;
};

class DesignerEditorFactory : public QtVariantEditorFactory
{
    Q_OBJECT
public:
    explicit DesignerEditorFactory(QDesignerFormEditorInterface *core, QObject *parent = 0);
    ~DesignerEditorFactory();
    void setSpacing(int spacing);
    void setFormWindowBase(FormWindowBase *fwb);
signals:
    void resetProperty(QtProperty *property);
protected:
    void connectPropertyManager(QtVariantPropertyManager *manager);
    QWidget *createEditor(QtVariantPropertyManager *manager, QtProperty *property,
                QWidget *parent);
    void disconnectPropertyManager(QtVariantPropertyManager *manager);
private slots:
    void slotEditorDestroyed(QObject *object);
    void slotAttributeChanged(QtProperty *property, const QString &attribute, const QVariant &value);
    void slotPropertyChanged(QtProperty *property);
    void slotValueChanged(QtProperty *property, const QVariant &value);
    void slotStringTextChanged(const QString &value);
    void slotKeySequenceChanged(const QKeySequence &value);
    void slotPaletteChanged(const QPalette &value);
    void slotPixmapChanged(const QString &value);
    void slotIconChanged(const QString &value);
    void slotIconThemeChanged(const QString &value);
    void slotUintChanged(const QString &value);
    void slotLongLongChanged(const QString &value);
    void slotULongLongChanged(const QString &value);
    void slotUrlChanged(const QString &value);
    void slotByteArrayChanged(const QString &value);
    void slotStringListChanged(const QStringList &value);
private:
    TextEditor *createTextEditor(QWidget *parent, TextPropertyValidationMode vm, const QString &value);

    ResetDecorator *m_resetDecorator;
    bool m_changingPropertyValue;
    QDesignerFormEditorInterface *m_core;
    FormWindowBase *m_fwb;

    int m_spacing;

    QMap<QtProperty *, QList<TextEditor *> >                m_stringPropertyToEditors;
    QMap<TextEditor *, QtProperty *>                        m_editorToStringProperty;
    QMap<QtProperty *, QList<QKeySequenceEdit *> >         m_keySequencePropertyToEditors;
    QMap<QKeySequenceEdit *, QtProperty *>                 m_editorToKeySequenceProperty;
    QMap<QtProperty *, QList<PaletteEditorButton *> >       m_palettePropertyToEditors;
    QMap<PaletteEditorButton *, QtProperty *>               m_editorToPaletteProperty;
    QMap<QtProperty *, QList<PixmapEditor *> >              m_pixmapPropertyToEditors;
    QMap<PixmapEditor *, QtProperty *>                      m_editorToPixmapProperty;
    QMap<QtProperty *, QList<PixmapEditor *> >              m_iconPropertyToEditors;
    QMap<PixmapEditor *, QtProperty *>                      m_editorToIconProperty;
    QMap<QtProperty *, QList<QLineEdit *> >                 m_uintPropertyToEditors;
    QMap<QLineEdit *, QtProperty *>                         m_editorToUintProperty;
    QMap<QtProperty *, QList<QLineEdit *> >                 m_longLongPropertyToEditors;
    QMap<QLineEdit *, QtProperty *>                         m_editorToLongLongProperty;
    QMap<QtProperty *, QList<QLineEdit *> >                 m_uLongLongPropertyToEditors;
    QMap<QLineEdit *, QtProperty *>                         m_editorToULongLongProperty;
    QMap<QtProperty *, QList<TextEditor *> >                m_urlPropertyToEditors;
    QMap<TextEditor *, QtProperty *>                        m_editorToUrlProperty;
    QMap<QtProperty *, QList<TextEditor *> >                m_byteArrayPropertyToEditors;
    QMap<TextEditor *, QtProperty *>                        m_editorToByteArrayProperty;
    QMap<QtProperty *, QList<StringListEditorButton *> >    m_stringListPropertyToEditors;
    QMap<StringListEditorButton *, QtProperty *>            m_editorToStringListProperty;
};

} // namespace qdesigner_internal

QT_END_NAMESPACE

Q_DECLARE_METATYPE(DesignerIntPair)
Q_DECLARE_METATYPE(DesignerFlagList)

#endif

