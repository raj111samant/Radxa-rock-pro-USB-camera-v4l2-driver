#####################################
cmake_minimum_required (VERSION 2.8)
set(CMAKE_MODULE_PATH "/usr/local/lib/cmake/${CMAKE_MODULE_PATH}")
project (test)
find_package( OpenCV REQUIRED )
link_libraries( main /usr/local/lib)
add_executable (picture picture.cpp)
target_link_libraries( picture ${OpenCV_LIBS})

