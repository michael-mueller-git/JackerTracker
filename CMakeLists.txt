cmake_minimum_required(VERSION 3.19)
project(JackerTracker)
find_package( OpenCV REQUIRED )

add_executable(${PROJECT_NAME}
	Main.cpp
	Main.h
	TrackingWindow.cpp
	TrackingWindow.h
	States.cpp
	States.h
)

target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS} )

target_link_libraries( ${PROJECT_NAME} ${OpenCV_LIBS} )