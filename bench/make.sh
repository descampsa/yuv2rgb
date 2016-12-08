#!/bin/bash

cc -I /usr/local/Cellar/ffmpeg/3.0.1/include -Wall -g -c \
        -o bench.o bench.c
cc  bench.o -L /usr/local/Cellar/ffmpeg/3.0.1/lib \
        -lavdevice -lavformat -lavfilter -lavcodec \
        -lswresample -lswscale -lavutil  \
        -o bench