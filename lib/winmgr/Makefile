all:
	gcc -std=c99 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -I../../src -g -O0 -DDEBUG example.c message.c handle.c window.c rect.c -o example -lncurses
	gcc -std=c99 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -I../../src -g -O0 -DDEBUG example2.c message.c handle.c window.c rect.c splitter.c layout.c -o example2 -lncurses
