/******************************************************************************
** This is just slightly modified version of convert_dict.cc
** chromium/chrome/tools/convert_dict/convert_dict.cc
**
** Original work:
** Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
** Modified work:
** Copyright (C) 2016 The Qt Company Ltd.
**
** Use of this source code is governed by a BSD-style license that can be
** found in the LICENSE file.
**
** This tool converts Hunspell .aff/.dic pairs to a combined binary dictionary
** format (.bdic). This format is more compact, and can be more efficiently
** read by the client application.
**
******************************************************************************/

#include <base/at_exit.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/i18n/icu_util.h>
#include <build/build_config.h>
#include <chrome/tools/convert_dict/aff_reader.h>
#include <chrome/tools/convert_dict/dic_reader.h>
#include <third_party/hunspell/google/bdict_reader.h>
#include <third_party/hunspell/google/bdict_writer.h>
#include <base/path_service.h>

#include <QTextStream>
#include <QLibraryInfo>
#include <QDir>

// see also src/core/type_conversion.h
inline base::FilePath::StringType toFilePathString(const QString &str)
{
#if defined(Q_OS_WIN)
    return QDir::toNativeSeparators(str).toStdWString();
#else
    return str.toStdString();
#endif
}

inline base::FilePath toFilePath(const QString &str)
{
    return base::FilePath(toFilePathString(str));
}

inline QString toQt(const base::string16 &string)
{
#if defined(OS_WIN)
    return QString::fromStdWString(string.data());
#else
    return QString::fromUtf16(string.data());
#endif
}

inline QString toQt(const std::string &string)
{
    return QString::fromStdString(string);
}

// Compares the given word list with the serialized trie to make sure they
// are the same.
inline bool VerifyWords(const convert_dict::DicReader::WordList& org_words,
                        const std::string& serialized, QTextStream& out)
{
    hunspell::BDictReader reader;
    if (!reader.Init(reinterpret_cast<const unsigned char*>(serialized.data()),
                     serialized.size())) {
        out << "BDict is invalid" << endl;
        return false;
    }
    hunspell::WordIterator iter = reader.GetAllWordIterator();

    int affix_ids[hunspell::BDict::MAX_AFFIXES_PER_WORD];

    static const int buf_size = 128;
    char buf[buf_size];
    for (size_t i = 0; i < org_words.size(); i++) {
        int affix_matches = iter.Advance(buf, buf_size, affix_ids);
        if (affix_matches == 0) {
            out << "Found the end before we expected" << endl;
            return false;
        }

        if (org_words[i].first != buf) {
            out << "Word doesn't match, word #" << buf << endl;
            return false;
        }

        if (affix_matches != static_cast<int>(org_words[i].second.size())) {
            out << "Different number of affix indices, word #" << buf << endl;
            return false;
        }

        // Check the individual affix indices.
        for (size_t affix_index = 0; affix_index < org_words[i].second.size();
             affix_index++) {
            if (affix_ids[affix_index] != org_words[i].second[affix_index]) {
                out <<  "Index doesn't match, word #" <<  buf << endl;
                return false;
            }
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    QTextStream out(stdout);

    if (argc != 3) {
        QTextStream out(stdout);
        out << "Usage: qwebengine_convert_dict <dic file> <bdic file>\n\nExample:\n"
               "qwebengine_convert_dict ./en-US.dic ./en-US.bdic\nwill read en-US.dic, "
               "en-US.dic_delta, and en-US.aff from the current directory and generate "
               "en-US.bdic\n" << endl;
        return 1;
    }

    PathService::Override(base::DIR_QT_LIBRARY_DATA,
                          toFilePath(QLibraryInfo::location(QLibraryInfo::DataPath) %
                                     QLatin1String("/resources")));

    base::AtExitManager exit_manager;
    base::i18n::InitializeICU();

    base::FilePath file_in_path = toFilePath(argv[1]);
    base::FilePath file_out_path = toFilePath(argv[2]);
    base::FilePath aff_path = file_in_path.ReplaceExtension(FILE_PATH_LITERAL(".aff"));

    out << "Reading " << toQt(aff_path.value()) << endl;
    convert_dict::AffReader aff_reader(aff_path);

    if (!aff_reader.Read()) {
        out << "Unable to read the aff file." << endl;
        return 1;
    }

    base::FilePath dic_path = file_in_path.ReplaceExtension(FILE_PATH_LITERAL(".dic"));
    out << "Reading " << toQt(dic_path.value()) << endl;

    // DicReader will also read the .dic_delta file.
    convert_dict::DicReader dic_reader(dic_path);
    if (!dic_reader.Read(&aff_reader)) {
        out << "Unable to read the dic file." << endl;
        return 1;
    }

    hunspell::BDictWriter writer;
    writer.SetComment(aff_reader.comments());
    writer.SetAffixRules(aff_reader.affix_rules());
    writer.SetAffixGroups(aff_reader.GetAffixGroups());
    writer.SetReplacements(aff_reader.replacements());
    writer.SetOtherCommands(aff_reader.other_commands());
    writer.SetWords(dic_reader.words());

    out << "Serializing..." << endl;

    std::string serialized = writer.GetBDict();

    out << "Verifying..." << endl;

    if (!VerifyWords(dic_reader.words(), serialized, out)) {
        out << "ERROR converting, the dictionary does not check out OK." << endl;
        return 1;
    }

    out << "Writing " << toQt(file_out_path.value()) << endl;
    FILE *out_file = base::OpenFile(file_out_path, "wb");
    if (!out_file) {
        out << "ERROR writing file" << endl;
        return 1;
    }
    size_t written = fwrite(&serialized[0], 1, serialized.size(), out_file);
    Q_ASSERT(written == serialized.size());
    base::CloseFile(out_file);
    out << "Success. Dictionary converted." << endl;
    return 0;
}

