cmake_minimum_required(VERSION 3.19)
project(JackerTracker)
find_package( OpenCV REQUIRED )

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(swresample_LIBRARY swresample)

add_subdirectory(lib/fftw)

add_executable(${PROJECT_NAME}
	Main.cpp
	Main.h
	TrackingWindow.cpp
	TrackingWindow.h
	States.cpp
	States.h
	StatesUtils.cpp
	StatesUtils.h
	VideoSource.cpp
	VideoSource.h
	Trackers.cpp
	Trackers.h
)

target_include_directories(${PROJECT_NAME} PUBLIC
	${OpenCV_INCLUDE_DIRS} 
	lib/ffmpeg/include
	lib/miniaudio
	lib/spectrogram-opencv/src
	lib/fftw/api
)

list(APPEND FFMPEG_LIBS
	${AVCODEC_LIBRARY}
	${AVFORMAT_LIBRARY}
	${AVUTIL_LIBRARY}
	${swresample_LIBRARY}
)

target_link_libraries(
	${PROJECT_NAME} ${OpenCV_LIBS} ${FFMPEG_LIBS} fftw3 ${TORCH_LIBRARIES}
)

set(MY_PATH "PATH=${CMAKE_CURRENT_SOURCE_DIR}/lib/ffmpeg/bin;${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv/Build/bin/Debug;C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.5/bin;${CMAKE_CURRENT_SOURCE_DIR}/lib/fftw")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}")
