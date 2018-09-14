cmake_minimum_required(VERSION 3.2)
project(gx2_texture C)
include("${WUT_ROOT}/share/wut.cmake" REQUIRED)

add_executable(gx2_texture
   main.c)

target_link_libraries(gx2_texture
   whb
   gfd
   gx2
   proc_ui
   nsysnet
   sysapp)

wut_default_malloc(gx2_texture)
wut_create_rpx(gx2_texture.rpx gx2_texture)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/gx2_texture.rpx"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")
