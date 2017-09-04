#! /usr/bin/perl -w
#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the translations module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################


use strict;

my @catalogs = ( "qtbase", "qtscript", "qtquick1", "qtmultimedia", "qtxmlpatterns" );

die "Usage: $0 <locale> [<builddir>]\n" if (@ARGV != 1 && @ARGV != 2);
my $lang = $ARGV[0];
my $lupdate = "lupdate -locations relative -no-ui-lines";
$lupdate .=  " -pro-out $ARGV[1]" if (@ARGV == 2);

for my $cat (@catalogs) {
    my $extra = "";
    $extra = " ../../qtactiveqt/src/src.pro ../../qtimageformats/src/src.pro" if ($cat eq "qtbase");
    system("$lupdate ../../$cat/src/src.pro$extra -xts qt_$lang.ts -ts ${cat}_$lang.ts") and die;
}
# qtdeclarative & qmlviewer are special: we import them, but they are not part of the meta catalog
system("$lupdate ../../qtdeclarative/src/src.pro -xts qt_$lang.ts -ts qtdeclarative_$lang.ts") and die;
system("$lupdate ../../qtquick1/tools/qml/qml.pro -xts qt_$lang.ts -ts qmlviewer_$lang.ts") and die;

open META, "> qt_$lang.ts" or die;
print META <<EOF ;
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.0" language="$lang">
    <dependencies>
EOF
for my $cat (@catalogs) {
    print META "        <dependency catalog=\"${cat}_$lang\"/>\n";
}
print META <<EOF ;
    </dependencies>
</TS>
EOF
close META or die;
