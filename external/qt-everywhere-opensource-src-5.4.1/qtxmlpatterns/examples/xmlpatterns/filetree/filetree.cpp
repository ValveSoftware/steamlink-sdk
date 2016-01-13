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

#include <QtCore/QUrl>
#include <QtCore/QVariant>
#include <QtXmlPatterns/QXmlNamePool>
#include "filetree.h"

/*
The model has two types of nodes: elements & attributes.

    <directory name="">
        <file name="">
        </file>
    </directory>

  In QXmlNodeModelIndex we store two values. QXmlNodeIndex::data()
  is treated as a signed int, and it is an index into m_fileInfos
  unless it is -1, in which case it has no meaning and the value
  of QXmlNodeModelIndex::additionalData() is a Type name instead.
 */

/*!
  The constructor passes \a pool to the base class, then loads an
  internal vector with an instance of QXmlName for each of the
  strings "file", "directory", "fileName", "filePath", "size",
  "mimeType", and "suffix".
 */
//! [2]
FileTree::FileTree(const QXmlNamePool& pool)
  : QSimpleXmlNodeModel(pool),
    m_filterAllowAll(QDir::AllEntries |
                     QDir::AllDirs |
                     QDir::NoDotAndDotDot |
                     QDir::Hidden),
    m_sortFlags(QDir::Name)
{
    QXmlNamePool np = namePool();
    m_names.resize(7);
    m_names[File]               = QXmlName(np, QLatin1String("file"));
    m_names[Directory]          = QXmlName(np, QLatin1String("directory"));
    m_names[AttributeFileName]  = QXmlName(np, QLatin1String("fileName"));
    m_names[AttributeFilePath]  = QXmlName(np, QLatin1String("filePath"));
    m_names[AttributeSize]      = QXmlName(np, QLatin1String("size"));
    m_names[AttributeMIMEType]  = QXmlName(np, QLatin1String("mimeType"));
    m_names[AttributeSuffix]    = QXmlName(np, QLatin1String("suffix"));
}
//! [2]

/*!
  Returns the QXmlNodeModelIndex for the model node representing
  the directory \a dirName.

  It calls QDir::cleanPath(), because an instance of QFileInfo
  constructed for a path ending in '/' will return the empty string in
  fileName(), instead of the directory name.
*/
QXmlNodeModelIndex FileTree::nodeFor(const QString& dirName) const
{
    QFileInfo dirInfo(QDir::cleanPath(dirName));
    Q_ASSERT(dirInfo.exists());
    return toNodeIndex(dirInfo);
}

/*!
  Since the value will always be in m_fileInfos, it is safe for
  us to return a const reference to it.
 */
//! [6]
const QFileInfo&
FileTree::toFileInfo(const QXmlNodeModelIndex &nodeIndex) const
{
    return m_fileInfos.at(nodeIndex.data());
}
//! [6]

/*!
  Returns the model node index for the node specified by the
  QFileInfo and node Type.
 */
//! [1]
QXmlNodeModelIndex
FileTree::toNodeIndex(const QFileInfo &fileInfo, Type attributeName) const
{
    const int indexOf = m_fileInfos.indexOf(fileInfo);

    if (indexOf == -1) {
        m_fileInfos.append(fileInfo);
        return createIndex(m_fileInfos.count()-1, attributeName);
    }
    else
        return createIndex(indexOf, attributeName);
}
//! [1]

/*!
  Returns the model node index for the node specified by the
  QFileInfo, which must be a  Type::File or Type::Directory.
 */
//! [0]
QXmlNodeModelIndex FileTree::toNodeIndex(const QFileInfo &fileInfo) const
{
    return toNodeIndex(fileInfo, fileInfo.isDir() ? Directory : File);
}
//! [0]

/*!
  This private helper function is only called by nextFromSimpleAxis().
  It is called whenever nextFromSimpleAxis() is called with an axis
  parameter of either \c{PreviousSibling} or \c{NextSibling}.
 */
//! [5]
QXmlNodeModelIndex FileTree::nextSibling(const QXmlNodeModelIndex &nodeIndex,
                                         const QFileInfo &fileInfo,
                                         qint8 offset) const
{
    Q_ASSERT(offset == -1 || offset == 1);

    // Get the context node's parent.
    const QXmlNodeModelIndex parent(nextFromSimpleAxis(Parent, nodeIndex));

    if (parent.isNull())
        return QXmlNodeModelIndex();

    // Get the parent's child list.
    const QFileInfo parentFI(toFileInfo(parent));
    Q_ASSERT(Type(parent.additionalData()) == Directory);
    const QFileInfoList siblings(QDir(parentFI.absoluteFilePath()).entryInfoList(QStringList(),
                                                                                 m_filterAllowAll,
                                                                                 m_sortFlags));
    Q_ASSERT_X(!siblings.isEmpty(), Q_FUNC_INFO, "Can't happen! We started at a child.");

    // Find the index of the child where we started.
    const int indexOfMe = siblings.indexOf(fileInfo);

    // Apply the offset.
    const int siblingIndex = indexOfMe + offset;
    if (siblingIndex < 0 || siblingIndex > siblings.count() - 1)
        return QXmlNodeModelIndex();
    else
        return toNodeIndex(siblings.at(siblingIndex));
}
//! [5]

/*!
  This function is called by the Qt XML Patterns query engine when it
  wants to move to the next node in the model. It moves along an \a
  axis, \e from the node specified by \a nodeIndex.

  This function is usually the one that requires the most design and
  implementation work, because the implementation depends on the
  perhaps unique structure of your non-XML data.

  There are \l {QAbstractXmlNodeModel::SimpleAxis} {four values} for
  \a axis that the implementation must handle, but there are really
  only two axes, i.e., vertical and horizontal. Two of the four values
  specify direction on the vertical axis (\c{Parent} and
  \c{FirstChild}), and the other two values specify direction on the
  horizontal axis (\c{PreviousSibling} and \c{NextSibling}).

  The typical implementation will be a \c switch statement with
  a case for each of the four \a axis values.
 */
//! [4]
QXmlNodeModelIndex
FileTree::nextFromSimpleAxis(SimpleAxis axis, const QXmlNodeModelIndex &nodeIndex) const
{
    const QFileInfo fi(toFileInfo(nodeIndex));
    const Type type = Type(nodeIndex.additionalData());

    if (type != File && type != Directory) {
        Q_ASSERT_X(axis == Parent, Q_FUNC_INFO, "An attribute only has a parent!");
        return toNodeIndex(fi, Directory);
    }

    switch (axis) {
        case Parent:
            return toNodeIndex(QFileInfo(fi.path()), Directory);

        case FirstChild:
        {
            if (type == File) // A file has no children.
                return QXmlNodeModelIndex();
            else {
                Q_ASSERT(type == Directory);
                Q_ASSERT_X(fi.isDir(), Q_FUNC_INFO, "It isn't really a directory!");
                const QDir dir(fi.absoluteFilePath());
                Q_ASSERT(dir.exists());

                const QFileInfoList children(dir.entryInfoList(QStringList(),
                                                               m_filterAllowAll,
                                                               m_sortFlags));
                if (children.isEmpty())
                    return QXmlNodeModelIndex();
                const QFileInfo firstChild(children.first());
                return toNodeIndex(firstChild);
            }
        }

        case PreviousSibling:
            return nextSibling(nodeIndex, fi, -1);

        case NextSibling:
            return nextSibling(nodeIndex, fi, 1);
    }

    Q_ASSERT_X(false, Q_FUNC_INFO, "Don't ever get here!");
    return QXmlNodeModelIndex();
}
//! [4]

/*!
  No matter what part of the file system we model (the whole file
  tree or a subtree), \a node will always have \c{file:///} as
  the document URI.
 */
QUrl FileTree::documentUri(const QXmlNodeModelIndex &node) const
{
    Q_UNUSED(node);
    return QUrl("file:///");
}

/*!
  This function returns QXmlNodeModelIndex::Element if \a node
  is a directory or a file, and QXmlNodeModelIndex::Attribute
  otherwise.
 */
QXmlNodeModelIndex::NodeKind
FileTree::kind(const QXmlNodeModelIndex &node) const
{
    switch (Type(node.additionalData())) {
        case Directory:
        case File:
            return QXmlNodeModelIndex::Element;
        default:
            return QXmlNodeModelIndex::Attribute;
    }
}

/*!
  No order is defined for this example, so we always return
  QXmlNodeModelIndex::Is, just to keep everyone happy.
 */
QXmlNodeModelIndex::DocumentOrder
FileTree::compareOrder(const QXmlNodeModelIndex&,
                       const QXmlNodeModelIndex&) const
{
    return QXmlNodeModelIndex::Is;
}

/*!
  Returns the name of \a node. The caller guarantees that \a node is
  not null and that it is contained in this node model.
 */
//! [3]
QXmlName FileTree::name(const QXmlNodeModelIndex &node) const
{
    return m_names.at(node.additionalData());
}
//! [3]

/*!
  Always returns the QXmlNodeModelIndex for the root of the
  file system, i.e. "/".
 */
QXmlNodeModelIndex FileTree::root(const QXmlNodeModelIndex &node) const
{
    Q_UNUSED(node);
    return toNodeIndex(QFileInfo(QLatin1String("/")));
}

/*!
  Returns the typed value for \a node, which must be either an
  attribute or an element. The QVariant returned represents the atomic
  value of an attribute or the atomic value contained in an element.

  If the QVariant is returned as a default constructed variant,
  it means that \a node has no typed value.
 */
QVariant FileTree::typedValue(const QXmlNodeModelIndex &node) const
{
    const QFileInfo &fi = toFileInfo(node);

    switch (Type(node.additionalData())) {
        case Directory:
            // deliberate fall through.
        case File:
            return QString();
        case AttributeFileName:
            return fi.fileName();
        case AttributeFilePath:
            return fi.filePath();
        case AttributeSize:
            return fi.size();
        case AttributeMIMEType:
            {
                /* We don't have any MIME detection code currently, so return
                 * the most generic one. */
                return QLatin1String("application/octet-stream");
            }
        case AttributeSuffix:
            return fi.suffix();
    }

    Q_ASSERT_X(false, Q_FUNC_INFO, "This line should never be reached.");
    return QString();
}

/*!
  Returns the attributes of \a element. The caller guarantees
  that \a element is an element in this node model.
 */
QVector<QXmlNodeModelIndex>
FileTree::attributes(const QXmlNodeModelIndex &element) const
{
    QVector<QXmlNodeModelIndex> result;

    /* Both elements has this attribute. */
    const QFileInfo &forElement = toFileInfo(element);
    result.append(toNodeIndex(forElement, AttributeFilePath));
    result.append(toNodeIndex(forElement, AttributeFileName));

    if (Type(element.additionalData() == File)) {
        result.append(toNodeIndex(forElement, AttributeSize));
        result.append(toNodeIndex(forElement, AttributeSuffix));
        //result.append(toNodeIndex(forElement, AttributeMIMEType));
    }
    else {
        Q_ASSERT(element.additionalData() == Directory);
    }

    return result;
}

