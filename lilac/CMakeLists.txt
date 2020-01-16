add_executable(lilac_parser
    lilac_parser.cc
)

add_library(spmv_naive SHARED
    spmv_csr_naive.cc
)

add_library(gemm_naive SHARED
    gemm_naive.cc
)

add_library(gemm_openblas SHARED
    gemm_openblas.cc
)

target_link_libraries(gemm_openblas openblas)

ADD_CUSTOM_TARGET("lilac-idl"
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
                   COMMAND cat spmv_csr.idl gemm.idl > Idioms.idl
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Concatenate idioms")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_naive.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_mkl.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_cuda.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generate spmv harnesses")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gemm_naive.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm_openblas.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm_cuda.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac 
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generate mm harnesses")
