add_library(gemm_naive SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/gemm_naive.cc
)

add_library(spmv_csr_naive SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_naive.cc
)

add_library(spmv_jds_naive SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_naive.cc
)

add_library(gemm_openblas SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/gemm_openblas.cc
)

target_link_libraries(gemm_openblas openblas)

set(CMAKE_CXX_STANDARD 17)
add_executable(lilac_parser
    Parser.cc
)

add_custom_target(lilac-idl
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
                          spmv_jds_naive
                          spmv_jds_naive
                          gemm_naive
                          gemm_openblas)

add_custom_target(lilac-replacer-code
                  DEPENDS CustomPassReplacerLiLACGenerated.h
)
add_custom_command(OUTPUT  CustomPassReplacerLiLACGenerated.h
                   COMMAND cat gemm_replacer.h spmv_csr_replacer.h spmv_jds_replacer.h > CustomPassReplacerLiLACGenerated.h
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gemm_replacer.h
                           ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_replacer.h
                           ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_replacer.h
)
add_custom_command(OUTPUT  Idioms.idl
                   COMMAND cat gemm.idl spmv_csr.idl spmv_jds.idl > Idioms.idl
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
                           ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
                           ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
                   COMMENT "Concatenate idioms")

add_custom_command(OUTPUT  spmv_csr_replacer.h spmv_csr_naive.cc spmv_csr_mkl.cc spmv_csr_cuda.cc spmv_csr.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   COMMENT "Generate spmv_csr harnesses")

add_custom_command(OUTPUT  spmv_jds_replacer.h spmv_jds_naive.cc spmv_jds.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
                   COMMENT "Generate spmv_jds harnesses")

add_custom_command(OUTPUT  gemm_replacer.h gemm_naive.cc gemm_openblas.cc gemm_cuda.cc gemm.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac 
                   COMMENT "Generate mm harnesses")
