#!/bin/bash
CC=gcc
SRC=main.c
OUT=pong

CFLAGS="-Wall -Wextra -g -std=c99 -fsanitize=address,undefined"
LIBS="-lglfw -lGL"

$CC $CFLAGS $SRC -o $OUT $LIBS
