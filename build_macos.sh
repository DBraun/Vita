if [ -z "$PYTHONMAJOR" ]; then
  echo "Build failed. You must set the environment variable PYTHONMAJOR to a value such as 3.11"
  exit 1
fi

if [ -z "$pythonLocation" ]; then
  echo "Build failed. You must set the environment variable pythonLocation to a value such as /Library/Frameworks/Python.framework/Versions/3.11"
  exit 1
fi

# if [[ $(uname -m) == 'arm64' ]]; then
#     export ARCHS="arm64"
#     export CFLAGS="-arch arm64"
#     export ARCHFLAGS="-arch arm64"
# else
#     export ARCHS="x86_64"
#     export CFLAGS="-arch x86_64"
#     export ARCHFLAGS="-arch x86_64"
# fi

# build Nanobind
cd third_party/nanobind
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
cd ../..

# build macOS release
xcodebuild ONLY_ACTIVE_ARCH=NO -configuration Release -project headless/builds/osx/Vita.xcodeproj/ CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED="NO" CODE_SIGN_ENTITLEMENTS="" CODE_SIGNING_ALLOWED="NO"
mv headless/builds/osx/build/Release/vita.so.dylib headless/builds/osx/build/Release/vita.so

rm tests/vita.so
cp headless/builds/osx/build/Release/vita.so tests/vita.so

# # To make a wheel locally:
# pip install setuptools wheel build delocate
# python3 -m build --wheel
# delocate-listdeps dist/vita-0.0.1-cp311-cp311-macosx_10_15_universal2.whl 
# delocate-wheel --require-archs x86_64 -w repaired_wheel dist/vita-0.0.1-cp311-cp311-macosx_10_15_universal2.whl
# pip install repaired_wheel/vita-0.0.1-cp311-cp311-macosx_10_15_universal2.whl
# cd tests
# python -m pytest -s .