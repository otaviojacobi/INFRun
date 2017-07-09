CPP = g++
OPTS =  -std=c++11 -Wall -Wno-unused-function -g -I ./include/ -L"/usr/lib" ../../bin/linux-gcc-64/libIrrKlang.so src/glad.c src/textrendering.cpp src/tiny_obj_loader.cpp src/stb_image.cpp ./lib-linux/libglfw3.a -lrt -lm -ldl -lX11 -lpthread -lXrandr -lXinerama -lXxf86vm -lXcursor


all: src/*.cpp include/*.h
	$(CPP) src/main.cpp -o mario $(OPTS)

clean:
	rm example
