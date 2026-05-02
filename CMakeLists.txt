cmake_minimum_required(VERSION 3.10)
project(analyser C)

set(CMAKE_C_STANDARD 99)

add_executable(analyser main.c io.c waveform.c)
target_link_libraries(analyser m)