# Google Test
add_subdirectory("ext_libs/gtest-1.7.0")

# Test include directories
include_directories("${gtest_SOURCE_DIR}/include" "${gtest_SOURCE_DIR}" "./include")

# Google test executables
add_executable(fsg_check fsg_check.cpp)
add_dependencies(fsg_check googletest)
m_link(fsg_check gtest bcobjs divsufsort)

add_executable(encoder_tests encoder_tests.cpp)
add_dependencies(encoder_tests googletest)
m_link(encoder_tests bcobjs gtest)

add_executable(api_test api_test.cpp)
add_dependencies(api_test googletest)
m_link(api_test bcobjs gtest divsufsort)

add_executable(graph_cacher graph_cacher.cpp)
add_dependencies(graph_cacher googletest)
m_link(graph_cacher bcobjs gtest gtest_main divsufsort)

# Register them as tests
add_test(Encoder encoder_tests)
add_test(Cacher graph_cacher test_file test_model)
set_tests_properties(Cacher PROPERTIES REQUIRED_FILES "test_file;test_model.tmod")
add_test(API api_test test_file)
set_tests_properties(API PROPERTIES REQUIRED_FILES "test_file")

add_executable(model_read_test model_read_test.cpp)
m_link(model_read_test bcobjs divsufsort)

add_executable(solution_getter_test solution_getter_test.cpp)
m_link(solution_getter_test bcobjs gtest divsufsort)

add_executable(fast_solution_integrator fast_solution_integrator.cpp)
m_link(fast_solution_integrator bcobjs gtest divsufsort)

add_executable(cost_model_id cost_model_id.cpp)
m_link(cost_model_id bcobjs gtest divsufsort)

add_executable(swapper_test swapper_test.cpp)
m_link(swapper_test bcobjs gtest divsufsort)
