add_llvm_library(LLVMIDLParser
  ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
  DEPENDS intrinsics_gen
          idl_parser
)

set(CMAKE_CXX_STANDARD 17)

add_executable(idl_parser
  Parser.cc
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
  COMMAND idl_parser ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf
                     ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl
              | pypy ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
                   > ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf
          ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl
          ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
          idl_parser
  COMMENT "Generate idiom specifications C++ file"
)
