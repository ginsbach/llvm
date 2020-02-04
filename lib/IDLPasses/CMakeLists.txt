include_directories(${CMAKE_BINARY_DIR}/lilac)
add_llvm_library(LLVMIDLPasses

  CustomPassFlangfix.cpp
  CustomPassPreprocess.cpp
  CustomPassReplace.cpp

  DEPENDS
  lilac-replacer-code
  intrinsics_gen
)
