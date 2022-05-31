cmake_minimum_required(VERSION 3.10)
project (test)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

set(LIBS ${LIBS} Qt5::Widgets)

FILE(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/../src/*.cpp")
FILE(GLOB_RECURSE SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.cpp")
set(HEADER_DIRS "${PROJECT_SOURCE_DIR}/../src")

message("header" "${HEADER_DIRS}")
message("source" "${SOURCE_FILES}")
message("${PROJECT_SOURCE_DIR}")

include_directories(include ${HEADER_DIRS})
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS}  pthread -ldl -lstdc++fs)
target_compile_options(${PROJECT_NAME} PUBLIC -O0 -g -Wall)
