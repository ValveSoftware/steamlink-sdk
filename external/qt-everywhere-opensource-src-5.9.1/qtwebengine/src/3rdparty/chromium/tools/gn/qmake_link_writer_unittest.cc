/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/qmake_link_writer.h"
#include "ninja_binary_target_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(QMakeLinkWriter, WriteLinkPri) {
   TestWithScope setup;
   Err err;

   setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));


   Target source_set_target(setup.settings(), Label(SourceDir("//foo1/"), "foo1"));
   source_set_target.set_output_type(Target::SOURCE_SET);
   source_set_target.visibility().SetPublic();
   source_set_target.sources().push_back(SourceFile("//foo1/input1.cc"));
   source_set_target.sources().push_back(SourceFile("//foo1/input2.cc"));
   source_set_target.SetToolchain(setup.toolchain());
   ASSERT_TRUE(source_set_target.OnResolved(&err));

   TestTarget static_lib_target(setup, "//foo5:bar", Target::STATIC_LIBRARY);
   static_lib_target.sources().push_back(SourceFile("//foo5/input1.cc"));
   static_lib_target.config_values().arflags().push_back("--bar");
   static_lib_target.set_complete_static_lib(true);
   ASSERT_TRUE(static_lib_target.OnResolved(&err));

   TestTarget deps_shared_lib_target(setup, "//foo6:shlib", Target::SHARED_LIBRARY);
   //this trigers solibs
   deps_shared_lib_target.set_create_pri_file(true);
   deps_shared_lib_target.sources().push_back(SourceFile("//foo6/input1.cc"));

   ASSERT_TRUE(deps_shared_lib_target.OnResolved(&err));

   Target shared_lib_target(setup.settings(), Label(SourceDir("//foo2/"), "foo2"));
   shared_lib_target.set_create_pri_file(true);
   shared_lib_target.set_output_type(Target::SHARED_LIBRARY);
   shared_lib_target.set_output_extension(std::string("so.1"));
   shared_lib_target.set_output_dir(SourceDir("//out/Debug/foo/"));
   shared_lib_target.sources().push_back(SourceFile("//foo2/input1.cc"));
   shared_lib_target.sources().push_back(SourceFile("//foo2/input 2.cc"));
   shared_lib_target.sources().push_back(SourceFile("//foo 2/input 3.cc"));
   shared_lib_target.config_values().libs().push_back(LibFile(SourceFile("//foo/libfoo3.a")));
   shared_lib_target.config_values().libs().push_back(LibFile("foo4"));
   shared_lib_target.config_values().lib_dirs().push_back(SourceDir("//foo/bar/"));
   shared_lib_target.public_deps().push_back(LabelTargetPair(&source_set_target));
   shared_lib_target.public_deps().push_back(LabelTargetPair(&static_lib_target));
   shared_lib_target.public_deps().push_back(LabelTargetPair(&deps_shared_lib_target));
   shared_lib_target.config_values().ldflags().push_back("-fooBAR");
   shared_lib_target.config_values().ldflags().push_back("/INCREMENTAL:NO");
   shared_lib_target.SetToolchain(setup.toolchain());
   ASSERT_TRUE(shared_lib_target.OnResolved(&err));

   std::ostringstream out1;
   NinjaBinaryTargetWriter writer(&shared_lib_target, out1);
   writer.Run();

   const char expected1[] =
       "defines =\n"
       "include_dirs =\n"
       "cflags =\n"
       "cflags_cc =\n"
       "root_out_dir = .\n"
       "target_out_dir = obj/foo2\n"
       "target_output_name = libfoo2\n"
       "\n"
       "build obj/foo2/libfoo2.input1.o: cxx ../../foo2/input1.cc\n"
       "build obj/foo2/libfoo2.input$ 2.o: cxx ../../foo2/input$ 2.cc\n"
       "build obj/foo$ 2/libfoo2.input$ 3.o: cxx ../../foo$ 2/input$ 3.cc\n"
       "\n"
       "build foo2.stamp: stamp | obj/foo2/libfoo2.input1.o obj/foo2/libfoo2.input$ 2.o"
           " obj/foo$ 2/libfoo2.input$ 3.o"
           " obj/foo1/foo1.input1.o obj/foo1/foo1.input2.o obj/foo5/libbar.a "
           "shlib.stamp ../../foo/libfoo3.a || obj/foo1/foo1.stamp\n"
       "  ldflags = -fooBAR /INCREMENTAL$:NO -L../../foo/bar\n"
       "  libs = ../../foo/libfoo3.a -lfoo4\n"
       "  output_extension = .so.1\n"
       "  output_dir = foo\n"
       "  solibs = ./libshlib.so\n";

   EXPECT_EQ(expected1, out1.str());

   std::ostringstream out2;
   QMakeLinkWriter pri_writer(&writer, &shared_lib_target, out2);
   pri_writer.Run();

   const char expected2[] =
       "NINJA_OBJECTS = \\\n"
       "    \"$$PWD/obj/foo2/libfoo2.input1.o\" \\\n"
       "    \"$$PWD/obj/foo2/libfoo2.input 2.o\" \\\n"
       "    \"$$PWD/obj/foo 2/libfoo2.input 3.o\" \\\n"
       "    \"$$PWD/obj/foo1/foo1.input1.o\" \\\n"
       "    \"$$PWD/obj/foo1/foo1.input2.o\"\n"
       "NINJA_LFLAGS = -fooBAR /INCREMENTAL:NO\n"
       "NINJA_ARCHIVES = \\\n"
       "    \"$$PWD/obj/foo5/libbar.a\"\n"
       "NINJA_LIB_DIRS = -L../../foo/bar\n"
       "NINJA_LIBS = ../../foo/libfoo3.a -lfoo4\n"
       "NINJA_SOLIBS = \"$$PWD/./libshlib.so\"\n"
       "NINJA_TARGETDEPS = \"$$PWD/foo2.stamp\"\n";
   std::string out_str2 = out2.str();
   EXPECT_EQ(expected2, out_str2);
}
