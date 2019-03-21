add_llvm_library(LLVMIDLParser
  ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp

  DEPENDS
  intrinsics_gen
)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BNFParser
                   COMMAND ghc ${CMAKE_CURRENT_SOURCE_DIR}/stage1.hs -hidir ${CMAKE_CURRENT_BINARY_DIR} -odir ${CMAKE_CURRENT_BINARY_DIR} -i${CMAKE_CURRENT_SOURCE_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/BNFParser
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/stage1.hs ${CMAKE_CURRENT_SOURCE_DIR}/BNFLower.hs ${CMAKE_CURRENT_SOURCE_DIR}/BNFOptimize.hs ${CMAKE_CURRENT_SOURCE_DIR}/BNFParser.hs ${CMAKE_CURRENT_SOURCE_DIR}/BNFPrefix.hs ${CMAKE_CURRENT_SOURCE_DIR}/GraphCalculations.hs
                   COMMENT "Generate IDL grammar parser")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/GeneratedParser.hs
                   COMMAND ${CMAKE_CURRENT_BINARY_DIR}/BNFParser < ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf > ${CMAKE_CURRENT_BINARY_DIR}/GeneratedParser.hs
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/BNFParser ${CMAKE_CURRENT_SOURCE_DIR}/IDL.bnf
                   COMMENT "Parse IDL grammar")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IDLParser
                   COMMAND ghc ${CMAKE_CURRENT_SOURCE_DIR}/stage2.hs -hidir ${CMAKE_CURRENT_BINARY_DIR} -odir ${CMAKE_CURRENT_BINARY_DIR} -i${CMAKE_CURRENT_BINARY_DIR} -i${CMAKE_CURRENT_SOURCE_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/IDLParser
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/stage2.hs ${CMAKE_CURRENT_SOURCE_DIR}/IDLProcessing.hs ${CMAKE_CURRENT_BINARY_DIR}/GeneratedParser.hs
                   COMMENT "Generate IDL program parser")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
                   COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/Preprocessing.idl ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl ${CMAKE_BINARY_DIR}/lilac/Idioms.idl | ${CMAKE_CURRENT_BINARY_DIR}/IDLParser | pypy ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py > ${CMAKE_CURRENT_BINARY_DIR}/IdiomSpecifications.cpp
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/IDLParser ${CMAKE_CURRENT_SOURCE_DIR}/Preprocessing.idl ${CMAKE_CURRENT_SOURCE_DIR}/Idioms.idl ${CMAKE_BINARY_DIR}/lilac/Idioms.idl ${CMAKE_CURRENT_SOURCE_DIR}/stage3.py
                   DEPENDS lilac-idl
                   COMMENT "Generate idiom specifications C++ file")
