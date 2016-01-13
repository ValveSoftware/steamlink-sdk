/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickView>
#include <QDir>
#include <QSettings>

static QString backendIdKey = QStringLiteral("backendId");

class BackendHelperContext : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString exampleName READ exampleName CONSTANT)
    Q_PROPERTY(QString backendId READ backendId WRITE setBackendId NOTIFY backendIdChanged)

    QString _backendId;
    QScopedPointer<QSettings> _settings;
    static QString _exampleName;

public:
    BackendHelperContext(QQuickView *parent)
        : QObject(parent)
    {
        QString fileName = QStringLiteral("EnginioExamples.conf");
        for (int i = 0; i < 4; ++i) {
            if (QFile::exists(fileName))
                break;
            fileName = fileName.prepend("../");
        }

        QFileInfo settingsFile = QFileInfo(fileName);
        _settings.reset(settingsFile.exists()
            ? new QSettings(settingsFile.absoluteFilePath(), QSettings::IniFormat)
            : new QSettings("com.digia", "EnginioExamples"));

        _settings->beginGroup(_exampleName);
        _backendId = _settings->value(backendIdKey).toString();
    }

    ~BackendHelperContext()
    {
        _settings->endGroup();
        _settings->sync();
    }

    QString backendId() const { return _backendId; }
    void setBackendId(const QString &backendId)
    {
        if (_backendId == backendId)
            return;
        _backendId = backendId;
        _settings->setValue(backendIdKey, _backendId);
        emit backendIdChanged();
    }

    QString exampleName() const { return _exampleName; }
signals:
    void backendIdChanged();
};
QString BackendHelperContext::_exampleName = ENGINIO_SAMPLE_NAME;

int main(int argc, char* argv[])
{
    QGuiApplication app(argc,argv);
    QQuickView view;
    const QString appPath = QCoreApplication::applicationDirPath();

    // This allows starting the example without previously defining QML2_IMPORT_PATH.
    QDir qmlImportDir(appPath);
#if defined (Q_OS_WIN)
    qmlImportDir.cd("..");
#endif
    qmlImportDir.cd("../../../qml");
    view.engine()->addImportPath(qmlImportDir.canonicalPath());
    QObject::connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));

    BackendHelperContext *backendContext = new BackendHelperContext(&view);

    view.engine()->rootContext()->setContextProperty("enginioBackendContext", backendContext);

    view.setSource(QUrl("qrc:///" ENGINIO_SAMPLE_NAME ".qml"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.show();
    return app.exec();
}

#include "main.moc"
