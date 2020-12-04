set(HDRS
buffer.h
clipboard.h
colors.h
compiler.h
engine.h
fbicon.h
forth.h
keyboard.h
music.h
preprocessor.h
utils.h
    )
	
set(SRCS
buffer.cpp
clipboard.cpp
colors.cpp
compiler.cpp
engine.cpp
fbicon.cpp
keyboard.cpp
main.cpp
music.cpp
preprocessor.cpp
utils.cpp
)


if (WIN32)
set(CMAKE_C_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_CXX_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_C_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi")
endif (WIN32)

# general build definitions
add_definitions(-D_SCL_SECURE_NO_WARNINGS)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

if (WIN32)
add_definitions(-DPDC_DLL_BUILD)
endif (WIN32)
add_definitions(-DPDC_RGB)
add_definitions(-DPDC_FORCE_UTF8)
add_definitions(-DPDC_WIDE)

if (WIN32)
add_executable(forthbyte WIN32 ${HDRS} ${SRCS} forthbyte.rc resource.h)
endif (WIN32)

if (UNIX)
add_executable(forthbyte ${HDRS} ${SRCS} ${JSON})
endif (UNIX)

source_group("Header Files" FILES ${HDRS})
source_group("Source Files" FILES ${SRCS})
source_group("ThirdParty/json" FILES ${JSON})

 target_include_directories(forthbyte
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-rrb/
    ${CMAKE_CURRENT_SOURCE_DIR}/../jtk/
    ${CMAKE_CURRENT_SOURCE_DIR}/../pdcurses/     
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_ttf/
    )	
	
target_link_libraries(forthbyte
    PRIVATE 
    pdcurses
    SDL2
    SDL2main
    SDL2_ttf
    )	
