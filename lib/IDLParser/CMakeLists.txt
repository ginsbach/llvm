add_llvm_library(LLVMIDLParser
  IdiomSpecifications.cpp

  DEPENDS
  idl_parser
  intrinsics_gen
)

set(CMAKE_CXX_STANDARD 17)
add_executable(idl_parser
    Parser.cc
)

add_custom_command(OUTPUT  Idioms.idl
                   COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl ${CMAKE_BINARY_DIR}/lilac/Idioms.idl > Idioms.idl
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl  ${CMAKE_BINARY_DIR}/lilac/Idioms.idl
                   DEPENDS lilac-idl
                   COMMENT "Merge idiom specification files.")

add_custom_command(OUTPUT  IdiomSpecifications.cpp
                   COMMAND idl_parser ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf Idioms.idl | pypy ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py > IdiomSpecifications.cpp
                   DEPENDS idl_parser ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf Idioms.idl ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
                   COMMENT "Generate idiom specifications C++ file")
