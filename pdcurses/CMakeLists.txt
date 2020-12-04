set(HDRS
curses
curspriv.h
panel.h
sdl2/pdcsdl.h
)
	
set(SRCS
pdcurses/addch.c
pdcurses/addchstr.c
pdcurses/addstr.c
pdcurses/attr.c
pdcurses/beep.c
pdcurses/bkgd.c
pdcurses/border.c
pdcurses/clear.c
pdcurses/color.c
pdcurses/debug.c
pdcurses/delch.c
pdcurses/deleteln.c
pdcurses/getch.c
pdcurses/getstr.c
pdcurses/getyx.c
pdcurses/inch.c
pdcurses/inchstr.c
pdcurses/initscr.c
pdcurses/inopts.c
pdcurses/insch.c
pdcurses/insstr.c
pdcurses/instr.c
pdcurses/kernel.c
pdcurses/keyname.c
pdcurses/mouse.c
pdcurses/move.c
pdcurses/outopts.c
pdcurses/overlay.c
pdcurses/pad.c
pdcurses/panel.c
pdcurses/printw.c
pdcurses/refresh.c
pdcurses/scanw.c
pdcurses/scroll.c
pdcurses/scr_dump.c
pdcurses/slk.c
pdcurses/termattr.c
pdcurses/touch.c
pdcurses/util.c
pdcurses/window.c
sdl2/pdcclip.c
sdl2/pdcdisp.c
sdl2/pdcgetsc.c
sdl2/pdckbd.c
sdl2/pdcscrn.c
sdl2/pdcsetsc.c
sdl2/pdcutil.c
#sdl2/sdltest.c
)

if (WIN32)
set(CMAKE_C_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_CXX_FLAGS_DEBUG "/W4 /MP /GF /RTCu /Od /MDd /Zi")
set(CMAKE_C_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi /DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "/W4 /MP /GF /O2 /Ob2 /Oi /Ot /MD /Zi /DNDEBUG")
endif (WIN32)

# general build definitions
#add_definitions(-D_UNICODE)
#add_definitions(-DUNICODE)
add_definitions(-D_SCL_SECURE_NO_WARNINGS)

if (WIN32)
add_definitions(-DPDC_DLL_BUILD)
endif (WIN32)
add_definitions(-DPDC_RGB)
add_definitions(-DPDC_FORCE_UTF8)
add_definitions(-DPDC_WIDE)

add_library(pdcurses SHARED ${HDRS} ${SRCS})
source_group("Header Files" FILES ${hdrs})
source_group("Source Files" FILES ${srcs})

target_include_directories(pdcurses
    PRIVATE
    	${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2/include
      ${CMAKE_CURRENT_SOURCE_DIR}/../SDL2_ttf/
    )	
	
target_link_libraries(pdcurses
    PRIVATE	    
    SDL2
    SDL2_ttf
    )		
