cmake_minimum_required(VERSION 3.1)
project( client-image )

find_package(OpenCV 4 REQUIRED)
find_package(Boost COMPONENTS thread REQUIRED)
include_directories(“./include”)
include_directories(${Boost_INCLUDE_DIR})

add_executable( client client_asio.cpp )
target_link_libraries( client ${OpenCV_LIBS} ${Boost_LIBRARIES} )

add_executable( server server_asio.cpp )
target_link_libraries( server ${OpenCV_LIBS} ${Boost_LIBRARIES} Boost::thread pthread)
