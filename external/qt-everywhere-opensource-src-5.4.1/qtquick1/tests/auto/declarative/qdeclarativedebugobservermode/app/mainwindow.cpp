#include "mainwindow.h"

#include <QDeclarativeError>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
}

MainWindow::~MainWindow()
{
    delete m_view;
}

bool MainWindow::loadQML(const QUrl &source)
{
    m_view = new QDeclarativeView(source, this);
    if (m_view->status() == QDeclarativeView::Error) {
        QList<QDeclarativeError> errors = m_view->errors();
        foreach (const QDeclarativeError &error, errors) {
            qWarning() << error.toString();
        }
        return false;
    }
    return true;
}
