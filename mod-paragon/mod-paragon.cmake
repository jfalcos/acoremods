# mod-paragon — per-module .cmake hook. Registers the module's unit tests
# into AC's `unit_tests` target via the global properties read by
# src/test/CMakeLists.txt (same pattern as mod-terror-zones). Tests only
# compile with -DBUILD_TESTING=ON; the module-source build is unaffected.

get_property(_ACORE_MODULE_TEST_SOURCES GLOBAL PROPERTY ACORE_MODULE_TEST_SOURCES)
list(APPEND _ACORE_MODULE_TEST_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/tests/ParagonPerksTests.cpp")
set_property(GLOBAL PROPERTY ACORE_MODULE_TEST_SOURCES
    "${_ACORE_MODULE_TEST_SOURCES}")

get_property(_ACORE_MODULE_TEST_INCLUDES GLOBAL PROPERTY ACORE_MODULE_TEST_INCLUDES)
list(APPEND _ACORE_MODULE_TEST_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/src")
set_property(GLOBAL PROPERTY ACORE_MODULE_TEST_INCLUDES
    "${_ACORE_MODULE_TEST_INCLUDES}")
