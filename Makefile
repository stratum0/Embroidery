path-guessing: path-guessing.c++ Makefile
	g++ -W -Wall -Wextra -Werror -pedantic -ggdb -O4 -std=c++11 -o $@ $< -lSDL2 -lSDL2_image -lSDL2_gfx
