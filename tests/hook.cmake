# Local-only test hook. Injected via CMAKE_PROJECT_Taiga_INCLUDE from CMakeUserPresets.json,
# so no tracked upstream file changes. Deferred so that src/ targets exist first
# (add_subdirectory is not allowed in deferred calls, include is). EVAL bakes in the path,
# since deferred arguments are otherwise expanded at call time.
cmake_language(EVAL CODE "
  cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
    CALL include [[${CMAKE_CURRENT_LIST_DIR}/tests.cmake]])
")
