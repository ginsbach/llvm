add_llvm_library(LLVMIDLPasses
  CustomPassFlangfix.cpp
  CustomPassPreprocess.cpp
  CustomPassReplace.cpp
  DEPENDS intrinsics_gen
)
