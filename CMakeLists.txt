cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

set(project_description_str "Radio message parser library for Albatros")

project(radio_message_parser
    VERSION 0.1.0
    DESCRIPTION ${project_description_str}
    LANGUAGES C)

set(RADIO_MESSAGE_PARSER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser_state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser.c
    ${CMAKE_SOURCE_DIR}/lwrb/lwrb_ext.c
    ${CMAKE_SOURCE_DIR}/lwrb/lwrb_message_api.c
    ${CMAKE_SOURCE_DIR}/lwrb/lwrb.c)

set_source_files_properties(${RADIO_MESSAGE_PARSER_SRC}
    PROPERTIES
    COMPILE_FLAGS "-Wall -Wextra -Werror -pedantic")
add_library(radio_message_parser STATIC ${RADIO_MESSAGE_PARSER_SRC})
target_link_libraries(radio_message_parser PUBLIC macros_definition)

target_include_directories(radio_message_parser
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/"
    "${CMAKE_SOURCE_DIR}/"
    "${CMAKE_SOURCE_DIR}/lwrb/"
    "${CMAKE_SOURCE_DIR}/PHAL/")

if(UTEST_ENABLE)
    message(STATUS "Build UTest for test_radio_message_parser")
    target_compile_definitions(radio_message_parser PUBLIC
        -DrmpTEST_ENABLE=1
        -DLWRB_CHECKING_ENABLE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
