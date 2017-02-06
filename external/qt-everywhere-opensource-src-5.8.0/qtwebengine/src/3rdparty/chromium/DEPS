vars = {
  'angle_revision':
    '3c754194a62c0ed8a3101ff1da88f77b35c7f69a',
  'boringssl_revision':
    '3cab5572b1fcf5a8f6018529dc30dc8d21b2a4bd',
  'buildspec_platforms':
    'all',
  'buildtools_revision':
    '454e53abae6e4d68ee992b0a93a4174b75519393',
  'catapult_revision':
    'fd47e24b0e0912f153cd68a5dfc995d6f7cd2b81',
  'chromium_git':
    'https://chromium.googlesource.com',
  'freetype_android_revision':
    'a512b0fe7a8d9db0e5aa9c0a4db1e92cb861722d',
  'google_toolbox_for_mac_revision':
    'e7b41fad2e7f77abf560c79f984c60d5ec6d419c',
  'googlecode_url':
    'http://%s.googlecode.com/svn',
  'libfuzzer_revision':
    '7ee243229b1df318115b71b656c14879e8c16a71',
  'lighttpd_revision':
    '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  'lss_revision':
    '4fc942258fe5509549333b9487ec018e3c8c5b10',
  'nacl_revision':
    '41bda598ad1ef18a9c5fde96aa4be24268fc0994',
  'openmax_dl_revision':
    '6670e52d32351145a6b6c198dab3f6a536edf3db',
  'pdfium_revision':
    '017052a077d8f900e3098b8f9115078a037ee805',
  'sfntly_revision':
    '468cad540fa1b0027cad60456f53feabecdce2bc',
  'skia_revision':
    'df4f47b8ff6378c4d8f775dcb3169ac7c64f2510',
  'swarming_revision':
    'df6e95e7669883c8fe9ef956c69a544154701a49',
  'v8_revision':
    '820a23aade5e74a92d794e05a0c2b3597f0da4b5'
}

allowed_hosts = [
  'android.googlesource.com',
  'boringssl.googlesource.com',
  'chromium.googlesource.com',
  'pdfium.googlesource.com'
]

deps = {
  'src/breakpad/src':
    (Var("chromium_git")) + '/breakpad/breakpad/src.git@7c70f47255713629c8053826008bd00f98d71ced',
  'src/buildtools':
    (Var("chromium_git")) + '/chromium/buildtools.git@454e53abae6e4d68ee992b0a93a4174b75519393',
  'src/chrome/test/data/perf/canvas_bench':
    (Var("chromium_git")) + '/chromium/canvas_bench.git@a7b40ea5ae0239517d78845a5fc9b12976bfc732',
  'src/chrome/test/data/perf/frame_rate/content':
    (Var("chromium_git")) + '/chromium/frame_rate/content.git@c10272c88463efeef6bb19c9ec07c42bc8fe22b9',
  'src/media/cdm/api':
    (Var("chromium_git")) + '/chromium/cdm.git@245af7782c9f54d776722a2c7b53372ee040e5fc',
  'src/native_client':
    (Var("chromium_git")) + '/native_client/src/native_client.git@41bda598ad1ef18a9c5fde96aa4be24268fc0994',
  'src/sdch/open-vcdiff':
    (Var("chromium_git")) + '/external/github.com/google/open-vcdiff.git@21d7d0b9c3d0c3ccbdb221c85ae889373f0a2a58',
  'src/testing/gmock':
    (Var("chromium_git")) + '/external/googlemock.git@0421b6f358139f02e102c9c332ce19a33faf75be',
  'src/testing/gtest':
    (Var("chromium_git")) + '/external/github.com/google/googletest.git@6f8a66431cb592dad629028a50b3dd418a408c87',
  'src/third_party/SPIRV-Tools/src':
    (Var("chromium_git")) + '/external/github.com/KhronosGroup/SPIRV-Tools.git@9166854ac93ef81b026e943ccd230fed6c8b8d3c',
  'src/third_party/angle':
    (Var("chromium_git")) + '/angle/angle.git@c9e8931198428ce58a7c2c772539186ffa3e1aca',
  'src/third_party/bidichecker':
    (Var("chromium_git")) + '/external/bidichecker/lib.git@97f2aa645b74c28c57eca56992235c79850fa9e0',
  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git@3cab5572b1fcf5a8f6018529dc30dc8d21b2a4bd',
  'src/third_party/catapult':
    (Var("chromium_git")) + '/external/github.com/catapult-project/catapult.git@fd47e24b0e0912f153cd68a5dfc995d6f7cd2b81',
  'src/third_party/ced/src':
    (Var("chromium_git")) + '/external/github.com/google/compact_enc_det.git@8ed7d41a5dc6539af30dc05449d10829a51ed183',
  'src/third_party/cld_2/src':
    (Var("chromium_git")) + '/external/github.com/CLD2Owners/cld2.git@84b58a5d7690ebf05a91406f371ce00c3daf31c0',
  'src/third_party/colorama/src':
    (Var("chromium_git")) + '/external/colorama.git@799604a1041e9b3bc5d2789ecbd7e8db2e18e6b8',
  'src/third_party/dom_distiller_js/dist':
    (Var("chromium_git")) + '/external/github.com/chromium/dom-distiller-dist.git@e87b378779c5641f50fc7ef885a2f88f26e47b7f',
  'src/third_party/ffmpeg':
    (Var("chromium_git")) + '/chromium/third_party/ffmpeg.git@d45f90eac6d5eb22be6120b1f7cb145f7a7c88b0',
  'src/third_party/flac':
    (Var("chromium_git")) + '/chromium/deps/flac.git@812243a85937e06102ba312c6caf8823e243b35b',
  'src/third_party/flatbuffers/src':
    (Var("chromium_git")) + '/external/github.com/google/flatbuffers.git@e92ae5199d52fd59540a800bec7eef46cd778257',
  'src/third_party/glslang/src':
    (Var("chromium_git")) + '/external/github.com/google/glslang.git@210c6bf4d8119dc5f8ac21da2d4c87184f7015e0',
  'src/third_party/hunspell_dictionaries':
    (Var("chromium_git")) + '/chromium/deps/hunspell_dictionaries.git@dc6e7c25bf47cbfb466e0701fd2728b4a12e79d5',
  'src/third_party/icu':
    (Var("chromium_git")) + '/chromium/deps/icu.git@6018b6f0668cdf38fda2db1462dfe830b63cbdcb',
  'src/third_party/jsoncpp/source':
    (Var("chromium_git")) + '/external/github.com/open-source-parsers/jsoncpp.git@f572e8e42e22cfcf5ab0aea26574f408943edfa4',
  'src/third_party/leveldatabase/src':
    (Var("chromium_git")) + '/external/leveldb.git@a7bff697baa062c8f6b8fb760eacf658712b611a',
  'src/third_party/libFuzzer/src':
    (Var("chromium_git")) + '/chromium/llvm-project/llvm/lib/Fuzzer.git@7ee243229b1df318115b71b656c14879e8c16a71',
  'src/third_party/libaddressinput/src':
    (Var("chromium_git")) + '/external/libaddressinput.git@5eeeb797e79fa01503fcdcbebdc50036fac023ef',
  'src/third_party/libjpeg_turbo':
    (Var("chromium_git")) + '/chromium/deps/libjpeg_turbo.git@7260e4d8b8e1e40b17f03fafdf1cd83296900f76',
  'src/third_party/libphonenumber/dist':
    (Var("chromium_git")) + '/external/libphonenumber.git@a4da30df63a097d67e3c429ead6790ad91d36cf4',
  'src/third_party/libsrtp':
    (Var("chromium_git")) + '/chromium/deps/libsrtp.git@720780acf8fa41c4a6ad515d0382d62f8f5195eb',
  'src/third_party/libvpx/source/libvpx':
    (Var("chromium_git")) + '/webm/libvpx.git@1c0a9f36f15a83816aebe5395e19c21f3d545aa2',
  'src/third_party/libwebm/source':
    (Var("chromium_git")) + '/webm/libwebm.git@9a235e0bc94319c5f7184bd69cbe5468a74a025c',
  'src/third_party/libyuv':
    (Var("chromium_git")) + '/libyuv/libyuv.git@76aee8ced7ca74c724d69c1dcf9891348450c8e8',
  'src/third_party/mesa/src':
    (Var("chromium_git")) + '/chromium/deps/mesa.git@ef811c6bd4de74e13e7035ca882cc77f85793fef',
  'src/third_party/openh264/src':
    (Var("chromium_git")) + '/external/github.com/cisco/openh264@b37cda248234162033e3e11b0335f3131cdfe488',
  'src/third_party/openmax_dl':
    (Var("chromium_git")) + '/external/webrtc/deps/third_party/openmax.git@6670e52d32351145a6b6c198dab3f6a536edf3db',
  'src/third_party/opus/src':
    (Var("chromium_git")) + '/chromium/deps/opus.git@655cc54c564b84ef2827f0b2152ce3811046201e',
  'src/third_party/pdfium':
    'https://pdfium.googlesource.com/pdfium.git@b48a715c301758ff66a96f74e3921c121b317603',
  'src/third_party/py_trace_event/src':
    (Var("chromium_git")) + '/external/py_trace_event.git@dd463ea9e2c430de2b9e53dea57a77b4c3ac9b30',
  'src/third_party/pyftpdlib/src':
    (Var("chromium_git")) + '/external/pyftpdlib.git@2be6d65e31c7ee6320d059f581f05ae8d89d7e45',
  'src/third_party/pywebsocket/src':
    (Var("chromium_git")) + '/external/github.com/google/pywebsocket.git@2d7b73c3acbd0f41dcab487ae5c97c6feae06ce2',
  'src/third_party/re2/src':
    (Var("chromium_git")) + '/external/github.com/google/re2.git@dba3349aba83b5588e85e5ecf2b56c97f2d259b7',
  'src/third_party/scons-2.0.1':
    (Var("chromium_git")) + '/native_client/src/third_party/scons-2.0.1.git@1c1550e17fc26355d08627fbdec13d8291227067',
  'src/third_party/sfntly/src':
    (Var("chromium_git")) + '/external/github.com/googlei18n/sfntly.git@468cad540fa1b0027cad60456f53feabecdce2bc',
  'src/third_party/shaderc/src':
    (Var("chromium_git")) + '/external/github.com/google/shaderc.git@cd8793c34907073025af2622c28bcee64e9879a4',
  'src/third_party/skia':
    (Var("chromium_git")) + '/skia.git@6367bfa90c99d0af22eaa8733ef69e9e8eae304a',
  'src/third_party/smhasher/src':
    (Var("chromium_git")) + '/external/smhasher.git@e87738e57558e0ec472b2fc3a643b838e5b6e88f',
  'src/third_party/snappy/src':
    (Var("chromium_git")) + '/external/snappy.git@762bb32f0c9d2f31ba4958c7c0933d22e80c20bf',
  'src/third_party/usrsctp/usrsctplib':
    (Var("chromium_git")) + '/external/github.com/sctplab/usrsctp@c60ec8b35c3fe6027d7a3faae89d1c8d7dd3ce98',
  'src/third_party/webdriver/pylib':
    (Var("chromium_git")) + '/external/selenium/py.git@5fd78261a75fe08d27ca4835fb6c5ce4b42275bd',
  'src/third_party/webgl/src':
    (Var("chromium_git")) + '/external/khronosgroup/webgl.git@53f132deb0974fd93c8d4620fe08ae00d8b112c3',
  'src/third_party/webpagereplay':
    (Var("chromium_git")) + '/external/github.com/chromium/web-page-replay.git@3cd3a3f6f06a1b87b14b9162c7eb16d23d141241',
  'src/third_party/webrtc':
    (Var("chromium_git")) + '/external/webrtc/trunk/webrtc.git@19ef428c81f230a1c809cf42089ba7e00051d0a0',
  'src/third_party/yasm/source/patched-yasm':
    (Var("chromium_git")) + '/chromium/deps/yasm/patched-yasm.git@7da28c6c7c6a1387217352ce02b31754deb54d2a',
  'src/tools/gyp':
    (Var("chromium_git")) + '/external/gyp.git@c61b0b35c8396bfd59efc6cfc11401d912b0f510',
  'src/tools/page_cycler/acid3':
    (Var("chromium_git")) + '/chromium/deps/acid3.git@6be0a66a1ebd7ebc5abc1b2f405a945f6d871521',
  'src/tools/swarming_client':
    (Var("chromium_git")) + '/external/swarming.client.git@df6e95e7669883c8fe9ef956c69a544154701a49',
  'src/v8':
    (Var("chromium_git")) + '/v8/v8.git@0431e266f3df7565381a20da8d303c06d5ea6cf5'
}

deps_os = {
  'android': {
    'src/third_party/android_protobuf/src':
      (Var("chromium_git")) + '/external/android_protobuf.git@999188d0dc72e97f7fe08bb756958a2cf090f4e7',
    'src/third_party/android_tools':
      (Var("chromium_git")) + '/android_tools.git@5b5f2f60b78198eaef25d442ac60f823142a8a6e',
    'src/third_party/apache-mime4j':
      (Var("chromium_git")) + '/chromium/deps/apache-mime4j.git@28cb1108bff4b6cf0a2e86ff58b3d025934ebe3a',
    'src/third_party/apache-portable-runtime/src':
      (Var("chromium_git")) + '/external/apache-portable-runtime.git@c76a8c4277e09a82eaa229e35246edea1ee0a6a1',
    'src/third_party/appurify-python/src':
      (Var("chromium_git")) + '/external/github.com/appurify/appurify-python.git@ee7abd5c5ae3106f72b2a0b9d2cb55094688e867',
    'src/third_party/cardboard-java/src':
      (Var("chromium_git")) + '/external/github.com/googlesamples/cardboard-java.git@e36ee57e72bbd057ddb53b127954177b50e18df7',
    'src/third_party/custom_tabs_client/src':
      (Var("chromium_git")) + '/external/github.com/GoogleChrome/custom-tabs-client.git@c51efbddc4f976c88d15c730a79feba65ca857af',
    'src/third_party/elfutils/src':
      (Var("chromium_git")) + '/external/elfutils.git@249673729a7e5dbd5de4f3760bdcaa3d23d154d7',
    'src/third_party/errorprone/lib':
      (Var("chromium_git")) + '/chromium/third_party/errorprone.git@0eea83b66343133b9c76b7d3288c30321818ebcf',
    'src/third_party/findbugs':
      (Var("chromium_git")) + '/chromium/deps/findbugs.git@57f05238d3ac77ea0a194813d3065dd780c6e566',
    'src/third_party/freetype-android/src':
      (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@a512b0fe7a8d9db0e5aa9c0a4db1e92cb861722d',
    'src/third_party/httpcomponents-client':
      (Var("chromium_git")) + '/chromium/deps/httpcomponents-client.git@285c4dafc5de0e853fa845dce5773e223219601c',
    'src/third_party/httpcomponents-core':
      (Var("chromium_git")) + '/chromium/deps/httpcomponents-core.git@9f7180a96f8fa5cab23f793c14b413356d419e62',
    'src/third_party/jsr-305/src':
      (Var("chromium_git")) + '/external/jsr-305.git@642c508235471f7220af6d5df2d3210e3bfc0919',
    'src/third_party/junit/src':
      (Var("chromium_git")) + '/external/junit.git@45a44647e7306262162e1346b750c3209019f2e1',
    'src/third_party/leakcanary/src':
      (Var("chromium_git")) + '/external/github.com/square/leakcanary.git@608ded739e036a3aa69db47ac43777dcee506f8e',
    'src/third_party/lss':
      (Var("chromium_git")) + '/external/linux-syscall-support/lss.git@4fc942258fe5509549333b9487ec018e3c8c5b10',
    'src/third_party/mockito/src':
      (Var("chromium_git")) + '/external/mockito/mockito.git@4d987dcd923b81525c42b1333e6c4e07440776c3',
    'src/third_party/netty-tcnative/src':
      (Var("chromium_git")) + '/external/netty-tcnative.git@dba66573998801a08ea41b605b1629857ae02a6b',
    'src/third_party/netty4/src':
      (Var("chromium_git")) + '/external/netty4.git@e0f26303b4ce635365be19414d0ac81f2ef6ba3c',
    'src/third_party/requests/src':
      (Var("chromium_git")) + '/external/github.com/kennethreitz/requests.git@f172b30356d821d180fa4ecfa3e71c7274a32de4',
    'src/third_party/robolectric/lib':
      (Var("chromium_git")) + '/chromium/third_party/robolectric.git@6b63c99a8b6967acdb42cbed0adb067c80efc810',
    'src/third_party/ub-uiautomator/lib':
      (Var("chromium_git")) + '/chromium/third_party/ub-uiautomator.git@00270549ce3161ae72ceb24712618ea28b4f9434'
  },
  'ios': {
    'src/ios/third_party/earl_grey/src':
      (Var("chromium_git")) + '/external/github.com/google/EarlGrey.git@6b23028b634974fb72dc960f2c063949076cf78f',
    'src/ios/third_party/fishhook/src':
      (Var("chromium_git")) + '/external/github.com/facebook/fishhook.git@d172d5247aa590c25d0b1885448bae76036ea22c',
    'src/ios/third_party/gcdwebserver/src':
      (Var("chromium_git")) + '/external/github.com/swisspol/GCDWebServer.git@3d5fd0b8281a7224c057deb2d17709b5bea64836',
    'src/ios/third_party/ochamcrest/src':
      (Var("chromium_git")) + '/external/github.com/hamcrest/OCHamcrest.git@d7ee4ecfb6bd13c3c8d364682b6228ccd86e1e1a',
    'src/third_party/class-dump/src':
      (Var("chromium_git")) + '/external/github.com/nygard/class-dump.git@978d177ca6f0d2e5e34acf3e8dadc63e3140ebbc',
    'src/third_party/google_toolbox_for_mac/src':
      (Var("chromium_git")) + '/external/github.com/google/google-toolbox-for-mac.git@e7b41fad2e7f77abf560c79f984c60d5ec6d419c'
  },
  'mac': {
    'src/chrome/installer/mac/third_party/xz/xz':
      (Var("chromium_git")) + '/chromium/deps/xz.git@eecaf55632ca72e90eb2641376bce7cdbc7284f7',
    'src/third_party/google_toolbox_for_mac/src':
      (Var("chromium_git")) + '/external/github.com/google/google-toolbox-for-mac.git@e7b41fad2e7f77abf560c79f984c60d5ec6d419c',
    'src/third_party/lighttpd':
      (Var("chromium_git")) + '/chromium/deps/lighttpd.git@9dfa55d15937a688a92cbf2b7a8621b0927d06eb'
  },
  'unix': {
    'src/third_party/chromite':
      (Var("chromium_git")) + '/chromiumos/chromite.git@8c1b19c098bd84bdefdea93da75821b082f0b395',
    'src/third_party/cros_system_api':
      (Var("chromium_git")) + '/chromiumos/platform/system_api.git@2cf87623ebe64dbff665490900713695ba7c5113',
    'src/third_party/fontconfig/src':
      (Var("chromium_git")) + '/external/fontconfig.git@f16c3118e25546c1b749f9823c51827a60aeb5c1',
    'src/third_party/freetype-android/src':
      (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@a512b0fe7a8d9db0e5aa9c0a4db1e92cb861722d',
    'src/third_party/freetype2/src':
      (Var("chromium_git")) + '/chromium/src/third_party/freetype2.git@fc1532a7c4c592f24a4c1a0261d2845524ca5cff',
    'src/third_party/grpc':
      (Var("chromium_git")) + '/external/github.com/grpc/grpc@c1f618fb61b5258c1718e7ae60a6a7dbf6f912fb',
    'src/third_party/liblouis/src':
      (Var("chromium_git")) + '/external/liblouis-github.git@5f9c03f2a3478561deb6ae4798175094be8a26c2',
    'src/third_party/lss':
      (Var("chromium_git")) + '/external/linux-syscall-support/lss.git@4fc942258fe5509549333b9487ec018e3c8c5b10',
    'src/third_party/minigbm/src':
      (Var("chromium_git")) + '/chromiumos/platform/minigbm.git@f9e7c4c8e91fb354abf714c7a5c89b4990814ff3',
    'src/third_party/pyelftools':
      (Var("chromium_git")) + '/chromiumos/third_party/pyelftools.git@19b3e610c86fcadb837d252c794cb5e8008826ae',
    'src/third_party/wayland-protocols/src':
      (Var("chromium_git")) + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git@65d09ef404fb26cb513f4f836e904f415c425a8f',
    'src/third_party/wayland/src':
      (Var("chromium_git")) + '/external/anongit.freedesktop.org/git/wayland/wayland.git@47163797f810373c81b6e13b7a8a245eb9877785',
    'src/third_party/wds/src':
      (Var("chromium_git")) + '/external/github.com/01org/wds@ac3d8210d95f3000bf5c8e16a79dbbbf22d554a5',
    'src/third_party/xdg-utils':
      (Var("chromium_git")) + '/chromium/deps/xdg-utils.git@d80274d5869b17b8c9067a1022e4416ee7ed5e0d'
  },
  'win': {
    'src/third_party/bison':
      (Var("chromium_git")) + '/chromium/deps/bison.git@083c9a45e4affdd5464ee2b224c2df649c6e26c3',
    'src/third_party/cygwin':
      (Var("chromium_git")) + '/chromium/deps/cygwin.git@c89e446b273697fadf3a10ff1007a97c0b7de6df',
    'src/third_party/gnu_binutils':
      (Var("chromium_git")) + '/native_client/deps/third_party/gnu_binutils.git@f4003433b61b25666565690caf3d7a7a1a4ec436',
    'src/third_party/gperf':
      (Var("chromium_git")) + '/chromium/deps/gperf.git@d892d79f64f9449770443fb06da49b5a1e5d33c1',
    'src/third_party/lighttpd':
      (Var("chromium_git")) + '/chromium/deps/lighttpd.git@9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
    'src/third_party/mingw-w64/mingw/bin':
      (Var("chromium_git")) + '/native_client/deps/third_party/mingw-w64/mingw/bin.git@3cc8b140b883a9fe4986d12cfd46c16a093d3527',
    'src/third_party/nacl_sdk_binaries':
      (Var("chromium_git")) + '/chromium/deps/nacl_sdk_binaries.git@759dfca03bdc774da7ecbf974f6e2b84f43699a5',
    'src/third_party/pefile':
      (Var("chromium_git")) + '/external/pefile.git@72c6ae42396cb913bcab63c15585dc3b5c3f92f1',
    'src/third_party/perl':
      (Var("chromium_git")) + '/chromium/deps/perl.git@ac0d98b5cee6c024b0cffeb4f8f45b6fc5ccdb78',
    'src/third_party/psyco_win32':
      (Var("chromium_git")) + '/chromium/deps/psyco_win32.git@f5af9f6910ee5a8075bbaeed0591469f1661d868',
    'src/third_party/yasm/binaries':
      (Var("chromium_git")) + '/chromium/deps/yasm/binaries.git@52f9b3f4b0aa06da24ef8b123058bb61ee468881'
  }
}

hooks = [
  {
    'action': [
      'python',
      'src/build/landmines.py'
    ],
    'pattern':
      '.',
    'name':
      'landmines'
  },
  {
    'action': [
      'python',
      'src/tools/remove_stale_pyc_files.py',
      'src/android_webview/tools',
      'src/build/android',
      'src/gpu/gles2_conform_support',
      'src/infra',
      'src/ppapi',
      'src/printing',
      'src/third_party/catapult',
      'src/third_party/closure_compiler/build',
      'src/tools'
    ],
    'pattern':
      '.',
    'name':
      'remove_stale_pyc_files'
  },
  {
    'action': [
      'python',
      'src/build/download_nacl_toolchains.py',
      '--mode',
      'nacl_core_sdk',
      'sync',
      '--extract'
    ],
    'pattern':
      '.',
    'name':
      'nacltools'
  },
  {
    'action': [
      'python',
      'src/build/android/play_services/update.py',
      'download'
    ],
    'pattern':
      '.',
    'name':
      'sdkextras'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-javax-inject',
      '-l',
      'third_party/javax_inject'
    ],
    'pattern':
      '.',
    'name':
      'javax_inject'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-hamcrest',
      '-l',
      'third_party/hamcrest'
    ],
    'pattern':
      '.',
    'name':
      'hamcrest'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-guava',
      '-l',
      'third_party/guava'
    ],
    'pattern':
      '.',
    'name':
      'guava'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-android-support-test-runner',
      '-l',
      'third_party/android_support_test_runner'
    ],
    'pattern':
      '.',
    'name':
      'android_support_test_runner'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-espresso',
      '-l',
      'third_party/espresso'
    ],
    'pattern':
      '.',
    'name':
      'espresso'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-apache-velocity',
      '-l',
      'third_party/apache_velocity'
    ],
    'pattern':
      '.',
    'name':
      'apache_velocity'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-ow2-asm',
      '-l',
      'third_party/ow2_asm'
    ],
    'pattern':
      '.',
    'name':
      'ow2_asm'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-icu4j',
      '-l',
      'third_party/icu4j'
    ],
    'pattern':
      '.',
    'name':
      'icu4j'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-accessibility-test-framework',
      '-l',
      'third_party/accessibility_test_framework'
    ],
    'pattern':
      '.',
    'name':
      'accessibility_test_framework'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-bouncycastle',
      '-l',
      'third_party/bouncycastle'
    ],
    'pattern':
      '.',
    'name':
      'bouncycastle'
  },
  {
    'action': [
      'python',
      'src/build/android/update_deps/update_third_party_deps.py',
      'download',
      '-b',
      'chromium-sqlite4java',
      '-l',
      'third_party/sqlite4java'
    ],
    'pattern':
      '.',
    'name':
      'sqlite4java'
  },
  {
    'action': [
      'python',
      'src/build/linux/sysroot_scripts/install-sysroot.py',
      '--running-as-hook'
    ],
    'pattern':
      '.',
    'name':
      'sysroot'
  },
  {
    'action': [
      'python',
      'src/build/vs_toolchain.py',
      'update'
    ],
    'pattern':
      '.',
    'name':
      'win_toolchain'
  },
  {
    'action': [
      'python',
      'src/build/mac_toolchain.py'
    ],
    'pattern':
      '.',
    'name':
      'mac_toolchain'
  },
  {
    'action': [
      'python',
      'src/third_party/binutils/download.py'
    ],
    'pattern':
      'src/third_party/binutils',
    'name':
      'binutils'
  },
  {
    'action': [
      'python',
      'src/tools/clang/scripts/update.py',
      '--if-needed'
    ],
    'pattern':
      '.',
    'name':
      'clang'
  },
  {
    'action': [
      'python',
      'src/build/util/lastchange.py',
      '-o',
      'src/build/util/LASTCHANGE'
    ],
    'pattern':
      '.',
    'name':
      'lastchange'
  },
  {
    'action': [
      'python',
      'src/build/util/lastchange.py',
      '--git-hash-only',
      '-s',
      'src/third_party/WebKit',
      '-o',
      'src/build/util/LASTCHANGE.blink'
    ],
    'pattern':
      '.',
    'name':
      'lastchange_blink'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/win/gn.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_win'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/mac/gn.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_mac'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket',
      'chromium-gn',
      '-s',
      'src/buildtools/linux64/gn.sha1'
    ],
    'pattern':
      '.',
    'name':
      'gn_linux64'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/win/clang-format.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_win'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/mac/clang-format.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_mac'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket',
      'chromium-clang-format',
      '-s',
      'src/buildtools/linux64/clang-format.sha1'
    ],
    'pattern':
      '.',
    'name':
      'clang_format_linux'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket',
      'chromium-libcpp',
      '-s',
      'src/third_party/libc++-static/libc++.a.sha1'
    ],
    'pattern':
      '.',
    'name':
      'libcpp_mac'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/win64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_win'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=darwin',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/mac64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_mac'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket',
      'chromium-luci',
      '-d',
      'src/tools/luci-go/linux64'
    ],
    'pattern':
      '.',
    'name':
      'luci-go_linux'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--no_auth',
      '--bucket',
      'chromium-eu-strip',
      '-s',
      'src/build/linux/bin/eu-strip.sha1'
    ],
    'pattern':
      '.',
    'name':
      'eu-strip'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--no_auth',
      '--bucket',
      'chromium-drmemory',
      '-s',
      'src/third_party/drmemory/drmemory-windows-sfx.exe.sha1'
    ],
    'pattern':
      '.',
    'name':
      'drmemory'
  },
  {
    'action': [
      'python',
      'src/build/get_syzygy_binaries.py',
      '--output-dir=src/third_party/syzygy/binaries',
      '--revision=da2c31c5634ec66236ab5851a53c4497e2212bfc',
      '--overwrite'
    ],
    'pattern':
      '.',
    'name':
      'syzygy-binaries'
  },
  {
    'action': [
      'python',
      'src/build/get_syzygy_binaries.py',
      '--output-dir=src/third_party/kasko/binaries',
      '--revision=266a18d9209be5ca5c5dcd0620942b82a2d238f3',
      '--resource=kasko.zip',
      '--resource=kasko_symbols.zip',
      '--overwrite'
    ],
    'pattern':
      '.',
    'name':
      'kasko'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=win32',
      '--directory',
      '--recursive',
      '--no_auth',
      '--num_threads=16',
      '--bucket',
      'chromium-apache-win32',
      'src/third_party/apache-win32'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'apache_win32'
  },
  {
    'action': [
      'download_from_google_storage',
      '--no_resume',
      '--platform=linux*',
      '--extract',
      '--no_auth',
      '--bucket',
      'chromium-fonts',
      '-s',
      'src/third_party/blimp_fonts/font_bundle.tar.gz.sha1'
    ],
    'pattern':
      '.',
    'name':
      'blimp_fonts'
  },
  {
    'action': [
      'python',
      'src/third_party/instrumented_libraries/scripts/download_binaries.py'
    ],
    'pattern':
      '\\.sha1',
    'name':
      'instrumented_libraries'
  },
  {
    'action': [
      'python',
      'src/build/android/download_doclava.py'
    ],
    'pattern':
      '.',
    'name':
      'doclava'
  },
  {
    'action': [
      'python',
      'src/build/gyp_chromium',
      '--running-as-hook'
    ],
    'pattern':
      '.',
    'name':
      'gyp'
  }
]

include_rules = [
  '+base',
  '+build',
  '+ipc',
  '+library_loaders',
  '+testing',
  '+third_party/icu/source/common/unicode',
  '+third_party/icu/source/i18n/unicode',
  '+url'
]

recursedeps = [
  [
    'DEPS.chromium',
    'src/third_party/angle'
  ]
]

skip_child_includes = [
  'breakpad',
  'native_client_sdk',
  'out',
  'sdch',
  'skia',
  'testing',
  'v8',
  'win8'
]
