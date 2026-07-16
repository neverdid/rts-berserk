set(ASHEN_CORE_MODULE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/../unreal/AshenDominion/Source/AshenCore
)

add_library(ashen_core STATIC
  ${ASHEN_CORE_MODULE_DIR}/Private/Catalog.cpp
  ${ASHEN_CORE_MODULE_DIR}/Private/Simulation.cpp
)

target_include_directories(ashen_core
  PUBLIC
    ${ASHEN_CORE_MODULE_DIR}/Public
)

target_compile_features(ashen_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(ashen_core PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(ashen_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(ashen_headless tools/headless_main.cpp)
target_link_libraries(ashen_headless PRIVATE ashen_core)

if(ASHEN_BUILD_TESTS AND BUILD_TESTING)
  add_executable(ashen_core_tests tests/SimulationTests.cpp)
  target_link_libraries(ashen_core_tests PRIVATE ashen_core)
  add_test(NAME ashen_core_tests COMMAND ashen_core_tests)

  add_executable(ashen_parity_runner tools/parity_runner.cpp)
  target_link_libraries(ashen_parity_runner PRIVATE ashen_core)
  add_test(NAME ashen_parity_catalog COMMAND ashen_parity_runner --catalog)

  if(MINGW)
    get_filename_component(ASHEN_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(ashen_core_tests ashen_parity_catalog PROPERTIES
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${ASHEN_MINGW_BIN_DIR}"
    )
  endif()
endif()
