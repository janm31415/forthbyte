# forthbyte
Bytebeat machine


Under development

## Building on Windows

1.) Build SDL2 (and install in C:\program files\sdl2)

2.) Build freetype-2.4.8 for x64bit using the sln file provided in folder builds/win32/vc2010

3.) Build ttf-2.0.15 using CMake, point to the correct freetype location. Press "Advanced" in CMakeGui to set the links to the lib files if these links are not visible by default.

4.) Install to C:\program files\sdl_ttf

5.) Copy the freetype lib files also to the sdl_ttf lib folder as your app needs to link with those too.

