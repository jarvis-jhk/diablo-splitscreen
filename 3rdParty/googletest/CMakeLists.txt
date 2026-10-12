include(functions/FetchContent_ExcludeFromAll_backport)

FetchContent_Declare_ExcludeFromAll(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH MD5=7e11f6cfcf6498324ac82d567dcb891e
)

set(INSTALL_GTEST OFF)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable_ExcludeFromAll(googletest)

# libc++ container annotations must agree across the test executable and GoogleTest.
# Otherwise unsanitized template instantiations can leave live vector entries poisoned,
# causing LeakSanitizer to miss their reachable allocations in the shared backend.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  genex_for_option(ASAN)
  genex_for_option(UBSAN)
  foreach(test_library gtest gtest_main gmock gmock_main)
    target_compile_options(${test_library} PRIVATE
      "$<${ASAN_GENEX}:-fsanitize=address;-fsanitize-recover=address>"
      $<${UBSAN_GENEX}:-fsanitize=undefined>)
    target_link_libraries(${test_library} PRIVATE
      $<${ASAN_GENEX}:-fsanitize=address>
      $<${UBSAN_GENEX}:-fsanitize=undefined>)
    if(TSAN)
      target_compile_options(${test_library} PRIVATE -fsanitize=thread)
      target_link_libraries(${test_library} PRIVATE -fsanitize=thread)
    endif()
  endforeach()
endif()
