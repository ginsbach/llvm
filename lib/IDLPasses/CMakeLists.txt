include_directories(${CMAKE_BINARY_DIR}/lilac)

add_llvm_library(LLVMIDLPasses
  CustomPassFlangfix.cpp
  CustomPassPreprocess.cpp
  CustomPassReplace.cpp
  DEPENDS intrinsics_gen
          lilac_target
)
