cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

set(project_description_str "Radio message parser library for Albatros")

project(
  radio_message_parser
  VERSION 1.0.0
  DESCRIPTION ${project_description_str}
  LANGUAGES C)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # Данный CMakeLists.txt является головным, необходимо явно определить
  # NEED_EMBEDDED_LWRB для удовлетворения зависимостей сборки
  set(NEED_EMBEDDED_LWRB true)
endif()

add_library(${PROJECT_NAME} STATIC)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -Wpedantic)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

target_sources(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser_api.c
          ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser_state.c
          ${CMAKE_CURRENT_SOURCE_DIR}/radio_message_parser.c)

target_include_directories(${PROJECT_NAME}
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")

if(NEED_EMBEDDED_LWRB)
  add_subdirectory(lwrb)
  target_link_libraries(${PROJECT_NAME} PUBLIC lwrb)
  target_include_directories(
    radio_message_parser
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lwrb/lwrb/src/include/lwrb/")
else()
  # Если переменная NEED_EMBEDDED_LWRB не определена, то пользовательский
  # CMakeLists.txt должен самостоятельно вызвать следующие команды:
  # target_sources(radio_message_parser PRIVATE path_to_lwrb/lwrb.c)
  # target_include_directories(radio_message_parser PUBLIC "${PATH_TO_LWRB}/")
endif()

target_include_directories(radio_message_parser
                           PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")

if(UTEST_ENABLE)
  target_compile_definitions(${PROJECT_NAME} PUBLIC -DrmpTEST_ENABLE=1
                                                    -DLWRB_CHECKING_ENABLE)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
