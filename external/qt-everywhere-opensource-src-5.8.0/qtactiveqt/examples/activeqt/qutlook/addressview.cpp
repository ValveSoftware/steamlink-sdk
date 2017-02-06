/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

//! [0]
#include "addressview.h"
#include "msoutl.h"
#include <QtWidgets>

class AddressBookModel : public QAbstractListModel
{
public:
    AddressBookModel(AddressView *parent);
    ~AddressBookModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;

    void changeItem(const QModelIndex &index, const QString &firstName, const QString &lastName, const QString &address, const QString &email);
    void addItem(const QString &firstName, const QString &lastName, const QString &address, const QString &email);
    void update();

private:
    Outlook::Application outlook;
    Outlook::Items * contactItems;

    mutable QHash<QModelIndex, QStringList> cache;
};
//! [0] //! [1]

AddressBookModel::AddressBookModel(AddressView *parent)
: QAbstractListModel(parent)
{
    if (!outlook.isNull()) {
        Outlook::NameSpace session(outlook.Session());
        session.Logon();
        Outlook::MAPIFolder *folder = session.GetDefaultFolder(Outlook::olFolderContacts);
        contactItems = new Outlook::Items(folder->Items());
        connect(contactItems, SIGNAL(ItemAdd(IDispatch*)), parent, SLOT(updateOutlook()));
        connect(contactItems, SIGNAL(ItemChange(IDispatch*)), parent, SLOT(updateOutlook()));
        connect(contactItems, SIGNAL(ItemRemove()), parent, SLOT(updateOutlook()));

        delete folder;
    }
}

//! [1] //! [2]
AddressBookModel::~AddressBookModel()
{
    delete contactItems;

    if (!outlook.isNull())
        Outlook::NameSpace(outlook.Session()).Logoff();
}

//! [2] //! [3]
int AddressBookModel::rowCount(const QModelIndex &) const
{
    return contactItems ? contactItems->Count() : 0;
}

int AddressBookModel::columnCount(const QModelIndex &parent) const
{
    return 4;
}

//! [3] //! [4]
QVariant AddressBookModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case 0:
        return tr("First Name");
    case 1:
        return tr("Last Name");
    case 2:
        return tr("Address");
    case 3:
        return tr("Email");
    default:
        break;
    }

    return QVariant();
}

//! [4] //! [5]
QVariant AddressBookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();

    QStringList data;
    if (cache.contains(index)) {
        data = cache.value(index);
    } else {
        Outlook::ContactItem contact(contactItems->Item(index.row() + 1));
        data << contact.FirstName() << contact.LastName() << contact.HomeAddress() << contact.Email1Address();
        cache.insert(index, data);
    }

    if (index.column() < data.count())
        return data.at(index.column());

    return QVariant();
}

//! [5] //! [6]
void AddressBookModel::changeItem(const QModelIndex &index, const QString &firstName, const QString &lastName, const QString &address, const QString &email)
{
    Outlook::ContactItem item(contactItems->Item(index.row() + 1));

    item.SetFirstName(firstName);
    item.SetLastName(lastName);
    item.SetHomeAddress(address);
    item.SetEmail1Address(email);

    item.Save();

    cache.take(index);
}

//! [6] //! [7]
void AddressBookModel::addItem(const QString &firstName, const QString &lastName, const QString &address, const QString &email)
{
    Outlook::ContactItem item(outlook.CreateItem(Outlook::olContactItem));
    if (!item.isNull()) {
        item.SetFirstName(firstName);
        item.SetLastName(lastName);
        item.SetHomeAddress(address);
        item.SetEmail1Address(email);

        item.Save();
    }
}

//! [7] //! [8]
void AddressBookModel::update()
{
    beginResetModel();
    cache.clear();
    endResetModel();
}


//! [8] //! [9]
AddressView::AddressView(QWidget *parent)
: QWidget(parent)
{
    QGridLayout *mainGrid = new QGridLayout(this);

    QLabel *liFirstName = new QLabel("First &Name", this);
    liFirstName->resize(liFirstName->sizeHint());
    mainGrid->addWidget(liFirstName, 0, 0);

    QLabel *liLastName = new QLabel("&Last Name", this);
    liLastName->resize(liLastName->sizeHint());
    mainGrid->addWidget(liLastName, 0, 1);

    QLabel *liAddress = new QLabel("Add&ress", this);
    liAddress->resize(liAddress->sizeHint());
    mainGrid->addWidget(liAddress, 0, 2);

    QLabel *liEMail = new QLabel("&E-Mail", this);
    liEMail->resize(liEMail->sizeHint());
    mainGrid->addWidget(liEMail, 0, 3);

    add = new QPushButton("A&dd", this);
    add->resize(add->sizeHint());
    mainGrid->addWidget(add, 0, 4);
    connect(add, SIGNAL(clicked()), this, SLOT(addEntry()));

    iFirstName = new QLineEdit(this);
    iFirstName->resize(iFirstName->sizeHint());
    mainGrid->addWidget(iFirstName, 1, 0);
    liFirstName->setBuddy(iFirstName);

    iLastName = new QLineEdit(this);
    iLastName->resize(iLastName->sizeHint());
    mainGrid->addWidget(iLastName, 1, 1);
    liLastName->setBuddy(iLastName);

    iAddress = new QLineEdit(this);
    iAddress->resize(iAddress->sizeHint());
    mainGrid->addWidget(iAddress, 1, 2);
    liAddress->setBuddy(iAddress);

    iEMail = new QLineEdit(this);
    iEMail->resize(iEMail->sizeHint());
    mainGrid->addWidget(iEMail, 1, 3);
    liEMail->setBuddy(iEMail);

    change = new QPushButton("&Change", this);
    change->resize(change->sizeHint());
    mainGrid->addWidget(change, 1, 4);
    connect(change, SIGNAL(clicked()), this, SLOT(changeEntry()));

    treeView = new QTreeView(this);
    treeView->setSelectionMode(QTreeView::SingleSelection);
    treeView->setRootIsDecorated(false);

    model = new AddressBookModel(this);
    treeView->setModel(model);

    connect(treeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(itemSelected(QModelIndex)));

    mainGrid->addWidget(treeView, 2, 0, 1, 5);
}

void AddressView::updateOutlook()
{
    model->update();
}

void AddressView::addEntry()
{
    if (!iFirstName->text().isEmpty() || !iLastName->text().isEmpty() ||
         !iAddress->text().isEmpty() || !iEMail->text().isEmpty()) {
        model->addItem(iFirstName->text(), iFirstName->text(), iAddress->text(), iEMail->text());
    }

    iFirstName->setText("");
    iLastName->setText("");
    iAddress->setText("");
    iEMail->setText("");
}

void AddressView::changeEntry()
{
    QModelIndex current = treeView->currentIndex();

    if (current.isValid())
        model->changeItem(current, iFirstName->text(), iLastName->text(), iAddress->text(), iEMail->text());
}

//! [9] //! [10]
void AddressView::itemSelected(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QAbstractItemModel *model = treeView->model();
    iFirstName->setText(model->data(model->index(index.row(), 0)).toString());
    iLastName->setText(model->data(model->index(index.row(), 1)).toString());
    iAddress->setText(model->data(model->index(index.row(), 2)).toString());
    iEMail->setText(model->data(model->index(index.row(), 3)).toString());
}
//! [10]
