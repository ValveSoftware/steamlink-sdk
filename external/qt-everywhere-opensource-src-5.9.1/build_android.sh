
#rm -rf build
mkdir -p build
pushd build

../configure -release -force-debug-info -opensource -confirm-license -qt-harfbuzz -qt-libpng -nomake examples -nomake tests -xplatform android-g++ -nomake tests -nomake examples -android-ndk /home/slouken/android/android-ndk-r10e -android-sdk /home/slouken/android/sdk -android-ndk-host linux-x86_64 -android-toolchain-version 4.9 -skip qttranslations -skip qtwebkit -skip qtserialport -skip qtwebkit-examples -no-warnings-are-errors || exit 1

# Need to bootstrap ninja for the qtwebengine build
mkdir -p qtwebengine/src/3rdparty/ninja
pushd qtwebengine/src/3rdparty/ninja
../../../../../qtwebengine/src/3rdparty/ninja/configure.py --bootstrap
popd

make && make install
