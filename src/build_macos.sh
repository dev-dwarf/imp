#!/usr/local/bin/bash
mkdir -p ../build
clang -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL ../libs/libraylib.a template_raylib -o ..\build\template_raylib

