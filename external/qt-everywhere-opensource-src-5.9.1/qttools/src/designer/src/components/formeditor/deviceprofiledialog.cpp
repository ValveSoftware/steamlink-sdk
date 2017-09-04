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

#include "deviceprofiledialog.h"
#include "ui_deviceprofiledialog.h"

#include <abstractdialoggui_p.h>
#include <deviceprofile_p.h>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyleFactory>
#include <QtGui/QFontDatabase>

#include <QtCore/QFileInfo>
#include <QtCore/QFile>

QT_BEGIN_NAMESPACE

static const char *profileExtensionC = "qdp";

static inline QString fileFilter()
{
    return qdesigner_internal::DeviceProfileDialog::tr("Device Profiles (*.%1)").arg(QLatin1String(profileExtensionC));
}

// Populate a combo with a sequence of integers, also set them as data.
template <class IntIterator>
    static void populateNumericCombo(IntIterator i1, IntIterator i2, QComboBox *cb)
{
    QString s;
    cb->setEditable(false);
    for ( ; i1 != i2 ; ++i1) {
        const int n = *i1;
        s.setNum(n);
        cb->addItem(s, QVariant(n));
    }
}

namespace qdesigner_internal {

DeviceProfileDialog::DeviceProfileDialog(QDesignerDialogGuiInterface *dlgGui, QWidget *parent) :
    QDialog(parent),
    m_ui(new Ui::DeviceProfileDialog),
    m_dlgGui(dlgGui)
{
    setModal(true);
    m_ui->setupUi(this);

    const QList<int> standardFontSizes = QFontDatabase::standardSizes();
    populateNumericCombo(standardFontSizes.constBegin(), standardFontSizes.constEnd(), m_ui->m_systemFontSizeCombo);

    // Styles
    const QStringList styles = QStyleFactory::keys();
    m_ui->m_styleCombo->addItem(tr("Default"), QVariant(QString()));
    const QStringList::const_iterator cend = styles.constEnd();
    for (QStringList::const_iterator it = styles.constBegin(); it != cend; ++it)
         m_ui->m_styleCombo->addItem(*it, *it);

    connect(m_ui->m_nameLineEdit, &QLineEdit::textChanged, this, &DeviceProfileDialog::nameChanged);
    connect(m_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_ui->buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            this, &QDialog::accept);
    // Note that Load/Save emit accepted() of the button box..
    connect(m_ui->buttonBox->button(QDialogButtonBox::Save), &QAbstractButton::clicked,
            this, &DeviceProfileDialog::save);
    connect(m_ui->buttonBox->button(QDialogButtonBox::Open), &QAbstractButton::clicked,
            this, &DeviceProfileDialog::open);
}

DeviceProfileDialog::~DeviceProfileDialog()
{
    delete m_ui;
}

DeviceProfile DeviceProfileDialog::deviceProfile() const
{
    DeviceProfile rc;
    rc.setName(m_ui->m_nameLineEdit->text());
    rc.setFontFamily(m_ui->m_systemFontComboBox->currentFont().family());
    rc.setFontPointSize(m_ui->m_systemFontSizeCombo->itemData(m_ui->m_systemFontSizeCombo->currentIndex()).toInt());

    int dpiX, dpiY;
    m_ui->m_dpiChooser->getDPI(&dpiX, &dpiY);
    rc.setDpiX(dpiX);
    rc.setDpiY(dpiY);

    rc.setStyle(m_ui->m_styleCombo->itemData(m_ui->m_styleCombo->currentIndex()).toString());

    return rc;
}

void DeviceProfileDialog::setDeviceProfile(const DeviceProfile &s)
{
    m_ui->m_nameLineEdit->setText(s.name());
    m_ui->m_systemFontComboBox->setCurrentFont(QFont(s.fontFamily()));
    const int fontSizeIndex = m_ui->m_systemFontSizeCombo->findData(QVariant(s.fontPointSize()));
    m_ui->m_systemFontSizeCombo->setCurrentIndex(fontSizeIndex != -1 ? fontSizeIndex : 0);
    m_ui->m_dpiChooser->setDPI(s.dpiX(), s.dpiY());
    const int styleIndex = m_ui->m_styleCombo->findData(s.style());
    m_ui->m_styleCombo->setCurrentIndex(styleIndex != -1 ? styleIndex : 0);
}

void DeviceProfileDialog::setOkButtonEnabled(bool v)
{
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(v);
}

bool DeviceProfileDialog::showDialog(const QStringList &existingNames)
{
    m_existingNames = existingNames;
    m_ui->m_nameLineEdit->setFocus(Qt::OtherFocusReason);
    nameChanged(m_ui->m_nameLineEdit->text());
    return exec() == Accepted;
}

void DeviceProfileDialog::nameChanged(const QString &name)
{
    const bool invalid = name.isEmpty() || m_existingNames.indexOf(name) != -1;
    setOkButtonEnabled(!invalid);
}

void DeviceProfileDialog::save()
{
    QString fn = m_dlgGui->getSaveFileName(this, tr("Save Profile"), QString(), fileFilter());
    if (fn.isEmpty())
        return;
    if (QFileInfo(fn).completeSuffix().isEmpty()) {
        fn += QLatin1Char('.');
        fn += QLatin1String(profileExtensionC);
    }

    QFile file(fn);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        critical(tr("Save Profile - Error"), tr("Unable to open the file '%1' for writing: %2").arg(fn, file.errorString()));
        return;
    }
    file.write(deviceProfile().toXml().toUtf8());
}

void DeviceProfileDialog::open()
{
    const QString fn = m_dlgGui->getOpenFileName(this, tr("Open profile"), QString(), fileFilter());
    if (fn.isEmpty())
        return;

    QFile file(fn);
    if (!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        critical(tr("Open Profile - Error"), tr("Unable to open the file '%1' for reading: %2").arg(fn, file.errorString()));
        return;
    }
    QString errorMessage;
    DeviceProfile newSettings;
    if (!newSettings.fromXml(QString::fromUtf8(file.readAll()), &errorMessage)) {
        critical(tr("Open Profile - Error"), tr("'%1' is not a valid profile: %2").arg(fn, errorMessage));
        return;
    }
    setDeviceProfile(newSettings);
}

void DeviceProfileDialog::critical(const QString &title, const QString &msg)
{
    m_dlgGui->message(this, QDesignerDialogGuiInterface::OtherMessage, QMessageBox::Critical, title, msg);
}
}

QT_END_NAMESPACE
