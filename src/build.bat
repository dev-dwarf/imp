@echo off   
if not exist ..\build mkdir ..\build
pushd ..\build

rem copy ..\libs\SDL2.dll .\

rem ~~~ build SDL2/DearImGui/OpenGL3 template
rem cl /nologo /Zi /I..\src\ ../src/template_sdl_dearimgui_opengl3.cpp ../src/third_party/dear_imgui/imgui*.cpp /link ..\libs\SDL2.lib ..\libs\SDL2main.lib opengl32.lib shell32.lib

rem ~~~ build SDL2/MicroUI/OpenGL template
rem cl /nologo /Zi /I..\src\ ../src/template_sdl_microui_opengl3.c ../src/third_party/microui/microui.c /link ..\libs\SDL2.lib ..\libs\SDL2main.lib opengl32.lib shell32.lib

cl /nologo /Zi /I..\src\ ../src/template_raylib.c /link ..\libs\raylib.lib winmm.lib opengl32.lib gdi32.lib user32.lib shell32.lib

popd
