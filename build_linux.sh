echo "PYTHONLIBPATH: $PYTHONLIBPATH"
echo "PYTHONINCLUDEPATH: $PYTHONINCLUDEPATH"
echo "LIBDIR: $LIBDIR"

yum install -y \
libsndfile \
libsndfile-devel

echo "Build Nanobind"
cd third_party/nanobind
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON
cmake --build build
cd ../..

make headless_server

echo "build_linux.sh is done!"
