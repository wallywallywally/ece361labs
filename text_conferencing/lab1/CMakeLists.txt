cmake_minimum_required(VERSION 3.30)
project(text_conferencing C)

set(CMAKE_C_STANDARD 11)

add_executable(text_conferencing server.c
        packet.h
        session.h
        user.h)
