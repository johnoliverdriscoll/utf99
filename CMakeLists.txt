cmake_minimum_required(VERSION 3.22)

project(utf99
  VERSION 0.1.0
  LANGUAGES C
)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(UTF99_BUILD_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(UTF99_USE_SYSTEM_SIO99 "Use installed sio99 through pkg-config" OFF)

if(NOT TARGET sio99)
  if(UTF99_USE_SYSTEM_SIO99)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SIO99 REQUIRED IMPORTED_TARGET GLOBAL sio99)
    add_library(sio99 ALIAS PkgConfig::SIO99)
  else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/sio99/CMakeLists.txt")
      message(FATAL_ERROR
        "The sio99 submodule is missing. Run: "
        "git submodule update --init --recursive"
      )
    endif()
    # Keep dependency options local and honor explicit parent settings.
    function(utf_add_sio99)
      foreach(option SIO99_BUILD_TESTING SIO99_WITH_STDIO SIO99_WITH_HEAP)
        if(NOT DEFINED ${option})
          set(${option} OFF)
        endif()
      endforeach()
      add_subdirectory(src/sio99)
    endfunction()
    utf_add_sio99()
  endif()
endif()

function(utf_target_defaults target)

  set_target_properties(${target}
    PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF
  )

  target_include_directories(${target} PUBLIC include)

  target_compile_options(${target}
    PRIVATE
      "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:"
        "-Werror;"
        "-Wpedantic;"
        "-Wall;"
        "-Wextra;"
        "-Werror=vla;"
        "-Werror=alloca;"
        "-Werror=implicit-function-declaration;"
        "-Wstrict-prototypes;"
        "-Wmissing-prototypes;"
        "-fno-fast-math;"
        "-ffp-contract=off;"
      ">"
  )

endfunction()

add_library(utf99
  src/utf99/utf.c
)
utf_target_defaults(utf99)
target_link_libraries(utf99 PUBLIC sio99)
set_target_properties(utf99
  PROPERTIES
    VERSION "${PROJECT_VERSION}"
    SOVERSION 0
)

include(GNUInstallDirs)

install(
  TARGETS utf99
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  FILES
    include/utf99/utf.h
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/utf99"
)

configure_file(utf99.pc.in utf99.pc @ONLY)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/utf99.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

if(UTF99_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

