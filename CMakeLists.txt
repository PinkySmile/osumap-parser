cmake_minimum_required(VERSION 3.19)

project(
	osumap_parser
	LANGUAGES C
	VERSION 0.0.1
)

add_library(
	osumap_parser
	src/osu_map_parser.c
)

target_compile_features(
	osumap_parser
	PRIVATE
	c_std_11
)

target_include_directories(
	osumap_parser
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
