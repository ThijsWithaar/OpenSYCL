find_package(fmt CONFIG REQUIRED)

add_executable(acpp-cl acpp-cl.cpp)

target_compile_definitions(acpp-cl PRIVATE -DHIPSYCL_TOOL_COMPONENT)

target_compile_features(acpp-cl PUBLIC cxx_std_20)

target_include_directories(acpp-cl PRIVATE 
    ${HIPSYCL_SOURCE_DIR}
    ${HIPSYCL_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)

target_link_libraries(acpp-cl PRIVATE acpp-rt fmt::fmt)

# Make sure that acpp-cl uses compatible sanitizer flags for sanitized runtime builds
target_link_libraries(acpp-cl PRIVATE ${ACPP_RT_SANITIZE_FLAGS})
target_compile_options(acpp-cl PRIVATE ${ACPP_RT_SANITIZE_FLAGS})
set_target_properties(acpp-cl PROPERTIES INSTALL_RPATH ${base}/../lib/)

install(TARGETS acpp-cl COMPONENT ACPP DESTINATION bin)
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink acpp-cl syclcc)")
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink acpp-cl syclcc-clang)")
