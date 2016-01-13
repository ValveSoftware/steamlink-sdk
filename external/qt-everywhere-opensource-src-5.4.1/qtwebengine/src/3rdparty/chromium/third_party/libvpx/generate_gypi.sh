#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gypi files and files in the config/platform
# directories needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# For example:
# $ ./generate_gypi.sh
#
# And this will update all the .gypi and config files needed.
#
# !!! It's highly recommended to install yasm before running this script.

export LC_ALL=C
BASE_DIR=`pwd`
LIBVPX_SRC_DIR="source/libvpx"
LIBVPX_CONFIG_DIR="source/config"

# Print gypi boilerplate header
# $1 - Output base name
function write_gypi_header {
  echo "# This file is generated. Do not edit." > $1
  echo "# Copyright (c) 2013 The Chromium Authors. All rights reserved." >> $1
  echo "# Use of this source code is governed by a BSD-style license that can be" >> $1
  echo "# found in the LICENSE file." >> $1
  echo "" >> $1
  echo "{" >> $1
}

# Print gypi boilerplate footer
# $1 - Output base name
function write_gypi_footer {
  echo "}" >> $1
}

# Generate a gypi with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - Output file
function write_file_list {
  # Convert the first argument back in to an array.
  declare -a file_list=("${!1}")

  write_gypi_header $2

  echo "  'sources': [" >> $2
  for f in $file_list
  do
    echo "    '<(libvpx_source)/$f'," >> $2
  done
  echo "  ]," >> $2

  write_gypi_footer $2
}

# Target template function
# $1 - Array name for file list.
# $2 - Output file
# $3 - Target name
# $4 - Compiler flag
function write_target_definition {
  declare -a sources_list=("${!1}")

  echo "    {" >> $2
  echo "      'target_name': '$3'," >> $2
  echo "      'type': 'static_library'," >> $2
  echo "      'include_dirs': [" >> $2
  echo "        'source/config/<(OS_CATEGORY)/<(target_arch_full)'," >> $2
  echo "        '<(libvpx_source)'," >> $2
  echo "      ]," >> $2
  echo "      'sources': [" >> $2
  for f in $sources_list
  do
    echo "        '<(libvpx_source)/$f'," >> $2
  done
  echo "      ]," >> $2
  echo "      'conditions': [" >> $2
  echo "        ['os_posix==1 and OS!=\"mac\" and OS!=\"ios\"', {" >> $2
  echo "          'cflags!': [ '-mfpu=vfpv3-d16' ]," >> $2
  echo "          'cflags': [ '-m$4', ]," >> $2
  echo "        }]," >> $2
  echo "        ['OS==\"mac\" or OS==\"ios\"', {" >> $2
  echo "          'xcode_settings': {" >> $2
  echo "            'OTHER_CFLAGS': [ '-m$4', ]," >> $2
  echo "          }," >> $2
  echo "        }]," >> $2
  if [[ $4 == avx* ]]; then
  echo "        ['OS==\"win\"', {" >> $2
  echo "          'msvs_settings': {" >> $2
  echo "            'VCCLCompilerTool': {" >> $2
  echo "              'EnableEnhancedInstructionSet': '3', # /arch:AVX" >> $2
  echo "            }," >> $2
  echo "          }," >> $2
  echo "        }]," >> $2
  fi
  echo "      ]," >> $2
  echo "    }," >> $2
}


# Generate a gypi which applies additional compiler flags based on the file
# name.
# $1 - Array name for file list.
# $2 - Output file
function write_special_flags {
  declare -a file_list=("${!1}")

  local mmx_sources=$(echo "$file_list" | grep '_mmx\.c$')
  local sse2_sources=$(echo "$file_list" | grep '_sse2\.c$')
  local sse3_sources=$(echo "$file_list" | grep '_sse3\.c$')
  local ssse3_sources=$(echo "$file_list" | grep '_ssse3\.c$')
  local sse4_1_sources=$(echo "$file_list" | grep '_sse4\.c$')
  local avx_sources=$(echo "$file_list" | grep '_avx\.c$')
  local avx2_sources=$(echo "$file_list" | grep '_avx2\.c$')

  local neon_sources=$(echo "$file_list" | grep '_neon\.c$')

  # Intrinsic functions and files are in flux. We can selectively generate them
  # but we can not selectively include them in libvpx.gyp. Throw some errors
  # when new targets are needed.

  write_gypi_header $2

  echo "  'targets': [" >> $2

  # x86[_64]
  if [ 0 -ne ${#mmx_sources} ]; then
    write_target_definition mmx_sources[@] $2 libvpx_intrinsics_mmx mmx
  fi
  if [ 0 -ne ${#sse2_sources} ]; then
    write_target_definition sse2_sources[@] $2 libvpx_intrinsics_sse2 sse2
  fi
  if [ 0 -ne ${#sse3_sources} ]; then
    #write_target_definition sse3_sources[@] $2 libvpx_intrinsics_sse3 sse3
    echo "ERROR: Uncomment sse3 sections in libvpx.gyp"
    exit 1
  fi
  if [ 0 -ne ${#ssse3_sources} ]; then
    write_target_definition ssse3_sources[@] $2 libvpx_intrinsics_ssse3 ssse3
  fi
  if [ 0 -ne ${#sse4_1_sources} ]; then
    #write_target_definition sse4_1_sources[@] $2 libvpx_intrinsics_sse4_1 sse4.1
    echo "ERROR: Uncomment sse4_1 sections in libvpx.gyp"
    exit 1
  fi
  if [ 0 -ne ${#avx_sources} ]; then
    #write_target_definition avx_sources[@] $2 libvpx_intrinsics_avx avx
    echo "ERROR: Uncomment avx sections in libvpx.gyp"
    exit 1
  fi
  if [ 0 -ne ${#avx2_sources} ]; then
    #write_target_definition avx2_sources[@] $2 libvpx_intrinsics_avx2 avx2
    echo "ERROR: Uncomment avx2 sections in libvpx.gyp"
    exit 1
  fi

  # arm neon
  if [ 0 -ne ${#neon_sources} ]; then
    write_target_definition neon_sources[@] $2 libvpx_intrinsics_neon fpu=neon
  fi

  echo "  ]," >> $2

  write_gypi_footer $2
}

# Convert a list of source files into gypi file.
# $1 - Input file.
# $2 - Output gypi file base. Will generate additional .gypi files when
#      different compilation flags are required.
function convert_srcs_to_gypi {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 2. Move certain files to a separate include to allow applying different
  #    compiler options.
  # 3. Replace .asm.s to .asm because gyp will do the conversion.

  local source_list=$(grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1)

  # _offsets are used in pre-processing to generate files for assembly. They are
  # not part of the compiled library.
  source_list=$(echo "$source_list" | grep -v '_offsets\.c')

  # Not sure why vpx_config is not included.
  source_list=$(echo "$source_list" | grep -v 'vpx_config\.c')

  # The actual ARM files end in .asm. We have rules to translate them to .S
  source_list=$(echo "$source_list" | sed s/\.asm\.s$/.asm/)

  # Select all x86 files ending with .c
  local intrinsic_list=$(echo "$source_list" | \
    egrep 'vp[89]/(encoder|decoder|common)/x86/'  | \
    egrep '(mmx|sse2|sse3|ssse3|sse4|avx|avx2).c$')

  # Select all neon files ending in C but only when building in RTCD mode
  if [ "libvpx_srcs_arm_neon_cpu_detect" == "$2" ]; then
    # Select all arm neon files ending in _neon.c
    # the pattern may need to be updated if vpx_scale gets intrinics
    local intrinsic_list=$(echo "$source_list" | \
      egrep 'vp[89]/(encoder|decoder|common)/arm/neon/'  | \
      egrep '_neon.c$')
  fi

  # Remove these files from the main list.
  source_list=$(comm -23 <(echo "$source_list") <(echo "$intrinsic_list"))

  write_file_list source_list $BASE_DIR/$2.gypi

  # All the files are in a single "element." Check if the first element has
  # length 0.
  if [ 0 -ne ${#intrinsic_list} ]; then
    write_special_flags intrinsic_list[@] $BASE_DIR/$2_intrinsics.gypi
  fi

}

# Clean files from previous make.
function make_clean {
  make clean > /dev/null
  rm -f libvpx_srcs.txt
}

# Lint a pair of vpx_config.h and vpx_config.asm to make sure they match.
# $1 - Header file directory.
function lint_config {
  # mips does not contain any assembly so the header does not need to be
  # compared to the asm.
  if [[ "$1" != *mipsel ]]; then
    $BASE_DIR/lint_config.sh \
      -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
      -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
  fi
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
    -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm
}

# Print the configuration from Header file.
# This function is an abridged version of print_config which does not use
# lint_config and it does not require existence of vpx_config.asm.
# $1 - Header file directory.
function print_config_basic {
  combined_config="$(cat $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
                   | grep -E ' +[01] *$')"
  combined_config="$(echo "$combined_config" | grep -v DO1STROUNDING)"
  combined_config="$(echo "$combined_config" | sed 's/[ \t]//g')"
  combined_config="$(echo "$combined_config" | sed 's/.*define//')"
  combined_config="$(echo "$combined_config" | sed 's/0$/=no/')"
  combined_config="$(echo "$combined_config" | sed 's/1$/=yes/')"
  echo "$combined_config" | sort | uniq
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
function gen_rtcd_header {
  echo "Generate $LIBVPX_CONFIG_DIR/$1/*_rtcd.h files."

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
  if [ "$2" = "mipsel" ]; then
    print_config_basic $1 > $BASE_DIR/$TEMP_DIR/libvpx.config
  else
    $BASE_DIR/lint_config.sh -p \
      -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
      -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm \
      -o $BASE_DIR/$TEMP_DIR/libvpx.config
  fi

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp8_rtcd \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    --disable-avx2 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8/common/rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp8_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp9_rtcd \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    --disable-avx2 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9/common/vp9_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp9_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_scale_rtcd \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    --disable-avx2 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_scale/vpx_scale_rtcd.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_scale_rtcd.h

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
}

# Generate Config files. "--enable-external-build" must be set to skip
# detection of capabilities on specific targets.
# $1 - Header file directory.
# $2 - Config command line.
function gen_config_files {
  ./configure $2  > /dev/null

  # Generate vpx_config.asm. Do not create one for mips.
  if [[ "$1" != *mipsel ]]; then
    if [[ "$1" == *x64* ]] || [[ "$1" == *ia32* ]]; then
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print $2 " equ " $3}' > vpx_config.asm
    else
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print $2 " EQU " $3}' | perl $BASE_DIR/$LIBVPX_SRC_DIR/build/make/ads2gas.pl > vpx_config.asm
    fi
  fi

  cp vpx_config.* $BASE_DIR/$LIBVPX_CONFIG_DIR/$1
  make_clean
  rm -rf vpx_config.*
}

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Generate Config Files"
# TODO(joeyparrish) Enable AVX2 when broader VS2013 support is available
all_platforms="--enable-external-build --enable-postproc --disable-install-srcs --enable-multi-res-encoding --enable-temporal-denoising --disable-unit-tests --disable-install-docs --disable-examples --disable-avx2"
gen_config_files linux/ia32 "--target=x86-linux-gcc --disable-ccache --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files linux/x64 "--target=x86_64-linux-gcc --disable-ccache --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files linux/arm "--target=armv6-linux-gcc --enable-pic --enable-realtime-only --disable-install-bins --disable-install-libs --disable-edsp ${all_platforms}"
gen_config_files linux/arm-neon "--target=armv7-linux-gcc --enable-pic --enable-realtime-only --disable-edsp ${all_platforms}"
gen_config_files linux/arm-neon-cpu-detect "--target=armv7-linux-gcc --enable-pic --enable-realtime-only --enable-runtime-cpu-detect --disable-edsp ${all_platforms}"
gen_config_files linux/arm64 "--force-target=armv8-linux-gcc --enable-pic --enable-realtime-only --disable-edsp ${all_platforms}"
gen_config_files linux/mipsel "--target=mips32-linux-gcc --disable-fast-unaligned ${all_platforms}"
gen_config_files linux/generic "--target=generic-gnu --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files win/ia32 "--target=x86-win32-vs12 --enable-realtime-only ${all_platforms}"
gen_config_files win/x64 "--target=x86_64-win64-vs12 --enable-realtime-only ${all_platforms}"
gen_config_files mac/ia32 "--target=x86-darwin9-gcc --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files mac/x64 "--target=x86_64-darwin9-gcc --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files nacl "--target=generic-gnu --enable-pic --enable-realtime-only ${all_platforms}"

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

echo "Lint libvpx configuration."
lint_config linux/ia32
lint_config linux/x64
lint_config linux/arm
lint_config linux/arm-neon
lint_config linux/arm-neon-cpu-detect
lint_config linux/arm64
lint_config linux/mipsel
lint_config linux/generic
lint_config win/ia32
lint_config win/x64
lint_config mac/ia32
lint_config mac/x64
lint_config nacl

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

gen_rtcd_header linux/ia32 x86
gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/arm armv6
gen_rtcd_header linux/arm-neon armv7
gen_rtcd_header linux/arm-neon-cpu-detect armv7
gen_rtcd_header linux/arm64 armv8
gen_rtcd_header linux/mipsel mipsel
gen_rtcd_header linux/generic generic
gen_rtcd_header win/ia32 x86
gen_rtcd_header win/x64 x86_64
gen_rtcd_header mac/ia32 x86
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header nacl nacl

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

echo "Generate X86 source list."
config=$(print_config linux/ia32)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_x86

# Copy vpx_version.h. The file should be the same for all platforms.
cp vpx_version.h $BASE_DIR/$LIBVPX_CONFIG_DIR

echo "Generate X86_64 source list."
config=$(print_config linux/x64)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_x86_64

echo "Generate ARM source list."
config=$(print_config linux/arm)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_arm

echo "Generate ARM NEON source list."
config=$(print_config linux/arm-neon)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_arm_neon

echo "Generate ARM NEON CPU DETECT source list."
config=$(print_config linux/arm-neon-cpu-detect)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_arm_neon_cpu_detect

echo "Generate ARM64 source list."
config=$(print_config linux/arm64)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_arm64

echo "Generate MIPS source list."
config=$(print_config_basic linux/mipsel)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_mips

echo "Generate NaCl source list."
config=$(print_config_basic nacl)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_nacl

echo "Generate GENERIC source list."
config=$(print_config_basic linux/generic)
make_clean
make libvpx_srcs.txt target=libs $config > /dev/null
convert_srcs_to_gypi libvpx_srcs.txt libvpx_srcs_generic

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

# TODO(fgalligan): Is "--disable-fast-unaligned" needed on mipsel?
# TODO(fgalligan): Can we turn on "--enable-realtime-only" for mipsel?
