add_llvm_library(LLVMIDLSolver
  BackendSpecializations.cpp
  BackendIncomingValue.cpp
  BackendDominate.cpp
  BackendOrdering.cpp
  BackendSameSets.cpp
  BackendCollect.cpp
  BackendSingle.cpp
  BackendEdge.cpp
  FunctionWrap.cpp
  Solution.cpp
  Solver.cpp
  DEPENDS intrinsics_gen
)
