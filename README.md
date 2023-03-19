# _Cooler World_

This project is intended as a tribute to the 1998 PSX puzzle platformer [_Kula World_](https://www.mobygames.com/game/9070/roll-away/); it grew out of wanting to make a game from scratch in the spirit of [_Handmade Hero_](https://guide.handmadehero.org/) and subsequently working through [Learn OpenGL](https://learnopengl.com/).

To describe it as being in pre-alpha state would be something of an understatement; features implemented so far that might however be worth mentioning include:
- a deferred renderer with directional, point and spot lighting;
- parallax occlusion mapping;
- tonemapping;
- bloom;
- SSAO;
- hot reloading of both game code and shader code;
- groundwork for editor functionality, with ability to add cubes via mouse picking.

Since my current focus is on implementing editing and gameplay basics, only directional lighting is shown in the video along with picking to add cubes and toggling between editing and playing modes (although there is currently no playing as such):

https://user-images.githubusercontent.com/77587819/226212907-4e68425d-c800-4b65-afe0-77cef8e87f85.mp4

It is currently a Win32+OGL game written in (C-style) C++ and although the end goal is to make it a truly handmade game through and through, it currently builds upon Assimp, GLM, Dear ImGui, `math.h` and `stb_image.h`, since I was primarily focused on learning OpenGL rather than implementing those parts of the engine; I also depend on Tracy for profiling. Down the road, I aim to make it platform-independent and to implement a graphics API abstraction layer with DirectX as an alternative backend.

To keep compilation times low, there are only two translation units: `main.cpp` for the executable and `game.cpp` for the game DLL; it is for this reason that the latter includes the other .cpp files.

## Build instructions

1. [Install GLEW](https://glew.sourceforge.net/install.html).
2. Clone the repository with `--recurse-submodules`.
2. Run `cd src && cl /c all_imgui.cpp /O2` and move `all_imgui.obj` to `../build`.
4. Run `build.bat`, which will generate the unoptimized debug build `../build/main.exe`.
