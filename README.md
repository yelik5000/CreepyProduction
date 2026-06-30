# Creepy Production
This is the place when you need a renderer, a physics engine or just an open source game.
Right now I don't have much to offer, this is completely centered on the graphycs now, however Ray-Tracing is (almost) set up.
It is basicly a rasterizer that will (should) grow into a complete game engine and then into a game library.
## How to build
### On linux or macOS
1. The project uses CMake so you'll need to install that:
    ```sudo apt install cmake```
    or download at https://cmake.org/download/
   
3. As well as GLFW and vulkanSDK
    ```sudo apt install libglfw3 libglfw3-dev```
    or download at https://www.glfw.org/download
4. And download vulkanSDK at  https://vulkan.lunarg.com/sdk/home
    (*if you want to use specific vulkan or glfw version for your application use the .env.cmake to target it*) 
5. Run ```chmod +x unixBuild.sh``` (only for linux) and ``` ./unixBuild.sh```
### On windows
1. Download CMake at https://cmake.org/download/
   
2. Download GLFW at https://www.glfw.org/download
3. Download vulkanSDK at https://vulkan.lunarg.com/sdk/home

4. Run mingwBuild.bat

You as well will need tiny object loader, so download it at https://github.com/tinyobjloader/tinyobjloader?tab=readme-ov-file
