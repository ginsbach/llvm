add_llvm_library(LLVMIDLSolver
  BackendSpecializations.cpp
  BackendIncomingValue.cpp
  BackendSameSets.cpp
  BackendOrdering.cpp
  BackendDominate.cpp
  BackendCollect.cpp
  BackendSingle.cpp
  BackendEdge.cpp
  FunctionWrap.cpp
  Solution.cpp
  Solver.cpp
  DEPENDS intrinsics_gen
)
