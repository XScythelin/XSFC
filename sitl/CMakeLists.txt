cmake_minimum_required(VERSION 3.16)
project(xsfc_p4_sitl LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(xsfc-p4-sitl main.cpp
  "${ROOT}/lib/AHRS/src/Mahony.cpp"
  "${ROOT}/lib/Xsfc/src/Control/Pid.cpp"
  "${ROOT}/lib/Xsfc/src/Utils/Filter.cpp")
target_include_directories(xsfc-p4-sitl PRIVATE
  "${ROOT}/lib/AHRS/src" "${ROOT}/lib/Xsfc/src"
  "${ROOT}/test/imu/stubs")
enable_testing()
foreach(RATE 2000 4000 8000)
  add_test(NAME p4_${RATE} COMMAND xsfc-p4-sitl --rate ${RATE})
endforeach()
