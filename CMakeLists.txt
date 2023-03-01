cmake_minimum_required(VERSION 2.5)
project(HCNL)

# HCNL最终编译为so动态库，设置动态库的路径
set(LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" -g -std=c++11)

# 定义参与编译的源代码文件
aux_source_directory(. SRC_LIST)
# 编译生成动态库hcnl
add_library(HCNL SHARED ${SRC_LIST})
