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

#include "newdynamicpropertydialog.h"
#include "ui_newdynamicpropertydialog.h"
#include <abstractdialoggui_p.h>
#include <qdesigner_propertysheet_p.h>

#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

namespace qdesigner_internal {

NewDynamicPropertyDialog::NewDynamicPropertyDialog(QDesignerDialogGuiInterface *dialogGui,
                                                       QWidget *parent)   :
    QDialog(parent),
    m_dialogGui(dialogGui),
    m_ui(new Ui::NewDynamicPropertyDialog)
{
    m_ui->setupUi(this);
    connect(m_ui->m_lineEdit, &QLineEdit::textChanged, this, &NewDynamicPropertyDialog::nameChanged);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_ui->m_comboBox->addItem(QStringLiteral("String"),      QVariant(QVariant::String));
    m_ui->m_comboBox->addItem(QStringLiteral("StringList"),  QVariant(QVariant::StringList));
    m_ui->m_comboBox->addItem(QStringLiteral("Char"),        QVariant(QVariant::Char));
    m_ui->m_comboBox->addItem(QStringLiteral("ByteArray"),   QVariant(QVariant::ByteArray));
    m_ui->m_comboBox->addItem(QStringLiteral("Url"),         QVariant(QVariant::Url));
    m_ui->m_comboBox->addItem(QStringLiteral("Bool"),        QVariant(QVariant::Bool));
    m_ui->m_comboBox->addItem(QStringLiteral("Int"),         QVariant(QVariant::Int));
    m_ui->m_comboBox->addItem(QStringLiteral("UInt"),        QVariant(QVariant::UInt));
    m_ui->m_comboBox->addItem(QStringLiteral("LongLong"),    QVariant(QVariant::LongLong));
    m_ui->m_comboBox->addItem(QStringLiteral("ULongLong"),   QVariant(QVariant::ULongLong));
    m_ui->m_comboBox->addItem(QStringLiteral("Double"),      QVariant(QVariant::Double));
    m_ui->m_comboBox->addItem(QStringLiteral("Size"),        QVariant(QVariant::Size));
    m_ui->m_comboBox->addItem(QStringLiteral("SizeF"),       QVariant(QVariant::SizeF));
    m_ui->m_comboBox->addItem(QStringLiteral("Point"),       QVariant(QVariant::Point));
    m_ui->m_comboBox->addItem(QStringLiteral("PointF"),      QVariant(QVariant::PointF));
    m_ui->m_comboBox->addItem(QStringLiteral("Rect"),        QVariant(QVariant::Rect));
    m_ui->m_comboBox->addItem(QStringLiteral("RectF"),       QVariant(QVariant::RectF));
    m_ui->m_comboBox->addItem(QStringLiteral("Date"),        QVariant(QVariant::Date));
    m_ui->m_comboBox->addItem(QStringLiteral("Time"),        QVariant(QVariant::Time));
    m_ui->m_comboBox->addItem(QStringLiteral("DateTime"),    QVariant(QVariant::DateTime));
    m_ui->m_comboBox->addItem(QStringLiteral("Font"),        QVariant(QVariant::Font));
    m_ui->m_comboBox->addItem(QStringLiteral("Palette"),     QVariant(QVariant::Palette));
    m_ui->m_comboBox->addItem(QStringLiteral("Color"),       QVariant(QVariant::Color));
    m_ui->m_comboBox->addItem(QStringLiteral("Pixmap"),      QVariant(QVariant::Pixmap));
    m_ui->m_comboBox->addItem(QStringLiteral("Icon"),        QVariant(QVariant::Icon));
    m_ui->m_comboBox->addItem(QStringLiteral("Cursor"),      QVariant(QVariant::Cursor));
    m_ui->m_comboBox->addItem(QStringLiteral("SizePolicy"),  QVariant(QVariant::SizePolicy));
    m_ui->m_comboBox->addItem(QStringLiteral("KeySequence"), QVariant(QVariant::KeySequence));

    m_ui->m_comboBox->setCurrentIndex(0); // String
    setOkButtonEnabled(false);
}

void NewDynamicPropertyDialog::setOkButtonEnabled(bool e)
{
    m_ui->m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(e);
}

NewDynamicPropertyDialog::~NewDynamicPropertyDialog()
{
    delete m_ui;
}

void NewDynamicPropertyDialog::setReservedNames(const QStringList &names)
{
    m_reservedNames = names;
}

void NewDynamicPropertyDialog::setPropertyType(QVariant::Type t)
{
    const int index = m_ui->m_comboBox->findData(QVariant(t));
    if (index != -1)
        m_ui->m_comboBox->setCurrentIndex(index);
}

QString NewDynamicPropertyDialog::propertyName() const
{
    return m_ui->m_lineEdit->text();
}

QVariant NewDynamicPropertyDialog::propertyValue() const
{
    const int index = m_ui->m_comboBox->currentIndex();
    if (index == -1)
        return QVariant();
    return m_ui->m_comboBox->itemData(index);
}

void NewDynamicPropertyDialog::information(const QString &message)
{
    m_dialogGui->message(this, QDesignerDialogGuiInterface::PropertyEditorMessage, QMessageBox::Information, tr("Set Property Name"), message);
}

void NewDynamicPropertyDialog::nameChanged(const QString &s)
{
    setOkButtonEnabled(!s.isEmpty());
}

bool NewDynamicPropertyDialog::validatePropertyName(const QString& name)
{
    if (m_reservedNames.contains(name)) {
        information(tr("The current object already has a property named '%1'.\nPlease select another, unique one.").arg(name));
        return false;
    }
    if (!QDesignerPropertySheet::internalDynamicPropertiesEnabled() && name.startsWith(QStringLiteral("_q_"))) {
        information(tr("The '_q_' prefix is reserved for the Qt library.\nPlease select another name."));
        return false;
    }
    return true;
}

void NewDynamicPropertyDialog::on_m_buttonBox_clicked(QAbstractButton *btn)
{
    const int role = m_ui->m_buttonBox->buttonRole(btn);
    switch (role) {
        case QDialogButtonBox::RejectRole:
            reject();
            break;
        case QDialogButtonBox::AcceptRole:
            if (validatePropertyName(propertyName()))
                accept();
            break;
    }
}
}

QT_END_NAMESPACE
