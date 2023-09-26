mkdir -p ../build
cc ../src/template_raylib.c -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o ../build/template_raylib
