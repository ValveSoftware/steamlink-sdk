/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

// Timeout values:

// A valid screen grab requires the scene to not change
// for SCENE_STABLE_TIME ms
#define SCENE_STABLE_TIME 200

// Give up after SCENE_TIMEOUT ms
#define SCENE_TIMEOUT     6000

//#define GRABBERDEBUG

class GrabbingView : public QQuickView
{
    Q_OBJECT

public:
    GrabbingView(const QString &outputFile)
        : ofile(outputFile), grabNo(0), isGrabbing(false), initDone(false)
    {
        grabTimer = new QTimer(this);
        grabTimer->setSingleShot(true);
        grabTimer->setInterval(SCENE_STABLE_TIME);
        connect(grabTimer, SIGNAL(timeout()), SLOT(grab()));

        connect(this, SIGNAL(afterRendering()), SLOT(startGrabbing()));

        QTimer::singleShot(SCENE_TIMEOUT, this, SLOT(timedOut()));
    }

private slots:
    void startGrabbing()
    {
        if (!initDone) {
            initDone = true;
            grabTimer->start();
        }
    }

    void grab()
    {
        if (isGrabbing)
            return;
        isGrabbing = true;
        grabNo++;
#ifdef GRABBERDEBUG
        printf("grab no. %i\n", grabNo);
#endif
        QImage img = grabWindow();
        if (!img.isNull() && img == lastGrab) {
            sceneStabilized();
        } else {
            lastGrab = img;
            grabTimer->start();
        }

        isGrabbing = false;
    }

    void sceneStabilized()
    {
#ifdef GRABBERDEBUG
        printf("...sceneStabilized IN\n");
#endif
        if (ofile == "-") {   // Write to stdout
            QFile of;
            if (!of.open(1, QIODevice::WriteOnly) || !lastGrab.save(&of, "ppm")) {
                qWarning() << "Error: failed to write grabbed image to stdout.";
                QGuiApplication::exit(2);
                return;
            }
        } else {
            if (!lastGrab.save(ofile)) {
                qWarning() << "Error: failed to store grabbed image to" << ofile;
                QGuiApplication::exit(2);
                return;
            }
        }
        QGuiApplication::exit(0);
#ifdef GRABBERDEBUG
        printf("...sceneStabilized OUT\n");
#endif
    }

    void timedOut()
    {
        qWarning() << "Error: timed out waiting for scene to stabilize." << grabNo << "grab(s) done. Last grab was" << (lastGrab.isNull() ? "invalid." : "valid.");
        QGuiApplication::exit(3);
    }

private:
    QImage lastGrab;
    QTimer *grabTimer;
    QString ofile;
    int grabNo;
    bool isGrabbing;
    bool initDone;
};


Q_CORE_EXPORT extern QBasicAtomicInt qt_qhash_seed;

int main(int argc, char *argv[])
{
    qt_qhash_seed = 0;

    QGuiApplication a(argc, argv);

    // Parse command line
    QString ifile, ofile;
    bool noText = false;
    QStringList args = a.arguments();
    int i = 0;
    bool argError = false;
    while (++i < args.size()) {
        QString arg = args.at(i);
        if ((arg == "-o") && (i < args.size()-1)) {
            ofile = args.at(++i);
        }
        else if (arg == "-notext") {
            noText = true;
        }
        else if (arg == "--cache-distance-fields") {
            ;
        }
        else if (ifile.isEmpty()) {
            ifile = arg;
        }
        else {
            argError = true;
            break;
        }
    }
    if (argError || ifile.isEmpty() || ofile.isEmpty()) {
        qWarning() << "Usage:" << args.at(0).toLatin1().constData() << "[-notext] <qml-infile> -o <outfile or - for ppm on stdout>";
        return 1;
    }

    QFileInfo ifi(ifile);
    if (!ifi.exists() || !ifi.isReadable() || !ifi.isFile()) {
        qWarning() << args.at(0).toLatin1().constData() << " error: unreadable input file" << ifile;
        return 1;
    }
    // End parsing

    GrabbingView v(ofile);
    v.setSource(QUrl::fromLocalFile(ifile));

    if (noText) {
        const QList<QQuickItem*> items = v.rootObject()->findChildren<QQuickItem*>();
        for (QQuickItem *item : items) {
            if (QByteArray(item->metaObject()->className()).contains("Text"))
                item->setVisible(false);
        }
    }

    v.show();

    int retVal = a.exec();
#ifdef GRABBERDEBUG
    printf("...retVal=%i\n", retVal);
#endif

    return retVal;
}

#include "main.moc"
