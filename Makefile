CC = g++
SOURCES = main.cpp 
IMGUI_SOURCES = imgui/imgui_demo.cpp
IMGUI_SOURCES += imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui.cpp
IMGUI_SOURCES += imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp
INCLUDES = -I imgui
CFLAGS = -lglfw3 -lGL -lX11

clean:
	rm build/*

build/imgui_demo.o: imgui/imgui_demo.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_demo.o imgui/imgui_demo.cpp

build/imgui_draw.o: imgui/imgui_draw.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_draw.o imgui/imgui_draw.cpp

build/imgui_tables.o: imgui/imgui_tables.cpp
	$(CC) $(INCLUDES) -c -o build/imgui_tables.o imgui/imgui_tables.cpp

build/imgui_widgets.o: imgui/imgui_widgets.cpp
	$(CC) $(INCLUDES) -c -o imgui/imgui_widgets.o imgui/imgui_widgets.cpp

build/imgui.o: imgui/imgui.cpp
	$(CC) $(INCLUDES) -c -o imgui/imgui.o imgui/imgui.cpp

all: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o build/soundbox