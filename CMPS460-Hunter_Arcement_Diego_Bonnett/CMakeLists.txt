cmake_minimum_required(VERSION 3.26)
project(CMPS460_Hunter_Arcement_Diego_Bonnett C)

set(CMAKE_C_STANDARD 23)

include_directories("${PROJECT_SOURCE_DIR}/")

set(Sources main.c sqlite3.c)
set(Headers sqlite3.h
        sha256.h
        sha256.c)

add_executable(HD_Corp main.c ${Sources} ${Headers})