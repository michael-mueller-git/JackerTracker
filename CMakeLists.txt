cmake_minimum_required(VERSION 3.19)
project(JackerTracker LANGUAGES CUDA CXX)

find_package( OpenCV REQUIRED )
find_package(CUDA REQUIRED)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(swresample_LIBRARY swresample)

add_subdirectory(lib/fftw)

#Guid start
	add_library(
		crossguid STATIC
		lib/crossguid/src/guid.cpp
	)
	
	set_target_properties(crossguid
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
	)
	
	target_include_directories(crossguid PUBLIC lib/crossguid/include)

	if(WIN32)
		target_compile_definitions(crossguid PRIVATE GUID_WINDOWS)
	elseif(APPLE)
		find_library(CFLIB CoreFoundation)
		target_link_libraries(crossguid ${CFLIB})
		target_compile_definitions(crossguid PRIVATE GUID_CFUUID)
	else()
		find_package(Libuuid REQUIRED)
		if (NOT LIBUUID_FOUND)
			message(FATAL_ERROR
				"You might need to run 'sudo apt-get install uuid-dev' or similar")
		endif()
		target_include_directories(crossguid PRIVATE ${LIBUUID_INCLUDE_DIR})
		target_link_libraries(crossguid ${LIBUUID_LIBRARY})
		target_compile_definitions(crossguid PRIVATE GUID_LIBUUID)
	endif()
#Guid end

file(GLOB_RECURSE sources
     "src/*.h"
     "src/*.cpp"
)
list(APPEND sources "src/Reader/nv12_to_rgb.cu")

add_executable(${PROJECT_NAME} ${sources})

set_source_files_properties(Reader/nv12_to_rgb.cu PROPERTIES LANGUAGE CUDA)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${sources})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
	${OpenCV_INCLUDE_DIRS}
	lib/ffmpeg/include
	lib/miniaudio
	lib/fftw/api
	lib/json
	lib/magic_enum/include
	src
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

list(APPEND FFMPEG_LIBS
	${AVCODEC_LIBRARY}
	${AVFORMAT_LIBRARY}
	${AVUTIL_LIBRARY}
	${swresample_LIBRARY}
)

target_link_libraries(${PROJECT_NAME}
	${OpenCV_LIBS}
	${FFMPEG_LIBS}
	fftw3
	${TORCH_LIBRARIES}
	cufft
	cuda
	nvcuvid
	crossguid
)

set(MY_PATH "PATH=${CMAKE_CURRENT_SOURCE_DIR}/lib/ffmpeg/bin;${CMAKE_CURRENT_SOURCE_DIR}/lib/opencv/Build/bin/Debug;C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.5/bin;${CMAKE_CURRENT_SOURCE_DIR}/lib/fftw")
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${MY_PATH}")
