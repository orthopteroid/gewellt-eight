CMAKE_MINIMUM_REQUIRED( VERSION 3.10 )

SET( CMAKE_CXX_STANDARD 11 )

SET( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
IF( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    SET( CMAKE_EXECUTABLE_SUFFIX_CXX "-debug" )
ENDIF()

SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Wall -fno-exceptions" )
SET( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g" )
SET( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3" )

#########
PROJECT( gewellt_eight )

ADD_EXECUTABLE( gewellt_eight main.cpp )

find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})

target_link_libraries(gewellt_eight ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} ${FREETYPE_LIBRARIES})

#########
PROJECT( gewellt_eight_test )

ADD_EXECUTABLE( gewellt_eight_test test.cpp )

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS})

target_link_libraries(gewellt_eight_test ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
