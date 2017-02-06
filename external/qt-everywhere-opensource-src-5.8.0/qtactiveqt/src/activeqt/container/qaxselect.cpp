/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the ActiveQt framework of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaxselect.h"

#include "ui_qaxselect.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSysInfo>
#include <QtCore/QTextStream>
#include <QtCore/QRegExp>
#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QPushButton>

#include <qt_windows.h>

#include <algorithm>
#include <functional>

QT_BEGIN_NAMESPACE

struct Control
{
    inline Control() : wordSize(0) {}
    int compare(const Control &rhs) const;
    QString toolTip() const;

    QString clsid;
    QString name;
    QString dll;
    QString version;
    unsigned wordSize;
};

inline int Control::compare(const Control &rhs) const
{
    // Sort reverse by word size to ensure that disabled 32bit controls
    // are listed last in 64bit executables.
    if (wordSize > rhs.wordSize)
        return -1;
    if (wordSize < rhs.wordSize)
        return 1;
    if (const int n = name.compare(rhs.name))
        return n;
    if (const int c = clsid.compare(rhs.clsid))
        return c;
    if (const int d = dll.compare(rhs.dll))
        return d;
    if (const int v = version.compare(rhs.version))
        return v;
    return 0;
}

QString Control::toolTip() const
{
    QString result;
    QTextStream str(&result);
    str << "<html><head/><body><table>"
        << "<tr><th>" << QAxSelect::tr("Name:") << "</th><td>" << name << "</td></tr>"
        << "<tr><th>" << QAxSelect::tr("CLSID:") << "</th><td>" << clsid << "</td></tr>"
        << "<tr><th>" << QAxSelect::tr("Word&nbsp;size:") << "</th><td>" << wordSize << "</td></tr>";
    if (!dll.isEmpty())
        str << "<tr><th>" << QAxSelect::tr("DLL:") << "</th><td>" << dll << "</td></tr>";
    if (!version.isEmpty())
        str << "<tr><th>" << QAxSelect::tr("Version:") << "</th><td>" << version << "</td></tr>";
    str << "</table></body></html>";
    result.replace(QStringLiteral(" "), QStringLiteral("&nbsp;"));
    return result;
}

inline bool operator<(const Control &c1, const Control &c2) { return c1.compare(c2) < 0; }
inline bool operator==(const Control &c1, const Control &c2) { return !c1.compare(c2); }

class FindByClsidPredicate : public std::unary_function<bool, Control> {
public:
    explicit FindByClsidPredicate(const QString &clsid) :  m_clsid(clsid) {}
    inline bool operator()(const Control &c) const { return m_clsid == c.clsid; }

private:
    const QString m_clsid;
};

static LONG RegistryQueryValue(HKEY hKey, LPCWSTR lpSubKey, LPBYTE lpData, LPDWORD lpcbData)
{
    LONG ret = ERROR_FILE_NOT_FOUND;
    HKEY hSubKey = NULL;
    RegOpenKeyEx(hKey, lpSubKey, 0, KEY_READ, &hSubKey);
    if (hSubKey) {
        ret = RegQueryValueEx(hSubKey, 0, 0, 0, lpData, lpcbData);
        RegCloseKey(hSubKey);
    }
    return ret;
}

static bool querySubKeyValue(HKEY hKey, const QString &subKeyName,  LPBYTE lpData, LPDWORD lpcbData)
{
    HKEY hSubKey = NULL;
    const LONG openResult = RegOpenKeyEx(hKey,  reinterpret_cast<const wchar_t *>(subKeyName.utf16()),
                                         0, KEY_READ, &hSubKey);
    if (openResult != ERROR_SUCCESS)
        return false;
    const bool result = RegQueryValueEx(hSubKey, 0, 0, 0, lpData, lpcbData) == ERROR_SUCCESS;
    RegCloseKey(hSubKey);
    return result;
}

static QList<Control> readControls(const wchar_t *rootKey, unsigned wordSize)
{
    QList<Control> controls;
    HKEY classesKey;
    RegOpenKeyEx(HKEY_CLASSES_ROOT, rootKey, 0, KEY_READ, &classesKey);
    if (!classesKey) {
        qErrnoWarning("RegOpenKeyEx failed.");
        return controls;
    }
    const QString systemRoot = QString::fromLocal8Bit(qgetenv("SystemRoot"));
    const QRegExp systemRootPattern(QStringLiteral("%SystemRoot%"), Qt::CaseInsensitive, QRegExp::FixedString);
    DWORD index = 0;
    LONG result = 0;
    wchar_t buffer[256];
    DWORD szBuffer = 0;
    FILETIME ft;
    do {
        szBuffer = sizeof(buffer) / sizeof(wchar_t);
        result = RegEnumKeyEx(classesKey, index, buffer, &szBuffer, 0, 0, 0, &ft);
        szBuffer = sizeof(buffer) / sizeof(wchar_t);
        if (result == ERROR_SUCCESS) {
            HKEY subKey;
            const QString clsid = QString::fromWCharArray(buffer);
            const QString key = clsid + QStringLiteral("\\Control");
            result = RegOpenKeyEx(classesKey, reinterpret_cast<const wchar_t *>(key.utf16()), 0, KEY_READ, &subKey);
            if (result == ERROR_SUCCESS) {
                RegCloseKey(subKey);
                szBuffer = sizeof(buffer) / sizeof(wchar_t);
                RegistryQueryValue(classesKey, buffer, reinterpret_cast<LPBYTE>(buffer), &szBuffer);
                Control control;
                control.clsid = clsid;
                control.wordSize = wordSize;
                control.name = QString::fromWCharArray(buffer);
                szBuffer = sizeof(buffer) / sizeof(wchar_t);
                if (querySubKeyValue(classesKey, clsid + QStringLiteral("\\InprocServer32"),
                                     reinterpret_cast<LPBYTE>(buffer), &szBuffer)) {
                    control.dll = QString::fromWCharArray(buffer);
                    control.dll.replace(systemRootPattern, systemRoot);
                }
                szBuffer = sizeof(buffer) / sizeof(wchar_t);
                if (querySubKeyValue(classesKey, clsid + QStringLiteral("\\VERSION"),
                                     reinterpret_cast<LPBYTE>(buffer), &szBuffer))
                    control.version = QString::fromWCharArray(buffer);
                controls.push_back(control);
            }
            result = ERROR_SUCCESS;
        }
        ++index;
    } while (result == ERROR_SUCCESS);
    RegCloseKey(classesKey);
    return controls;
}

class ControlList : public QAbstractListModel
{
public:
    ControlList(QObject *parent=0)
    : QAbstractListModel(parent)
    {
        m_controls = readControls(L"CLSID", unsigned(QSysInfo::WordSize));
        if (QSysInfo::WordSize == 64) { // Append the 32bit controls as disabled items.
            foreach (const Control &c, readControls(L"Wow6432Node\\CLSID", 32u)) {
                if (std::find_if(m_controls.constBegin(), m_controls.constEnd(), FindByClsidPredicate(c.clsid)) == m_controls.constEnd())
                    m_controls.append(c);
            }
        }
        std::sort(m_controls.begin(), m_controls.end());
    }

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_controls.count(); }
    QVariant data(const QModelIndex &index, int role) const override ;
    Qt::ItemFlags flags(const QModelIndex &index) const override ;

private:
    QList<Control> m_controls;
};

QVariant ControlList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return m_controls.at(index.row()).name;
    case Qt::ToolTipRole:
        return m_controls.at(index.row()).toolTip();
    case Qt::UserRole:
        return m_controls.at(index.row()).clsid;
    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags ControlList::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractListModel::flags(index);
    if (!index.isValid() || m_controls.at(index.row()).wordSize != QSysInfo::WordSize)
        result &= ~Qt::ItemIsEnabled;
    return result;
}

class QAxSelectPrivate {
public:
    inline QString clsidAt(const QModelIndex &index) const
    {
        if (index.isValid()) {
            const QModelIndex sourceIndex = filterModel->mapToSource(index);
            if (sourceIndex.isValid())
                return sourceIndex.data(Qt::UserRole).toString();
        }
        return QString();
    }

    inline QPushButton *okButton() const { return selectUi.buttonBox->button(QDialogButtonBox::Ok); }

    inline void setOkButtonEnabled(bool enabled) { okButton()->setEnabled(enabled); }

    Ui::QAxSelect selectUi;
    QSortFilterProxyModel *filterModel;
};

/*!
    \class QAxSelect
    \brief The QAxSelect class provides a selection dialog for registered COM components.

    \inmodule QAxContainer

    QAxSelect dialog can be used to provide users with a way to browse the registered COM
    components of the system and select one. The CLSID of the selected component can
    then be used in the application to e.g. initialize a QAxWidget:

    \snippet src_activeqt_container_qaxselect.cpp 0

    \sa QAxWidget, {ActiveQt Framework}
*/

/*!
    Constructs a QAxSelect object. Dialog parent widget and window flags can be
    optionally specified with \a parent and \a flags parameters, respectively.
*/
QAxSelect::QAxSelect(QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , d(new QAxSelectPrivate)
{
    setWindowFlags(windowFlags() &~ Qt::WindowContextHelpButtonHint);
    d->selectUi.setupUi(this);
    d->setOkButtonEnabled(false);

    const QRect availableGeometry = QApplication::desktop()->availableGeometry(this);
    resize(availableGeometry.width() / 4, availableGeometry.height() * 2 / 3);

#ifndef QT_NO_CURSOR
    QApplication::setOverrideCursor(Qt::WaitCursor);
#endif

    d->filterModel = new QSortFilterProxyModel(this);
    d->filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    d->filterModel->setSourceModel(new ControlList(this));
    d->selectUi.ActiveXList->setModel(d->filterModel);

    connect(d->selectUi.ActiveXList->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &QAxSelect::onActiveXListCurrentChanged);
    connect(d->selectUi.ActiveXList, &QAbstractItemView::activated,
            this, &QAxSelect::onActiveXListActivated);

#ifndef QT_NO_CURSOR
    QApplication::restoreOverrideCursor();
#endif
    d->selectUi.ActiveXList->setFocus();

    connect(d->selectUi.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(d->selectUi.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->selectUi.filterLineEdit, &QLineEdit::textChanged,
            this, &QAxSelect::onFilterLineEditChanged);
}

/*!
    Destroys the QAxSelect object.
*/
QAxSelect::~QAxSelect()
{
}

/*!
    \fn QString QAxSelect::clsid() const

    Returns the CLSID of the selected COM component.
*/
QString QAxSelect::clsid() const
{
    return d->selectUi.ActiveX->text().trimmed();
}

void QAxSelect::onActiveXListCurrentChanged(const QModelIndex &index)
{
    const QString newClsid = d->clsidAt(index);
    d->selectUi.ActiveX->setText(newClsid);
    d->setOkButtonEnabled(!newClsid.isEmpty());
}

void QAxSelect::onActiveXListActivated()
{
    if (!clsid().isEmpty())
        d->okButton()->animateClick();
}

void QAxSelect::onFilterLineEditChanged(const QString &text)
{
    d->filterModel->setFilterFixedString(text);
}

QT_END_NAMESPACE
