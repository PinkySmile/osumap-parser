cmake_minimum_required(VERSION 3.12)
project(osumap_parser C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_library(
	osumap_parser
	include/osu_map_parser.h
	src/osu_map_parser.c
)
