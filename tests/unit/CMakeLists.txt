add_executable(test_poly_helpers
    test_poly_helpers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/poly_mul/common/poly_helpers.c
)

target_include_directories(test_poly_helpers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/poly_mul/common
)

add_test(NAME poly_helpers COMMAND test_poly_helpers)
