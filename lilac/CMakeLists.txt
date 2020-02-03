
set(CMAKE_CXX_STANDARD 17)
add_executable(lilac_parser
    Parser.cc
)

add_library(gemm_naive SHARED
    gemm_naive.cc
)

add_library(spmv_csr_naive SHARED
    spmv_csr_naive.cc
)

add_library(spmv_jds_naive SHARED
    spmv_jds_naive.cc
)

add_library(gemm_openblas SHARED
    gemm_openblas.cc
)

target_link_libraries(gemm_openblas openblas)

ADD_CUSTOM_TARGET("lilac-idl"
                  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
                  DEPENDS spmv_jds_naive spmv_jds_naive gemm_naive gemm_openblas)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
                   COMMAND cat gemm.idl spmv_csr.idl spmv_jds.idl > Idioms.idl
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Concatenate idioms")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_naive.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_mkl.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_cuda.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generate spmv_csr harnesses")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_naive.cc ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generate spmv_jds harnesses")

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gemm_naive.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm_openblas.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm_cuda.cc ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
                   COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac
                   DEPENDS lilac_parser ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac 
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generate mm harnesses")
