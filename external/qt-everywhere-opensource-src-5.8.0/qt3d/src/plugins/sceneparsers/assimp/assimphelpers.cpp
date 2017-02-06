/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "assimphelpers.h"

#include <QFileDevice>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QDebug>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace AssimpHelper {
/*!
 *  \class Qt3DRender::AssimpHelper::AssimpIOStream
 *
 *  \internal
 *
 *  \brief Provides a custom file stream class to be used by AssimpIOSystem.
 *
 */

/*!
 *  Builds a new AssimpIOStream instance.
 */
AssimpIOStream::AssimpIOStream(QIODevice *device) :
    Assimp::IOStream(),
    m_device(device)
{
    Q_ASSERT(m_device != nullptr);
}

/*!
 *  Clears an AssimpIOStream instance before deletion.
 */
AssimpIOStream::~AssimpIOStream()
{
    // Owns m_device
    delete m_device;
}

/*!
 *  Reads at most \a pCount elements of \a pSize bytes of data into \a pvBuffer.
 *  Returns the number of bytes read or -1 if an error occurred.
 */
size_t AssimpIOStream::Read(void *pvBuffer, size_t pSize, size_t pCount)
{
    qint64 readBytes = m_device->read((char *)pvBuffer, pSize * pCount);
    if (readBytes < 0)
        qWarning() << Q_FUNC_INFO << " Reading failed";
    return readBytes;
}


/*!
 * Writes \a pCount elements of \a pSize bytes from \a pvBuffer.
 * Returns the number of bytes written or -1 if an error occurred.
 */
size_t AssimpIOStream::Write(const void *pvBuffer, size_t pSize, size_t pCount)
{
    qint64 writtenBytes = m_device->write((char *)pvBuffer, pSize * pCount);
    if (writtenBytes < 0)
        qWarning() << Q_FUNC_INFO << " Writing failed";
    return writtenBytes;
}

/*!
 * Seeks the current file descriptor to a position defined by \a pOrigin and \a pOffset
 */
aiReturn AssimpIOStream::Seek(size_t pOffset, aiOrigin pOrigin)
{
    qint64 seekPos = pOffset;

    if (pOrigin == aiOrigin_CUR)
        seekPos += m_device->pos();
    else if (pOrigin == aiOrigin_END)
        seekPos += m_device->size();

    if (!m_device->seek(seekPos)) {
        qWarning() << Q_FUNC_INFO << " Seeking failed";
        return aiReturn_FAILURE;
    }
    return aiReturn_SUCCESS;
}

/*!
 * Returns the current position of the read/write cursor.
 */
size_t AssimpIOStream::Tell() const
{
    return m_device->pos();
}

/*!
 * Returns the filesize;
 */
size_t AssimpIOStream::FileSize() const
{
    return m_device->size();
}

/*!
 * Flushes the current device.
 */
void AssimpIOStream::Flush()
{
    // QIODevice has no flush method
    if (QFileDevice *file = qobject_cast<QFileDevice *>(m_device))
        file->flush();
}

/*!
 * \class Qt3DRender::AssimpHelper::AssimpIOSystem
 *
 * \internal
 *
 * \brief Provides a custom file handler to the Assimp importer in order to handle
 * various Qt specificities (QResources ...)
 *
 */

/*!
 * Builds a new instance of AssimpIOSystem.
 */
AssimpIOSystem::AssimpIOSystem() :
    Assimp::IOSystem()
{
    m_openModeMaps[QByteArrayLiteral("r")] = QIODevice::ReadOnly;
    m_openModeMaps[QByteArrayLiteral("r+")] = QIODevice::ReadWrite;
    m_openModeMaps[QByteArrayLiteral("w")] = QIODevice::WriteOnly | QIODevice::Truncate;
    m_openModeMaps[QByteArrayLiteral("w+")] = QIODevice::ReadWrite | QIODevice::Truncate;
    m_openModeMaps[QByteArrayLiteral("a")] = QIODevice::WriteOnly | QIODevice::Append;
    m_openModeMaps[QByteArrayLiteral("a+")] = QIODevice::ReadWrite | QIODevice::Append;
    m_openModeMaps[QByteArrayLiteral("wb")] = QIODevice::WriteOnly;
    m_openModeMaps[QByteArrayLiteral("wt")] = QIODevice::WriteOnly | QIODevice::Text;
    m_openModeMaps[QByteArrayLiteral("rb")] = QIODevice::ReadOnly;
    m_openModeMaps[QByteArrayLiteral("rt")] = QIODevice::ReadOnly | QIODevice::Text;
}

/*!
 * Clears an AssimpIOSystem instance before deletion.
 */
AssimpIOSystem::~AssimpIOSystem()
{
}

/*!
 * Returns true if the file located at pFile exists, false otherwise.
 */
bool AssimpIOSystem::Exists(const char *pFile) const
{
    return QFileInfo(QString::fromUtf8(pFile)).exists();
}

/*!
 * Returns the default system separator.
 */
char AssimpIOSystem::getOsSeparator() const
{
    return QDir::separator().toLatin1();
}

/*!
 * Opens the file located at \a pFile with the opening mode
 * specified by \a pMode.
 */
Assimp::IOStream *AssimpIOSystem::Open(const char *pFile, const char *pMode)
{
    const QString fileName(QString::fromUtf8(pFile));
    const QByteArray cleanedMode(QByteArray(pMode).trimmed());

    const QIODevice::OpenMode openMode = m_openModeMaps.value(cleanedMode, QIODevice::NotOpen);

    QScopedPointer<QFile> file(new QFile(fileName));
    if (file->open(openMode))
        return new AssimpIOStream(file.take());

    return nullptr;
}

/*!
 * Closes the Assimp::IOStream \a pFile.
 */
void AssimpIOSystem::Close(Assimp::IOStream *pFile)
{
    // Assimp::IOStream has a virtual destructor which closes the stream
    delete pFile;
}

} // namespace AssimpHelper
} // namespace Qt3DRender

QT_END_NAMESPACE
