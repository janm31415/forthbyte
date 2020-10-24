# forthbyte
Bytebeat and floatbeat machine.

Bytebeat is a type of music made from mathematical formulas, first discovered by [Viznut](http://viznut.fi/en/) in 2011.
The idea is that `t` represents a timer, infinitely increasing. In most cases `t` increases 8000 times per second (for a 8000Hz bytebeat song), but you can also let `t` represent a 44100Hz timer if you like.
If you take the next formula with a 8000Hz timer

    t | (t >> 4)
    
then a bytebeat generator will compute the above formula 8000 times per second for the corresponding value of `t`. Next the result (which can be a large value) is brought back to 8 bit by only retaining the remainder of the integer division by 256. This 8-bit value is sent to the speakers. The above formula sounds like [this](https://greggman.com/downloads/examples/html5bytebeat/html5bytebeat.html#t=0&e=0&s=8000&bb=5d000001000c00000000000000003a080b8211271601699e47dc7a96bdeefffe47b000).

Floatbeat is very similar to bytebeat. The difference is that the formula can use floating point (decimal) computations, and the result is expected to be in the interval `[-1, 1]`. Again this value is reworked to a 8-bit value and sent to the speakers. The advantage of floatbeat is that it is more natural here to work with sine and cosine functions. The next formula with a 44100Hz timer

    sin(440*t*3.1415926535*2/44100)

generates a 440Hz tone, like [this](https://greggman.com/downloads/examples/html5bytebeat/html5bytebeat.html#t=1&e=0&s=44100&bb=5d000001001f0000000000000000399a4a1a8961f80549b689502166e2852d12e851ce4a54854c282dd601f86f25b2bfffd47d0000).

There are already a bunch of very nice bytebeat and floatbeat generators available, such as
- [Greggman's html5bytebeat](https://github.com/greggman/html5bytebeat)
- [Viznut's IBNIZ](http://pelulamu.net/ibniz/)
- [Wurstcaptures](http://wurstcaptures.untergrund.net/music/)
- [Rampcode](https://github.com/gabochi/rampcode)
- probably many others

Forthbyte is my attempt at generating a bytebeat and floatbeat machine using a dialect of the language Forth. If you want to try it out, you can take a look at some examples in the examples subfolder.

Screenshot
----------
![](images/forthbyte.png)

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

Glossary
--------

### Preprocessor directives

`#byte` use bytebeat

`#float` use floatbeat (this is the default)

`#samplerate nr` set the sample rate (default value is 8000)

`#initmemory a b c ... ` initializes the memory with the values given by `a`, `b`, `c`, ... . There are 256 memory spots available.

### Predefined variables

`t` the timer

`sr` the current samplerate that was set by the preprocessor directive `#samplerate`

`c` the current channel. It's possible to use stereo. The left channel has a value of `c` equal to 0, and the right channel has a value equal to 1.

### Forth words

The Forth stack has 256 entries. If you go over this amount, the counter resets to zero, so you never can get a stack overflow.

`+` ( a b -- c ) Pops the two top values from the stack, and pushes their sum on the stack.

`-` ( a b -- c )  Pops the two top values from the stack, and pushes their subtraction on the stack.

`*` ( a b -- c )  Pops the two top values from the stack, and pushes their multiplication on the stack.

`/` ( a b -- c )  Pops the two top values from the stack, and pushes their division on the stack.

`<<` ( a b -- c )  Pops the two top values from the stack, and pushes their left shift on the stack.

`>>` ( a b -- c )  Pops the two top values from the stack, and pushes their right shift on the stack.

`&` ( a b -- c )  Pops the two top values from the stack, and pushes their binary and on the stack.

`|` ( a b -- c )  Pops the two top values from the stack, and pushes their binary or on the stack.

`^` ( a b -- c )  Pops the two top values from the stack, and pushes their binary xor on the stack.

`%` ( a b -- c )  Pops the two top values from the stack, and pushes their modulo on the stack.

`<` ( a b -- c )  Pops the two top values from the stack, compares them with `<`, and puts 0 on the stack if the comparison fails, 1 otherwise.

`>` ( a b -- c )  Pops the two top values from the stack, compares them with `>`, and puts 0 on the stack if the comparison fails, 1 otherwise.

`<=` ( a b -- c )  Pops the two top values from the stack, compares them with `<=`, and puts 0 on the stack if the comparison fails, 1 otherwise.

`>=` ( a b -- c )  Pops the two top values from the stack, compares them wiht `>=`, and puts 0 on the stack if the comparison fails, 1 otherwise.

`=` ( a b -- c )  Pops the two top values from the stack, compares them with `=`, and puts 0 on the stack if the comparison fails, 1 otherwise.

`<>` ( a b -- c )  Pops the two top values from the stack, compares them with `<>` (not equal), and puts 0 on the stack if the comparison fails, 1 otherwise.

`@` ( a -- b ) Pops the top value from the stack, gets the value at memory address `a`, and pushes this value on the stack.

`!` ( a b -- ) Pops the two top values from the stack, and stores value `a` at memory address `b`.

`>r` ( a -- ) Pops the top value from the stack, and moves it to the return stack. The return stack has 256 entries.

`r>` ( -- a ) Pops the top value from the return stack, and moves it to the regular stack. 

`: ;` Define a new word, e..g. `: twice 2 * ;` defines the word `twice`, so that `3 twice` equals `3 2 *` equals `6`.
