cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# Unit тестирование написано с использованием библиотеки <Check>
# Поэтому необходимо выполнить поиск установленной библиотеки <Check> средствами cmake
INCLUDE(FindPkgConfig)

# Take care about check.pc settings
PKG_SEARCH_MODULE(CHECK Check)

# ------------------------------------------------------------------------------
# Создание исполняемого файла Unit тестов и его регистрация в <CTest>
set(TEST_RADIO_MESSAGE_PARSER_SRC
    test_main.c)

add_executable(test_radio_message_parser ${TEST_RADIO_MESSAGE_PARSER_SRC})

target_link_libraries(test_radio_message_parser PRIVATE
    radio_message_parser
    ${CHECK_LIBRARIES})

add_test(NAME test_radio_message_parser COMMAND test_radio_message_parser)
message(STATUS "Build and registered <test_radio_message_parser> in CTest")
