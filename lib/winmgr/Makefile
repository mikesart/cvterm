all:
	gcc-5 -std=c99 -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address -g -O0 -I../../src -I../libtickit/include example.c message.c handle.c window.c rect.c key.c ../libtickit/.libs/libtickit.a ../libtermkey/.libs/libtermkey.a -o example -lncursesw
