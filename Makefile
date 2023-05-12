CC = g++
SOURCES = src/main.cpp src/song.cpp
# IMGUI_SOURCES = imgui/imgui_demo.cpp
# IMGUI_SOURCES += imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui.cpp
# IMGUI_SOURCES += imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
INCLUDES = -I imgui
LIBS = -lglfw3 -lGL -lX11
IMGUI_OBJS = build/imgui_demo.o build/imgui_draw.o build/imgui_tables.o build/imgui_widgets.o build/imgui.o build/imgui_impl_glfw.o build/imgui_impl_opengl3.o

all: build/soundbox

clean:
	rm build/*

build/soundbox: $(IMGUI_OBJS) $(SOURCES)
	$(CC) $(INCLUDES) -o build/soundbox $(SOURCES) $(IMGUI_OBJS) $(LIBS)

build/imgui_demo.o: imgui/imgui_demo.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_demo.o imgui/imgui_demo.cpp

build/imgui_draw.o: imgui/imgui_draw.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_draw.o imgui/imgui_draw.cpp

build/imgui_tables.o: imgui/imgui_tables.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_tables.o imgui/imgui_tables.cpp

build/imgui_widgets.o: imgui/imgui_widgets.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_widgets.o imgui/imgui_widgets.cpp

build/imgui.o: imgui/imgui.cpp
	$(CC) $(INCLUDES) -c -o build/imgui.o imgui/imgui.cpp

build/imgui_impl_glfw.o: imgui/backends/imgui_impl_glfw.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_impl_glfw.o imgui/backends/imgui_impl_glfw.cpp

build/imgui_impl_opengl3.o: imgui/backends/imgui_impl_opengl3.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_impl_opengl3.o imgui/backends/imgui_impl_opengl3.cpp