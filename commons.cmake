# akvirtualcamera, virtual camera for Mac and Windows.
# Copyright (C) 2021  Gonzalo Exequiel Pedone
#
# akvirtualcamera is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# akvirtualcamera is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with akvirtualcamera. If not, see <http://www.gnu.org/licenses/>.
#
# Web-Site: http://webcamoid.github.io/


# 配置版本号，环境检测等

cmake_minimum_required(VERSION 3.5)

# APPLE WIN32 -- 平台描述变量
if (NOT APPLE AND NOT WIN32)
    message(FATAL_ERROR "This driver only works in Mac an Windows. For Linux check 'akvcam' instead.")
endif ()

# 加载CMake内置的模块CheckCXXSourceCompiles
# 该模块检测提供的源代码是否可以编译为 C++ 源文件并链接为可执行文件（因此它必须至少包含一个main() 函数）。
include(CheckCXXSourceCompiles)

set(COMMONS_APPNAME AkVirtualCamera)
# 将变量COMMONS_APPNAME转换成小写字母，并赋值到变量COMMONS_TARGET
string(TOLOWER ${COMMONS_APPNAME} COMMONS_TARGET)

# 设置版本号
set(VER_MAJ 9)
set(VER_MIN 1)
set(VER_PAT 0)
set(VERSION ${VER_MAJ}.${VER_MIN}.${VER_PAT})
set(DAILY_BUILD OFF CACHE BOOL "Mark this as a daily build")

# 添加编译器命令行参数
add_definitions(-DCOMMONS_APPNAME="${COMMONS_APPNAME}"
                -DCOMMONS_TARGET="${COMMONS_TARGET}"
                -DCOMMONS_VER_MAJ="${VER_MAJ}"
                -DCOMMONS_VERSION="${VERSION}"
                -DPREFIX="${PREFIX}")

if (APPLE)
    check_cxx_source_compiles("
    #ifndef __x86_64__
        #error Not x64
    #endif

    int main()
    {
        return 0;
    }" IS_64BITS_TARGET)
elseif (WIN32)
    check_cxx_source_compiles("
    #ifndef _WIN64
        #error Not x64
    #endif

    int main()
    {
        return 0;
    }" IS_64BITS_TARGET)

    add_definitions(-DUNICODE -D_UNICODE)
endif ()

if (IS_64BITS_TARGET)
    set(TARGET_ARCH x64 CACHE INTERNAL "")
else ()
    set(TARGET_ARCH x86 CACHE INTERNAL "")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

if (DAILY_BUILD)
    add_definitions(-DDAILY_BUILD)
endif ()
