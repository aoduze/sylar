cmake_minimum_required(VERSION 3.31)
project(sylar_study)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(.idea)
include_directories(sylar)

add_executable(sylar_study
        .idea/.gitignore
        .idea/material_theme_project_new.xml
        .idea/modules.xml
        .idea/sylar_study.iml
        .idea/vcs.xml
        log.cpp
        log.h)
