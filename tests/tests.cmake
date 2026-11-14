include_guard(GLOBAL)

# QtTest suite for Taiga v2 (personal fork). Enabled with -DTAIGA_BUILD_TESTS=ON.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Sql Widgets Test)

enable_testing()

set(TAIGA_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Core sources are compiled straight into the `taiga` executable upstream, so mirror them
# (minus main.cpp) into a static library that tests can link. Only .cpp files are taken;
# headers are found through the include path and resources come from taiga-resources.
get_target_property(_taiga_sources taiga SOURCES)
set(_core_sources)
foreach(_src IN LISTS _taiga_sources)
  if(_src MATCHES "\\.cpp$" AND NOT _src STREQUAL "main.cpp" AND NOT IS_ABSOLUTE "${_src}")
    list(APPEND _core_sources ${TAIGA_SRC_DIR}/${_src})
  endif()
endforeach()

add_library(taiga-core-tests STATIC ${_core_sources})
set_target_properties(taiga-core-tests PROPERTIES AUTOMOC ON AUTORCC ON)
target_include_directories(taiga-core-tests PUBLIC ${TAIGA_SRC_DIR})
target_compile_definitions(taiga-core-tests PUBLIC $<TARGET_PROPERTY:taiga,COMPILE_DEFINITIONS>)
target_link_libraries(taiga-core-tests PUBLIC
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Widgets
  taiga-config taiga-deps taiga-gui taiga-resources
)

# Tests build into their own directory so the portable data path (<exe dir>/data)
# never touches bin/ or a real user's data.
set(TAIGA_TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests/bin)

function(taiga_add_test name)
  add_executable(${name} ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${name}.cpp)
  set_target_properties(${name} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    RUNTIME_OUTPUT_DIRECTORY ${TAIGA_TEST_OUTPUT_DIR}
  )
  if(MSVC)
    target_link_libraries(${name} PRIVATE taiga-core-tests Qt6::Test)
  else()
    # GNU-style linkers resolve static libraries in order, and core and gui reference each other.
    target_link_libraries(${name} PRIVATE "$<LINK_GROUP:RESCAN,taiga-core-tests,taiga-gui>" Qt6::Test)
  endif()
  add_test(NAME ${name} COMMAND ${name})
  # All tests share the data directory next to the executables.
  set_tests_properties(${name} PROPERTIES RUN_SERIAL TRUE ENVIRONMENT_MODIFICATION
    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_QPA_PLATFORM=set:offscreen"
  )
endfunction()

taiga_add_test(tst_relations)
target_compile_definitions(tst_relations PRIVATE
  TAIGA_RELATIONS_FILE="${CMAKE_SOURCE_DIR}/deps/anime-relations/anime-relations.txt")

taiga_add_test(tst_media_dialog)
taiga_add_test(tst_anime_db)
taiga_add_test(tst_form_encoding)
taiga_add_test(tst_queue_auth)
taiga_add_test(tst_settings_accounts)
taiga_add_test(tst_auth_service_id)
taiga_add_test(tst_queue_order)
taiga_add_test(tst_detection_choice)
taiga_add_test(tst_episode_number)
taiga_add_test(tst_discord_ipc)
taiga_add_test(tst_discord_presence)
taiga_add_test(tst_settings_discord)
