# forthbyte
Bytebeat and floatbeat machine

Bytebeat is a type of music made from mathematical formulas, first discovered by [Viznut](http://viznut.fi/en/) in 2011.
The idea is that `t` represents a timer, infinitely increasing. In most cases `t` increases 8000 times per second (for a 8000Hz bytebeat song), but you can also let `t` represent a 44100Hz timer if you like.
If you take the next formula with a 8000Hz timer

    t | (t >> 4)
    
then a bytebeat generator will compute the above formula 8000 times per second for the corresponding value of `t`. Next the result (which can be a large value) is brought back to 8 bit by only retaining the remainder of the integer division by 256. This 8-bit value is sent to the speakers. The above formula sounds like [this](https://greggman.com/downloads/examples/html5bytebeat/html5bytebeat.html#t=0&e=0&s=8000&bb=5d000001000c00000000000000003a080b8211271601699e47dc7a96bdeefffe47b000).

Floatbeat is very similar to bytebeat. The difference is that the formula can use floating point (decimal) computations, and the result is expected to be in the interval `[-1, 1]`. Again this value is reworked to a 8-bit value and send to the speakers. The advantage of floatbeat is that it is more natural here to work with sine and cosine functions. The next formula with a 44100Hz timer

    sin(440*t*3.1415926535*2/44100)

generates a 440Hz tone, like [this](https://greggman.com/downloads/examples/html5bytebeat/html5bytebeat.html#t=1&e=0&s=44100&bb=5d000001001f0000000000000000399a4a1a8961f80549b689502166e2852d12e851ce4a54854c282dd601f86f25b2bfffd47d0000).

Building
--------
### Building instructions for Windows

1.) Download and build [SDL2](https://www.libsdl.org/) (and install in C:\program files\sdl2 or any other location but then you'll need to update the CMakeLists.txt files with this new location)

2.) Download and build [freetype-2.4.8](https://www.freetype.org/download.html) for x64bit using the sln file provided in folder builds/win32/vc2010

3.) Download and build [ttf-2.0.15](https://www.libsdl.org/projects/SDL_ttf/) using CMake, point to the correct freetype location. Press "Advanced" in CMakeGui to set the links to the lib files if these links are not visible by default.

4.) Install to C:\program files\sdl_ttf (or any other location but then you'll need to update the CMakeLists.txt files with this new location)

5.) Copy the freetype lib files also to the sdl_ttf lib folder as your app needs to link with those too.

6.) Use CMake to make a Visual Studio solution file for this project and build.

### Building instructions for Linux

1.) Install SDL2: `sudo apt-get install libsdl2-dev`

2.) Install SDL2-ttf: `sudo apt-get install libsdl2-ttf-dev`

3.) Use CMake to generate a makefile, and run make in the build folder.

### Building instructions for MacOs

1.) Download the SDL2 framework from the [SDL2](https://www.libsdl.org/) website and install in /Library/Frameworks/

2.) Download the SDL2-ttf framework from the [ttf-2.0.15](https://www.libsdl.org/projects/SDL_ttf/) website and install in /Library/Frameworks/

3.) Use CMake to generate a XCode project and build.
