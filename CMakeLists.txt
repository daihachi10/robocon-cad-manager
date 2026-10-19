cmake_minimum_required(VERSION 3.24)
project(RoboconCadManager VERSION 0.1.7 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
configure_file(src/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
add_executable(RoboconCadManager WIN32
  "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
  src/main.cpp src/app.cpp src/config.cpp src/process.cpp src/git.cpp
  src/json.cpp src/cad.cpp src/ui.cpp src/app_actions.cpp src/solidworks_monitor.cpp src/update.cpp)
target_include_directories(RoboconCadManager PRIVATE src)
target_compile_definitions(RoboconCadManager PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX RC_VERSION=L\"${PROJECT_VERSION}\")
target_link_libraries(RoboconCadManager PRIVATE comctl32 shell32 shlwapi ole32 oleaut32 winhttp bcrypt)
if(MSVC)
  target_compile_options(RoboconCadManager PRIVATE /W4 /permissive- /utf-8)
  target_compile_options(RoboconCadManager PRIVATE $<$<CONFIG:Release>:/O2 /GL /Gy>)
  target_link_options(RoboconCadManager PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
  set_property(TARGET RoboconCadManager PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
install(TARGETS RoboconCadManager RUNTIME DESTINATION .)

add_executable(RoboconCadManagerMinimal WIN32 src/minimal_main.cpp)
target_compile_definitions(RoboconCadManagerMinimal PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(RoboconCadManagerMinimal PRIVATE user32)
if(MSVC)
  target_compile_options(RoboconCadManagerMinimal PRIVATE /W4 /permissive- /utf-8 $<$<CONFIG:Release>:/O2 /GL>)
  target_link_options(RoboconCadManagerMinimal PRIVATE $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
  set_property(TARGET RoboconCadManagerMinimal PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
install(TARGETS RoboconCadManagerMinimal RUNTIME DESTINATION .)

