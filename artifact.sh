#!/usr/bin/env bash

export ARTIFACT_ROOT=$HOME/code/cc-eval
mkdir -p "$ARTIFACT_ROOT"
cd "$ARTIFACT_ROOT"

git clone https://github.com/xianyi/OpenBLAS.git cc-openblas
cd cc-openblas
mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ARTIFACT_ROOT/cc-install" \
  ..
make -j$(nproc)
make install

cd "$ARTIFACT_ROOT"
git clone https://github.com/ginsbach/llvm.git cc-llvm
cd cc-llvm/tools
git checkout linearalgebra

git clone https://github.com/ginsbach/clang.git
cd clang
git checkout research

cd "$ARTIFACT_ROOT/cc-llvm"
mkdir build
cd build

if [ $(command -v xcrun) ]; then
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DCMAKE_INSTALL_PREFIX="$ARTIFACT_ROOT/cc-install" \
    -DOpenBLAS_DIR="$ARTIFACT_ROOT/cc-install/share/cmake/OpenBLAS" \
    -DDEFAULT_SYSROOT=$(xcrun --sdk macosx --show-sdk-path) \
    ..
else
  cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DCMAKE_INSTALL_PREFIX="$ARTIFACT_ROOT/cc-install" \
    -DOpenBLAS_DIR="$ARTIFACT_ROOT/cc-install/share/cmake/OpenBLAS" \
    ..
fi

make -j$(nproc)
make install

cd "$ARTIFACT_ROOT"
mv "$HOME/SNU_NPB-1.0.3.tar.gz" .
tar xvf SNU_NPB-1.0.3.tar.gz

cd "$ARTIFACT_ROOT/SNU_NPB-1.0.3/NPB3.3-SER-C/"
make \
  CC="$ARTIFACT_ROOT/cc-install/bin/clang" \
  CFLAGS="-O2 -fno-vectorize -fno-slp-vectorize -fno-unroll-loops" \
  CLINKFLAGS="-L$ARTIFACT_ROOT/lib -lspmv_csr_naive" \
  CLASS=A \
  -B cg

LD_LIBRARY_PATH="$ARTIFACT_ROOT/cc-install/lib" ./bin/cg.A.x
