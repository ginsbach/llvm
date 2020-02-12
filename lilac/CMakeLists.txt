add_custom_target(lilac_target
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
          ${CMAKE_CURRENT_BINARY_DIR}/CustomPassReplacerLiLACGenerated.h
          gemm_naive
          spmv_csr_naive
          spmv_jds_naive
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
  COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
              ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
              ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
            > ${CMAKE_CURRENT_BINARY_DIR}/Idioms.idl
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
  COMMENT "Concatenate idioms"
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/CustomPassReplacerLiLACGenerated.h
  COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/gemm_replacer.h
              ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_replacer.h
              ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_replacer.h
            > ${CMAKE_CURRENT_BINARY_DIR}/CustomPassReplacerLiLACGenerated.h
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gemm_replacer.h
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_replacer.h
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_replacer.h
  COMMENT "Generate C++ code to complete CustomPassReplace"
)

add_library(gemm_naive SHARED
  ${CMAKE_CURRENT_BINARY_DIR}/gemm_naive.cc
)

add_library(spmv_csr_naive SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_naive.cc
)

add_library(spmv_jds_naive SHARED
    ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_naive.cc
)

set(CMAKE_CXX_STANDARD 17)

add_executable(lilac_parser
  Parser.cc
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/gemm.idl
          ${CMAKE_CURRENT_BINARY_DIR}/gemm_replacer.h
          ${CMAKE_CURRENT_BINARY_DIR}/gemm_naive.cc
          ${CMAKE_CURRENT_BINARY_DIR}/gemm_openblas.cc
          ${CMAKE_CURRENT_BINARY_DIR}/gemm_cuda.cc
  COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gemm.lilac
          lilac_parser
  COMMENT "Process LiLAC specification for GEMM"
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr.idl
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_replacer.h
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_naive.cc
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_mkl.cc
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_csr_cuda.cc
  COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/spmv_csr.lilac
          lilac_parser
  COMMENT "Process LiLAC specification for SPMV_CSR"
)

add_custom_command(
  OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds.idl
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_replacer.h
          ${CMAKE_CURRENT_BINARY_DIR}/spmv_jds_naive.cc
  COMMAND lilac_parser < ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/spmv_jds.lilac
          lilac_parser
  COMMENT "Process LiLAC specification for SPMV_JDS"
)
