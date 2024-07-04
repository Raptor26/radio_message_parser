cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# Unit тестирование написано с использованием библиотеки <Check> Поэтому
# необходимо выполнить поиск установленной библиотеки <Check> средствами cmake
include(FindPkgConfig)

# Take care about check.pc settings
pkg_search_module(CHECK Check)

if(CHECK_LIBRARIES)
  add_executable(test_radio_message_parser test_main.c)

  target_link_libraries(test_radio_message_parser PRIVATE radio_message_parser
                                                          ${CHECK_LIBRARIES})

  add_test(NAME test_radio_message_parser COMMAND test_radio_message_parser)
  message(STATUS "Build and registered <test_radio_message_parser> in CTest")
else()
  # https://libcheck.github.io/check/ for windows available in msys2
  # https://packages.msys2.org/package/mingw-w64-x86_64-check
  message(WARNING "Can't find check framework for build unit testing")
endif()
