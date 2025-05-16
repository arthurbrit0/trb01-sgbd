cmake_minimum_required(VERSION 3.10)
project(YggdrasilVinumIndex)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Inclui o diretório de headers
include_directories(${CMAKE_SOURCE_DIR}/include)

# Coleta todos os .cpp em src
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# Executável final
add_executable(IndexApp ${SOURCES})
