add_llvm_library(LLVMCAnDLParser
  IdiomSpecifications.cpp
  DEPENDS intrinsics_gen
          candl_parser
)

set(CMAKE_CXX_STANDARD 17)

add_executable(candl_parser
  Parser.cc
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
  COMMAND candl_parser ${CMAKE_CURRENT_SOURCE_DIR}/CAnDL.bnf
                       ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.candl
                | pypy ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
                     > ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CAnDL.bnf
          ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.candl
          ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
          candl_parser
  COMMENT "Generate idiom specifications C++ file"
)
