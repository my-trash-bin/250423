cmake_minimum_required(VERSION 3.10)

project(jsonc)

add_executable(${PROJECT_NAME} main.cpp ../src/jsonc.c)
include_directories(../include)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
