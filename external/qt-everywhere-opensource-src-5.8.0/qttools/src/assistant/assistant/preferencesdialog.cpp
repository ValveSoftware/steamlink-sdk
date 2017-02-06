/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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
#include "preferencesdialog.h"

#include "centralwidget.h"
#include "filternamedialog.h"
#include "fontpanel.h"
#include "helpenginewrapper.h"
#include "openpagesmanager.h"
#include "tracer.h"

#include <QtCore/QAbstractListModel>
#include <QtCore/QtAlgorithms>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QVector>

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QFileDialog>
#include <QtGui/QFontDatabase>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

#include <QtHelp/QHelpEngineCore>

#include <algorithm>

QT_BEGIN_NAMESPACE

struct RegisteredDocEntry
{
    QString nameSpace;
    QString fileName;
};

typedef QVector<RegisteredDocEntry> RegisteredDocEntries;

class RegisteredDocsModel : public QAbstractListModel {
public:

    explicit RegisteredDocsModel(const RegisteredDocEntries &e = RegisteredDocEntries(), QObject *parent = nullptr)
        : QAbstractListModel(parent), m_docEntries(e) {}

    int rowCount(const QModelIndex & = QModelIndex()) const override { return m_docEntries.size(); }
    QVariant data(const QModelIndex &index, int role) const override;

    bool contains(const QString &nameSpace) const
    {
        return m_docEntries.cend() !=
            std::find_if(m_docEntries.cbegin(), m_docEntries.cend(),
                         [nameSpace] (const RegisteredDocEntry &e) { return e.nameSpace == nameSpace; });
    }

    void append(const RegisteredDocEntry &e);

    const RegisteredDocEntries &docEntries() const { return m_docEntries; }
    void setDocEntries(const RegisteredDocEntries &);

private:
    RegisteredDocEntries m_docEntries;
};

QVariant RegisteredDocsModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    const int row = index.row();
    if (index.isValid() && row < m_docEntries.size()) {
        switch (role) {
        case Qt::DisplayRole:
            result = QVariant(m_docEntries.at(row).nameSpace);
            break;
        case Qt::ToolTipRole:
            result = QVariant(QDir::toNativeSeparators(m_docEntries.at(row).fileName));
            break;
        default:
            break;
        }
    }
    return result;
}

void RegisteredDocsModel::append(const RegisteredDocEntry &e)
{
    beginInsertRows(QModelIndex(), m_docEntries.size(), m_docEntries.size());
    m_docEntries.append(e);
    endInsertRows();
}

void RegisteredDocsModel::setDocEntries(const RegisteredDocEntries &e)
{
    beginResetModel();
    m_docEntries = e;
    endResetModel();
}

static RegisteredDocEntries registeredDocEntries(const HelpEngineWrapper &wrapper)
{
    RegisteredDocEntries result;
    const QStringList nameSpaces = wrapper.registeredDocumentations();
    result.reserve(nameSpaces.size());
    foreach (const QString &nameSpace, nameSpaces) {
        RegisteredDocEntry entry;
        entry.nameSpace = nameSpace;
        entry.fileName = wrapper.documentationFileName(nameSpace);
        result.append(entry);
    }
    return result;
}

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
    , m_appFontChanged(false)
    , m_browserFontChanged(false)
    , helpEngine(HelpEngineWrapper::instance())
    , m_hideFiltersTab(!helpEngine.filterFunctionalityEnabled())
    , m_hideDocsTab(!helpEngine.documentationManagerEnabled())
{
    TRACE_OBJ
    m_ui.setupUi(this);

    m_registeredDocsModel =
        new RegisteredDocsModel(m_hideDocsTab ? RegisteredDocEntries() : registeredDocEntries(helpEngine));
    m_registereredDocsFilterModel = new QSortFilterProxyModel(m_ui.registeredDocsListView);
    m_registereredDocsFilterModel->setSourceModel(m_registeredDocsModel);
    m_ui.registeredDocsListView->setModel(m_registereredDocsFilterModel);
    connect(m_ui.registeredDocsFilterLineEdit, &QLineEdit::textChanged,
            m_registereredDocsFilterModel, &QSortFilterProxyModel::setFilterFixedString);

    connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
        this, SLOT(applyChanges()));
    connect(m_ui.buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()),
        this, SLOT(reject()));

    if (!m_hideFiltersTab) {
        m_ui.attributeWidget->header()->hide();
        m_ui.attributeWidget->setRootIsDecorated(false);

        connect(m_ui.attributeWidget, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(updateFilterMap()));

        connect(m_ui.filterWidget,
            SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this,
            SLOT(updateAttributes(QListWidgetItem*)));

        connect(m_ui.filterAddButton, SIGNAL(clicked()), this,
            SLOT(addFilter()));
        connect(m_ui.filterRemoveButton, SIGNAL(clicked()), this,
            SLOT(removeFilter()));

        updateFilterPage();
    } else {
        m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.filtersTab));
    }

    if (!m_hideDocsTab) {
        connect(m_ui.docAddButton, SIGNAL(clicked()), this,
            SLOT(addDocumentationLocal()));
        connect(m_ui.docRemoveButton, SIGNAL(clicked()), this,
            SLOT(removeDocumentation()));

        m_docsBackup.reserve(m_registeredDocsModel->rowCount());
        foreach (const RegisteredDocEntry &e, m_registeredDocsModel->docEntries())
            m_docsBackup.append(e.nameSpace);
    } else {
        m_ui.tabWidget->removeTab(m_ui.tabWidget->indexOf(m_ui.docsTab));
    }

    updateFontSettingsPage();
    updateOptionsPage();

    if (helpEngine.usesAppFont())
        setFont(helpEngine.appFont());
}

PreferencesDialog::~PreferencesDialog()
{
    TRACE_OBJ
    if (m_appFontChanged) {
        helpEngine.setAppFont(m_appFontPanel->selectedFont());
        helpEngine.setUseAppFont(m_appFontPanel->isChecked());
        helpEngine.setAppWritingSystem(m_appFontPanel->writingSystem());
        emit updateApplicationFont();
    }

    if (m_browserFontChanged) {
        helpEngine.setBrowserFont(m_browserFontPanel->selectedFont());
        helpEngine.setUseBrowserFont(m_browserFontPanel->isChecked());
        helpEngine.setBrowserWritingSystem(m_browserFontPanel->writingSystem());
        emit updateBrowserFont();
    }

    QString homePage = m_ui.homePageLineEdit->text();
    if (homePage.isEmpty())
        homePage = QLatin1String("help");
    helpEngine.setHomePage(homePage);

    int option = m_ui.helpStartComboBox->currentIndex();
    helpEngine.setStartOption(option);
}

void PreferencesDialog::showDialog()
{
    TRACE_OBJ
    if (exec() != Accepted)
        m_appFontChanged = m_browserFontChanged = false;
}

void PreferencesDialog::updateFilterPage()
{
    TRACE_OBJ
    m_ui.filterWidget->clear();
    m_ui.attributeWidget->clear();

    m_filterMapBackup.clear();
    const QStringList &filters = helpEngine.customFilters();
    foreach (const QString &filter, filters) {
        if (filter == HelpEngineWrapper::TrUnfiltered())
            continue;
        QStringList atts = helpEngine.filterAttributes(filter);
        m_filterMapBackup.insert(filter, atts);
        if (!m_filterMap.contains(filter))
            m_filterMap.insert(filter, atts);
    }

    m_ui.filterWidget->addItems(m_filterMap.keys());

    foreach (const QString &a, helpEngine.filterAttributes())
        new QTreeWidgetItem(m_ui.attributeWidget, QStringList() << a);

    if (!m_filterMap.keys().isEmpty())
        m_ui.filterWidget->setCurrentRow(0);
}

void PreferencesDialog::updateAttributes(QListWidgetItem *item)
{
    TRACE_OBJ
    QStringList checkedList;
    if (item)
        checkedList = m_filterMap.value(item->text());
    QTreeWidgetItem *itm;
    for (int i = 0; i < m_ui.attributeWidget->topLevelItemCount(); ++i) {
        itm = m_ui.attributeWidget->topLevelItem(i);
        if (checkedList.contains(itm->text(0)))
            itm->setCheckState(0, Qt::Checked);
        else
            itm->setCheckState(0, Qt::Unchecked);
    }
}

void PreferencesDialog::updateFilterMap()
{
    TRACE_OBJ
    if (!m_ui.filterWidget->currentItem())
        return;
    QString filter = m_ui.filterWidget->currentItem()->text();
    if (!m_filterMap.contains(filter))
        return;

    QStringList newAtts;
    QTreeWidgetItem *itm = 0;
    for (int i = 0; i < m_ui.attributeWidget->topLevelItemCount(); ++i) {
        itm = m_ui.attributeWidget->topLevelItem(i);
        if (itm->checkState(0) == Qt::Checked)
            newAtts.append(itm->text(0));
    }
    m_filterMap[filter] = newAtts;
}

void PreferencesDialog::addFilter()
{
    TRACE_OBJ
    FilterNameDialog dia(this);
    if (dia.exec() == QDialog::Rejected)
        return;

    QString filterName = dia.filterName();
    if (!m_filterMap.contains(filterName)) {
        m_filterMap.insert(filterName, QStringList());
        m_ui.filterWidget->addItem(filterName);
    }

    QList<QListWidgetItem*> lst = m_ui.filterWidget
        ->findItems(filterName, Qt::MatchCaseSensitive);
    m_ui.filterWidget->setCurrentItem(lst.first());
}

void PreferencesDialog::removeFilter()
{
    TRACE_OBJ
    QListWidgetItem *item =
        m_ui.filterWidget ->takeItem(m_ui.filterWidget->currentRow());
    if (!item)
        return;

    m_filterMap.remove(item->text());
    m_removedFilters.append(item->text());
    delete item;
    if (m_ui.filterWidget->count())
        m_ui.filterWidget->setCurrentRow(0);
}

void PreferencesDialog::addDocumentationLocal()
{
    TRACE_OBJ
    const QStringList fileNames = QFileDialog::getOpenFileNames(this,
        tr("Add Documentation"), QString(), tr("Qt Compressed Help Files (*.qch)"));
    if (fileNames.isEmpty())
        return;

    QStringList invalidFiles;
    QStringList alreadyRegistered;
    foreach (const QString &fileName, fileNames) {
        const QString nameSpace = QHelpEngineCore::namespaceName(fileName);
        if (nameSpace.isEmpty()) {
            invalidFiles.append(fileName);
            continue;
        }

        if (m_registeredDocsModel->contains(nameSpace)) {
                alreadyRegistered.append(nameSpace);
                continue;
        }

        if (helpEngine.registerDocumentation(fileName)) {
            RegisteredDocEntry entry;
            entry.nameSpace = nameSpace;
            entry.fileName = fileName;
            m_registeredDocsModel->append(entry);
            m_regDocs.append(nameSpace);
            m_unregDocs.removeAll(nameSpace);
        }
    }

    if (!invalidFiles.isEmpty() || !alreadyRegistered.isEmpty()) {
        QString message;
        if (!alreadyRegistered.isEmpty()) {
            foreach (const QString &ns, alreadyRegistered) {
                message += tr("The namespace %1 is already registered!")
                    .arg(QString("<b>%1</b>").arg(ns)) + QLatin1String("<br>");
            }
            if (!invalidFiles.isEmpty())
                message.append(QLatin1String("<br>"));
        }

        if (!invalidFiles.isEmpty()) {
            message += tr("The specified file is not a valid Qt Help File!");
            message.append(QLatin1String("<ul>"));
            foreach (const QString &file, invalidFiles)
                message += QLatin1String("<li>") + file + QLatin1String("</li>");
            message.append(QLatin1String("</ul>"));
        }
        QMessageBox::warning(this, tr("Add Documentation"), message);
    }

    updateFilterPage();
}

QList<int> PreferencesDialog::currentRegisteredDocsSelection() const
{
    QList<int> result;
    foreach (const QModelIndex &index, m_ui.registeredDocsListView->selectionModel()->selectedRows())
        result.append(m_registereredDocsFilterModel->mapToSource(index).row());
    return result;
}

void PreferencesDialog::removeDocumentation()
{
    TRACE_OBJ

    const QList<int> currentSelection = currentRegisteredDocsSelection();
    if (currentSelection.isEmpty())
        return;

    RegisteredDocEntries entries = m_registeredDocsModel->docEntries();

    bool foundBefore = false;
    for (int i = currentSelection.size() - 1; i >= 0; --i) {
        const int row = currentSelection.at(i);
        const QString& ns = entries.at(row).nameSpace;
        if (!foundBefore && OpenPagesManager::instance()->pagesOpenForNamespace(ns)) {
            if (0 == QMessageBox::information(this, tr("Remove Documentation"),
                tr("Some documents currently opened in Assistant reference the "
                   "documentation you are attempting to remove. Removing the "
                   "documentation will close those documents."), tr("Cancel"),
                tr("OK"))) return;
            foundBefore = true;
        }

        m_unregDocs.append(ns);
        entries.removeAt(row);
    }

    m_registeredDocsModel->setDocEntries(entries);

    if (m_registereredDocsFilterModel->rowCount()) {
        const QModelIndex first = m_registereredDocsFilterModel->index(0, 0);
        m_ui.registeredDocsListView->selectionModel()->setCurrentIndex(first,
                                                                       QItemSelectionModel::ClearAndSelect);
    }
}

void PreferencesDialog::applyChanges()
{
    TRACE_OBJ
    bool filtersWereChanged = false;
    if (!m_hideFiltersTab) {
        if (m_filterMap.count() != m_filterMapBackup.count()) {
            filtersWereChanged = true;
        } else {
            QMapIterator<QString, QStringList> it(m_filterMapBackup);
            while (it.hasNext() && !filtersWereChanged) {
                it.next();
                if (!m_filterMap.contains(it.key())) {
                    filtersWereChanged = true;
                } else {
                    QStringList a = it.value();
                    QStringList b = m_filterMap.value(it.key());
                    if (a.count() != b.count()) {
                        filtersWereChanged = true;
                    } else {
                        QStringList::const_iterator i(a.constBegin());
                        while (i != a.constEnd()) {
                            if (!b.contains(*i)) {
                                filtersWereChanged = true;
                                break;
                            }
                            ++i;
                        }
                    }
                }
            }
        }
    }

    if (filtersWereChanged) {
        foreach (const QString &filter, m_removedFilters)
            helpEngine.removeCustomFilter(filter);
        QMapIterator<QString, QStringList> it(m_filterMap);
        while (it.hasNext()) {
            it.next();
            helpEngine.addCustomFilter(it.key(), it.value());
        }
    }

    foreach (const QString &doc, m_unregDocs) {
        OpenPagesManager::instance()->closePages(doc);
        helpEngine.unregisterDocumentation(doc);
    }

    if (filtersWereChanged || !m_regDocs.isEmpty() || !m_unregDocs.isEmpty())
        helpEngine.setupData();

    helpEngine.setShowTabs(m_ui.showTabs->isChecked());
    if (m_showTabs != m_ui.showTabs->isChecked())
        emit updateUserInterface();

    accept();
}

void PreferencesDialog::updateFontSettingsPage()
{
    TRACE_OBJ
    m_browserFontPanel = new FontPanel(this);
    m_browserFontPanel->setCheckable(true);
    m_ui.stackedWidget_2->insertWidget(0, m_browserFontPanel);

    m_appFontPanel = new FontPanel(this);
    m_appFontPanel->setCheckable(true);
    m_ui.stackedWidget_2->insertWidget(1, m_appFontPanel);

    m_ui.stackedWidget_2->setCurrentIndex(0);

    const QString customSettings(tr("Use custom settings"));
    m_appFontPanel->setTitle(customSettings);

    QFont font = helpEngine.appFont();
    m_appFontPanel->setSelectedFont(font);

    QFontDatabase::WritingSystem system = helpEngine.appWritingSystem();
    m_appFontPanel->setWritingSystem(system);

    m_appFontPanel->setChecked(helpEngine.usesAppFont());

    m_browserFontPanel->setTitle(customSettings);

    font = helpEngine.browserFont();
    m_browserFontPanel->setSelectedFont(font);

    system = helpEngine.browserWritingSystem();
    m_browserFontPanel->setWritingSystem(system);

    m_browserFontPanel->setChecked(helpEngine.usesBrowserFont());

    connect(m_appFontPanel, SIGNAL(toggled(bool)), this,
        SLOT(appFontSettingToggled(bool)));
    connect(m_browserFontPanel, SIGNAL(toggled(bool)), this,
        SLOT(browserFontSettingToggled(bool)));

    QList<QComboBox*> allCombos = m_appFontPanel->findChildren<QComboBox*>();
    foreach (QComboBox* box, allCombos) {
        connect(box, SIGNAL(currentIndexChanged(int)), this,
            SLOT(appFontSettingChanged(int)));
    }

    allCombos = m_browserFontPanel->findChildren<QComboBox*>();
    foreach (QComboBox* box, allCombos) {
        connect(box, SIGNAL(currentIndexChanged(int)), this,
            SLOT(browserFontSettingChanged(int)));
    }
}

void PreferencesDialog::appFontSettingToggled(bool on)
{
    TRACE_OBJ
    Q_UNUSED(on)
    m_appFontChanged = true;
}

void PreferencesDialog::appFontSettingChanged(int index)
{
    TRACE_OBJ
    Q_UNUSED(index)
    m_appFontChanged = true;
}

void PreferencesDialog::browserFontSettingToggled(bool on)
{
    TRACE_OBJ
    Q_UNUSED(on)
    m_browserFontChanged = true;
}

void PreferencesDialog::browserFontSettingChanged(int index)
{
    TRACE_OBJ
    Q_UNUSED(index)
    m_browserFontChanged = true;
}

void PreferencesDialog::updateOptionsPage()
{
    TRACE_OBJ
    m_ui.homePageLineEdit->setText(helpEngine.homePage());

    int option = helpEngine.startOption();
    m_ui.helpStartComboBox->setCurrentIndex(option);

    m_showTabs = helpEngine.showTabs();
    m_ui.showTabs->setChecked(m_showTabs);

    connect(m_ui.blankPageButton, SIGNAL(clicked()), this, SLOT(setBlankPage()));
    connect(m_ui.currentPageButton, SIGNAL(clicked()), this, SLOT(setCurrentPage()));
    connect(m_ui.defaultPageButton, SIGNAL(clicked()), this, SLOT(setDefaultPage()));
}

void PreferencesDialog::setBlankPage()
{
    TRACE_OBJ
    m_ui.homePageLineEdit->setText(QLatin1String("about:blank"));
}

void PreferencesDialog::setCurrentPage()
{
    TRACE_OBJ
    QString homepage = CentralWidget::instance()->currentSource().toString();
    if (homepage.isEmpty())
        homepage = QLatin1String("help");

    m_ui.homePageLineEdit->setText(homepage);
}

void PreferencesDialog::setDefaultPage()
{
    TRACE_OBJ
    m_ui.homePageLineEdit->setText(helpEngine.defaultHomePage());
}

QT_END_NAMESPACE
