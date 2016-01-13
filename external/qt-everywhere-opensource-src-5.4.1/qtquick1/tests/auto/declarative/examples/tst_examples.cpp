/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <qtest.h>
#include <QLibraryInfo>
#include <QDir>
#include <QDebug>
#include "qmlruntime.h"
#include <QDeclarativeView>
#include <QDeclarativeError>

class tst_examples : public QObject
{
    Q_OBJECT
public:
    tst_examples();

private slots:
    void examples_data();
    void examples();

    void namingConvention();
private:
    QStringList excludedDirs;

    void namingConvention(const QDir &);
    QStringList findQmlFiles(const QDir &);
};

tst_examples::tst_examples()
{
    // Add directories you want excluded here
    excludedDirs << "doc/src/snippets/declarative/visualdatamodel_rootindex"
                 << "doc/src/snippets/declarative/qtbinding"
                 << "examples/declarative/tutorials"; //Excluded when tutorials were moved into examples (not checked before)
    // Known to violate naming conventions, QTQAINFRA-428
    excludedDirs << "demos/mobile/qtbubblelevel/qml"
                 << "demos/mobile/quickhit";
    // Layouts do not install, QTQAINFRA-428
    excludedDirs << "examples/declarative/cppextensions/qgraphicslayouts/qgraphicsgridlayout/qml/qgraphicsgridlayout"
                 << "examples/declarative/cppextensions/qgraphicslayouts/qgraphicslinearlayout/qml/qgraphicslinearlayout";
    // Various QML errors, QTQAINFRA-428
    excludedDirs << "doc/src/snippets/declarative/imports";

#ifdef QT_NO_WEBKIT
    excludedDirs << "examples/declarative/modelviews/webview"
                 << "demos/declarative/webbrowser"
                 << "doc/src/snippets/declarative/webview";
#endif

#ifdef QT_NO_XMLPATTERNS
    excludedDirs << "examples/declarative/xml/xmldata"
                 << "demos/declarative/twitter"
                 << "demos/declarative/flickr"
                 << "demos/declarative/photoviewer"
                 << "demos/declarative/rssnews/qml/rssnews"
                 << "doc/src/snippets/declarative";
#endif
}

/*
This tests that the demos and examples follow the naming convention required
to have them tested by the examples() test.
*/
void tst_examples::namingConvention(const QDir &d)
{
    for (int ii = 0; ii < excludedDirs.count(); ++ii) {
        QString s = excludedDirs.at(ii);
        if (d.absolutePath().endsWith(s))
            return;
    }

    QStringList files = d.entryList(QStringList() << QLatin1String("*.qml"),
                                    QDir::Files);

    bool seenQml = !files.isEmpty();
    bool seenLowercase = false;

    foreach (const QString &file, files) {
        if (file.at(0).isLower())
            seenLowercase = true;
    }

    if (!seenQml) {
        QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                QDir::NoSymLinks);
        foreach (const QString &dir, dirs) {
            QDir sub = d;
            sub.cd(dir);
            namingConvention(sub);
        }
    } else if(!seenLowercase) {
        // QTBUG-28271 don't fail, but rather warn only
        qWarning() << QString(
             "Directory %1 violates naming convention; expected at least one qml file "
                 "starting with lower case, got: %2"
        ).arg(d.absolutePath()).arg(files.join(","));

        // QTest::qFail(QString("Directory " + d.absolutePath() + " violates naming convention").toLatin1().constData(), __FILE__, __LINE__);
    }
}

void tst_examples::namingConvention()
{
    QString examples = QLibraryInfo::location(QLibraryInfo::ExamplesPath);

    namingConvention(QDir(examples));
}

QStringList tst_examples::findQmlFiles(const QDir &d)
{
    const QString absolutePath = d.absolutePath();
#ifdef Q_OS_MAC // Mac: Do not recurse into bundle folders of built examples.
    if (absolutePath.endsWith(QLatin1String(".app")))
        return QStringList();
#endif
    foreach (const QString &excludedDir, excludedDirs)
        if (absolutePath.endsWith(excludedDir))
            return QStringList();

    QStringList rv;

    QStringList cppfiles = d.entryList(QStringList() << QLatin1String("*.cpp"), QDir::Files);
    if (cppfiles.isEmpty()) {
        QStringList files = d.entryList(QStringList() << QLatin1String("*.qml"),
                                        QDir::Files);
        foreach (const QString &file, files) {
            if (file.at(0).isLower()) {
                rv << d.absoluteFilePath(file);
            }
        }
    }

    QStringList dirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot |
                                   QDir::NoSymLinks);
    foreach (const QString &dir, dirs) {
        QDir sub = d;
        sub.cd(dir);
        rv << findQmlFiles(sub);
    }

    return rv;
}

/*
This test runs all the examples in the declarative UI source tree and ensures
that they start and exit cleanly.

Examples are any .qml files under the examples/ or demos/ directory that start
with a lower case letter.
*/
void tst_examples::examples_data()
{
    QTest::addColumn<QString>("file");

    QString examples = QLatin1String(SRCDIR) + "/../../../../demos/declarative/";
    QString demos = QLatin1String(SRCDIR) + "/../../../../examples/declarative/";
    QString snippets = QLatin1String(SRCDIR) + "/../../../../doc/src/snippets/";

    QStringList files;
    files << findQmlFiles(QDir(examples));
    files << findQmlFiles(QDir(demos));
    files << findQmlFiles(QDir(snippets));

    foreach (const QString &file, files)
        QTest::newRow(qPrintable(file)) << file;
}

static void silentErrorsMsgHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
}

static inline QByteArray msgViewerErrors(const QList<QDeclarativeError> &l)
{
    QString errors;
    QDebug(&errors) << '\n' << l;
    return errors.toLocal8Bit();
}

void tst_examples::examples()
{
    QFETCH(QString, file);

    QDeclarativeViewer viewer;

    QtMessageHandler old = qInstallMessageHandler(silentErrorsMsgHandler);
    QVERIFY(viewer.open(file));
    qInstallMessageHandler(old);
    QVERIFY2(viewer.view()->status() != QDeclarativeView::Error,
             msgViewerErrors(viewer.view()->errors()).constData());
    QTRY_VERIFY(viewer.view()->status() != QDeclarativeView::Loading);
    QVERIFY2(viewer.view()->status() == QDeclarativeView::Ready,
             msgViewerErrors(viewer.view()->errors()).constData());

    viewer.show();

    QVERIFY(QTest::qWaitForWindowExposed(&viewer, 3000));
}

QTEST_MAIN(tst_examples)

#include "tst_examples.moc"
