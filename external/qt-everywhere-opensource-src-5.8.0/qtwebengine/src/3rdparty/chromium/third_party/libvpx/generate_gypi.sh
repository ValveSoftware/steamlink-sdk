#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gypi, .gni files and files in the
# config/platform directories needed to build libvpx.
# Every time libvpx source code is updated just run this script.
#
# Usage:
# $ ./generate_gypi.sh [--disable-avx] [--only-configs]
#
# The following optional flags are supported:
# --disable-avx : AVX+AVX2 support is disabled.
# --only-configs: Excludes generation of GN and GYP files (i.e. only
#                 configuration headers are generated).

export LC_ALL=C
BASE_DIR=$(pwd)
LIBVPX_SRC_DIR="source/libvpx"
LIBVPX_CONFIG_DIR="source/config"
unset DISABLE_AVX

for i in "$@"
do
case $i in
  --disable-avx)
  DISABLE_AVX="--disable-avx --disable-avx2"
  shift
  ;;
  --only-configs)
  ONLY_CONFIGS=true
  shift
  ;;
  *)
  echo "Unknown option: $i"
  exit 1
  ;;
esac
done

# Print license header.
# $1 - Output base name
function write_license {
  echo "# This file is generated. Do not edit." >> $1
  echo "# Copyright (c) 2014 The Chromium Authors. All rights reserved." >> $1
  echo "# Use of this source code is governed by a BSD-style license that can be" >> $1
  echo "# found in the LICENSE file." >> $1
  echo "" >> $1
}

# Search for source files with the same basename in vp8, vp9, and vpx_dsp. The
# build can support such files but only when they are built into disparate
# modules. Configuring such modules for both gyp and gn are tricky so avoid the
# issue at least until vp10 is added.
function find_duplicates {
  local readonly duplicate_file_names=$(find \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
    -type f -name \*.c  | xargs -I {} basename {} | sort | uniq -d \
  )

  if [ -n "${duplicate_file_names}" ]; then
    echo "WARNING: DUPLICATE FILES FOUND"
    for file in  ${duplicate_file_names}; do
      find \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp8 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vp9 \
        $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp \
        -name $file
    done
    exit 1
  fi
}

# Print gypi boilerplate header.
# $1 - Output base name
function write_gypi_header {
  echo "{" >> $1
}

# Print gypi boilerplate footer.
# $1 - Output base name
function write_gypi_footer {
  echo "}" >> $1
}

# Generate a gypi with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - Output file
function write_gypi {
  # Convert the first argument back in to an array.
  local readonly file_list=(${!1})

  rm -rf "$2"
  write_license "$2"
  write_gypi_header "$2"

  echo "  'sources': [" >> "$2"
  for f in ${file_list[@]}
  do
    echo "    '<(libvpx_source)/$f'," >> "$2"
  done
  echo "  ]," >> "$2"

  write_gypi_footer "$2"
}

# Generate a gni with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - GN variable name.
# $3 - Output file.
function write_gni {
  # Convert the first argument back in to an array.
  declare -a file_list=("${!1}")

  echo "$2 = [" >> "$3"
  for f in $file_list
  do
    echo "  \"//third_party/libvpx/source/libvpx/$f\"," >> "$3"
  done
  echo "]" >> "$3"
}

# Target template function
# $1 - Array name for file list.
# $2 - Output file
# $3 - Target name
# $4 - Compiler flag
function write_target_definition {
  declare -a sources_list=("${!1}")

  echo "    {" >> "$2"
  echo "      'target_name': '$3'," >> "$2"
  echo "      'type': 'static_library'," >> "$2"
  echo "      'include_dirs': [" >> "$2"
  echo "        'source/config/<(OS_CATEGORY)/<(target_arch_full)'," >> "$2"
  echo "        '<(libvpx_source)'," >> "$2"
  echo "      ]," >> "$2"
  echo "      'sources': [" >> "$2"
  for f in $sources_list
  do
    echo "        '<(libvpx_source)/$f'," >> $2
  done
  echo "      ]," >> "$2"
  if [[ $4 == fpu=neon ]]; then
  echo "      'includes': [ 'ads2gas.gypi' ]," >> "$2"
  echo "      'cflags!': [ '-mfpu=vfpv3-d16' ]," >> "$2"
  echo "      'conditions': [" >> $2
  echo "        # Disable GCC LTO in neon targets due to compiler bug" >> "$2"
  echo "        # crbug.com/408997" >> "$2"
  echo "        ['clang==0 and use_lto==1', {" >> "$2"
  echo "          'cflags!': [" >> "$2"
  echo "            '-flto'," >> "$2"
  echo "            '-ffat-lto-objects'," >> "$2"
  echo "          ]," >> "$2"
  echo "        }]," >> "$2"
  echo "      ]," >> "$2"
  echo "      'cflags': [ '-m$4', ]," >> "$2"
  echo "      'asmflags': [ '-m$4', ]," >> "$2"
  else
  echo "      'cflags': [ '-m$4', ]," >> "$2"
  echo "      'xcode_settings': { 'OTHER_CFLAGS': [ '-m$4' ] }," >> "$2"
  fi
  if [[ -z $DISABLE_AVX && $4 == avx ]]; then
  echo "      'msvs_settings': {" >> "$2"
  echo "        'VCCLCompilerTool': {" >> "$2"
  echo "          'EnableEnhancedInstructionSet': '3', # /arch:AVX" >> "$2"
  echo "        }," >> "$2"
  echo "      }," >> "$2"
  fi
  if [[ -z $DISABLE_AVX && $4 == avx2 ]]; then
  echo "      'msvs_settings': {" >> "$2"
  echo "        'VCCLCompilerTool': {" >> "$2"
  echo "          'EnableEnhancedInstructionSet': '5', # /arch:AVX2" >> "$2"
  echo "        }," >> "$2"
  echo "      }," >> "$2"
  fi
  if [[ $4 == ssse3 || $4 == sse4.1 ]]; then
  echo "      'conditions': [" >> "$2"
  echo "        ['OS==\"win\" and clang==1', {" >> "$2"
  echo "          # cl.exe's /arch flag doesn't have a setting for SSSE3/4, and cl.exe" >> "$2"
  echo "          # doesn't need it for intrinsics. clang-cl does need it, though." >> "$2"
  echo "          'msvs_settings': {" >> "$2"
  echo "            'VCCLCompilerTool': { 'AdditionalOptions': [ '-m$4' ] }," >> "$2"
  echo "          }," >> "$2"
  echo "        }]," >> "$2"
  echo "      ]," >> "$2"
  fi
  echo "    }," >> "$2"
}


# Generate a gypi which applies additional compiler flags based on the file
# name.
# $1 - Array name for file list.
# $2 - Output file
function write_intrinsics_gypi {
  declare -a file_list=("${!1}")

  local mmx_sources=$(echo "$file_list" | grep '_mmx\.c$')
  local sse2_sources=$(echo "$file_list" | grep '_sse2\.c$')
  local sse3_sources=$(echo "$file_list" | grep '_sse3\.c$')
  local ssse3_sources=$(echo "$file_list" | grep '_ssse3\.c$')
  local sse4_1_sources=$(echo "$file_list" | grep '_sse4\.c$')
  local avx_sources=$(echo "$file_list" | grep '_avx\.c$')
  local avx2_sources=$(echo "$file_list" | grep '_avx2\.c$')
  local neon_sources=$(echo "$file_list" | grep '_neon\.c$\|\.asm$')

  # Intrinsic functions and files are in flux. We can selectively generate them
  # but we can not selectively include them in libvpx.gyp. Throw some errors
  # when new targets are needed.

  rm -rf "$2"
  write_license "$2"
  write_gypi_header "$2"

  echo "  'targets': [" >> "$2"

  # x86[_64]
  if [ 0 -ne ${#mmx_sources} ]; then
    write_target_definition mmx_sources[@] "$2" libvpx_intrinsics_mmx mmx
  fi
  if [ 0 -ne ${#sse2_sources} ]; then
    write_target_definition sse2_sources[@] "$2" libvpx_intrinsics_sse2 sse2
  fi
  if [ 0 -ne ${#sse3_sources} ]; then
    #write_target_definition sse3_sources[@] "$2" libvpx_intrinsics_sse3 sse3
    echo "ERROR: Uncomment sse3 sections in libvpx.gyp"
    exit 1
  fi
  if [ 0 -ne ${#ssse3_sources} ]; then
    write_target_definition ssse3_sources[@] "$2" libvpx_intrinsics_ssse3 ssse3
  fi
  if [ 0 -ne ${#sse4_1_sources} ]; then
    write_target_definition sse4_1_sources[@] "$2" libvpx_intrinsics_sse4_1 sse4.1
  fi
  if [[ -z $DISABLE_AVX && 0 -ne ${#avx_sources} ]]; then
    write_target_definition avx_sources[@] "$2" libvpx_intrinsics_avx avx
  fi
  if [[ -z $DISABLE_AVX && 0 -ne ${#avx2_sources} ]]; then
    write_target_definition avx2_sources[@] "$2" libvpx_intrinsics_avx2 avx2
  fi

  # arm neon
  if [ 0 -ne ${#neon_sources} ]; then
    write_target_definition neon_sources[@] "$2" libvpx_intrinsics_neon fpu=neon
  fi

  echo "  ]," >> "$2"

  write_gypi_footer "$2"
}

# Convert a list of source files into gypi and gni files.
# $1 - Input file.
# $2 - Output gypi file base. Will generate additional .gypi files when
#      different compilation flags are required.
function convert_srcs_to_project_files {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 2. Move certain files to a separate include to allow applying different
  #    compiler options.
  # 3. Replace .asm.s to .asm because gyp will do the conversion.

  local source_list=$(grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1)

  # Not sure why vpx_config is not included.
  source_list=$(echo "$source_list" | grep -v 'vpx_config\.c')

  # The actual ARM files end in .asm. We have rules to translate them to .S
  source_list=$(echo "$source_list" | sed s/\.asm\.s$/.asm/)

  # Select all x86 files ending with .c
  local intrinsic_list=$(echo "$source_list" | \
    egrep '(mmx|sse2|sse3|ssse3|sse4|avx|avx2).c$')

  # Select all neon files ending in C but only when building in RTCD mode
  if [ "libvpx_srcs_arm_neon_cpu_detect" == "$2" ]; then
    # Select all arm neon files ending in _neon.c and all asm files.
    # The asm files need to be included in the intrinsics target because
    # they need the -mfpu=neon flag.
    # the pattern may need to be updated if vpx_scale gets intrinics
    local intrinsic_list=$(echo "$source_list" | \
      egrep 'neon.*(\.c|\.asm)$')
  fi

  # Remove these files from the main list.
  source_list=$(comm -23 <(echo "$source_list") <(echo "$intrinsic_list"))

  local x86_list=$(echo "$source_list" | egrep '/x86/')

  write_gypi source_list "$BASE_DIR/$2.gypi"

  # All the files are in a single "element." Check if the first element has
  # length 0.
  if [ 0 -ne ${#intrinsic_list} ]; then
    write_intrinsics_gypi intrinsic_list[@] "$BASE_DIR/$2_intrinsics.gypi"
  fi

  # Write a single .gni file that includes all source files for all archs.
  if [ 0 -ne ${#x86_list} ]; then
    local c_sources=$(echo "$source_list" | egrep '.(c|h)$')
    local assembly_sources=$(echo "$source_list" | egrep '.asm$')
    local mmx_sources=$(echo "$intrinsic_list" | grep '_mmx\.c$')
    local sse2_sources=$(echo "$intrinsic_list" | grep '_sse2\.c$')
    local sse3_sources=$(echo "$intrinsic_list" | grep '_sse3\.c$')
    local ssse3_sources=$(echo "$intrinsic_list" | grep '_ssse3\.c$')
    local sse4_1_sources=$(echo "$intrinsic_list" | grep '_sse4\.c$')
    local avx_sources=$(echo "$intrinsic_list" | grep '_avx\.c$')
    local avx2_sources=$(echo "$intrinsic_list" | grep '_avx2\.c$')

    write_gni c_sources $2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni assembly_sources $2_assembly "$BASE_DIR/libvpx_srcs.gni"
    write_gni mmx_sources $2_mmx "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse2_sources $2_sse2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse3_sources $2_sse3 "$BASE_DIR/libvpx_srcs.gni"
    write_gni ssse3_sources $2_ssse3 "$BASE_DIR/libvpx_srcs.gni"
    write_gni sse4_1_sources $2_sse4_1 "$BASE_DIR/libvpx_srcs.gni"
    if [ -z "$DISABLE_AVX" ]; then
      write_gni avx_sources $2_avx "$BASE_DIR/libvpx_srcs.gni"
      write_gni avx2_sources $2_avx2 "$BASE_DIR/libvpx_srcs.gni"
    fi
  else
    local c_sources=$(echo "$source_list" | egrep '.(c|h)$')
    local assembly_sources=$(echo -e "$source_list\n$intrinsic_list" | \
      egrep '.asm$')
    local neon_sources=$(echo "$intrinsic_list" | grep '_neon\.c$')
    write_gni c_sources $2 "$BASE_DIR/libvpx_srcs.gni"
    write_gni assembly_sources $2_assembly "$BASE_DIR/libvpx_srcs.gni"
    if [ 0 -ne ${#neon_sources} ]; then
      write_gni neon_sources $2_neon "$BASE_DIR/libvpx_srcs.gni"
    fi
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
  if [[ "$1" != *mipsel && "$1" != *mips64el ]]; then
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
  if [[ "$2" == "mipsel" || "$2" == "mips64el" ]]; then
    print_config_basic $1 > $BASE_DIR/$TEMP_DIR/libvpx.config
  else
    $BASE_DIR/lint_config.sh -p \
      -h $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.h \
      -a $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_config.asm \
      -o $BASE_DIR/$TEMP_DIR/libvpx.config
  fi

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp8_rtcd $DISABLE_AVX \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp8/common/rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp8_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vp9_rtcd $DISABLE_AVX \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vp9/common/vp9_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vp9_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_scale_rtcd $DISABLE_AVX \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_scale/vpx_scale_rtcd.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_scale_rtcd.h

  $BASE_DIR/$LIBVPX_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=vpx_dsp_rtcd $DISABLE_AVX \
    --config=$BASE_DIR/$TEMP_DIR/libvpx.config \
    $BASE_DIR/$LIBVPX_SRC_DIR/vpx_dsp/vpx_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBVPX_CONFIG_DIR/$1/vpx_dsp_rtcd.h

  rm -rf $BASE_DIR/$TEMP_DIR/libvpx.config
}

# Generate Config files. "--enable-external-build" must be set to skip
# detection of capabilities on specific targets.
# $1 - Header file directory.
# $2 - Config command line.
function gen_config_files {
  ./configure $2 > /dev/null

  # Disable HAVE_UNISTD_H as it causes vp8 to try to detect how many cpus
  # available, which doesn't work from iniside a sandbox on linux.
  ( echo '/HAVE_UNISTD_H/s/[01]/0/' ; echo 'w' ; echo 'q' ) | ed -s vpx_config.h

  # Generate vpx_config.asm. Do not create one for mips.
  if [[ "$1" != *mipsel && "$1" != *mips64el ]]; then
    if [[ "$1" == *x64* ]] || [[ "$1" == *ia32* ]]; then
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print "%define " $2 " " $3}' > vpx_config.asm
    else
      egrep "#define [A-Z0-9_]+ [01]" vpx_config.h | awk '{print $2 " EQU " $3}' | perl $BASE_DIR/$LIBVPX_SRC_DIR/build/make/ads2gas.pl > vpx_config.asm
    fi
  fi

  cp vpx_config.* $BASE_DIR/$LIBVPX_CONFIG_DIR/$1
  make_clean
  rm -rf vpx_config.*
}

find_duplicates

echo "Create temporary directory."
TEMP_DIR="$LIBVPX_SRC_DIR.temp"
rm -rf $TEMP_DIR
cp -R $LIBVPX_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Generate config files."
all_platforms="--enable-external-build --enable-postproc --disable-install-srcs --enable-multi-res-encoding --enable-temporal-denoising --disable-unit-tests --disable-install-docs --disable-examples --enable-vp9-temporal-denoising --enable-vp9-postproc --size-limit=16384x16384 $DISABLE_AVX --as=yasm"
gen_config_files linux/ia32 "--target=x86-linux-gcc --disable-ccache --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files linux/x64 "--target=x86_64-linux-gcc --disable-ccache --enable-pic --enable-realtime-only ${all_platforms}"
gen_config_files linux/arm "--target=armv6-linux-gcc --enable-pic --enable-realtime-only --disable-install-bins --disable-install-libs --disable-edsp ${all_platforms}"
gen_config_files linux/arm-neon "--target=armv7-linux-gcc --enable-pic --enable-realtime-only --disable-edsp ${all_platforms}"
gen_config_files linux/arm-neon-cpu-detect "--target=armv7-linux-gcc --enable-pic --enable-realtime-only --enable-runtime-cpu-detect --disable-edsp ${all_platforms}"
gen_config_files linux/arm64 "--force-target=armv8-linux-gcc --enable-pic --enable-realtime-only --disable-edsp ${all_platforms}"
gen_config_files linux/mipsel "--target=mips32-linux-gcc ${all_platforms}"
gen_config_files linux/mips64el "--target=mips64-linux-gcc ${all_platforms}"
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
lint_config linux/mips64el
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
gen_rtcd_header linux/mips64el mips64el
gen_rtcd_header linux/generic generic
gen_rtcd_header win/ia32 x86
gen_rtcd_header win/x64 x86_64
gen_rtcd_header mac/ia32 x86
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header nacl nacl

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

if [ -z $ONLY_CONFIGS ]; then
  # Remove existing .gni file.
  rm -rf $BASE_DIR/libvpx_srcs.gni
  write_license $BASE_DIR/libvpx_srcs.gni

  echo "Generate X86 source list."
  config=$(print_config linux/ia32)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_x86

  # Copy vpx_version.h. The file should be the same for all platforms.
  cp vpx_version.h $BASE_DIR/$LIBVPX_CONFIG_DIR

  echo "Generate X86_64 source list."
  config=$(print_config linux/x64)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_x86_64

  echo "Generate ARM source list."
  config=$(print_config linux/arm)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm

  echo "Generate ARM NEON source list."
  config=$(print_config linux/arm-neon)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm_neon

  echo "Generate ARM NEON CPU DETECT source list."
  config=$(print_config linux/arm-neon-cpu-detect)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm_neon_cpu_detect

  echo "Generate ARM64 source list."
  config=$(print_config linux/arm64)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_arm64

  echo "Generate MIPS source list."
  config=$(print_config_basic linux/mipsel)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_mips

  echo "MIPS64 source list is identical to MIPS source list. No need to generate it."

  echo "Generate NaCl source list."
  config=$(print_config_basic nacl)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_nacl

  echo "Generate GENERIC source list."
  config=$(print_config_basic linux/generic)
  make_clean
  make libvpx_srcs.txt target=libs $config > /dev/null
  convert_srcs_to_project_files libvpx_srcs.txt libvpx_srcs_generic
fi

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

gn format --in-place $BASE_DIR/BUILD.gn
gn format --in-place $BASE_DIR/libvpx_srcs.gni

cd $BASE_DIR/$LIBVPX_SRC_DIR
echo
echo "Update README.chromium:"
git log -1 --format="%cd%nCommit: %H" --date=format:"Date: %A %B %d %Y"

cd $BASE_DIR

# TODO(fgalligan): Can we turn on "--enable-realtime-only" for mipsel?
