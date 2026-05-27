# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
bash build.sh   # build
bash run.sh     # build and run
./pong          # run after building
```

## Dependencies

- `libglfw` — window creation, event loop, and OpenGL context
- `libGL` — OpenGL

## Architecture

Single-file C program (`main.c`) with a fixed game loop structure:

```
inputs → update → render → swap
```

**Window + context setup** (runs once at startup):
- GLFW handles window creation, the event loop (`glfwPollEvents`), and OpenGL context creation
- OpenGL context is a 3.3 core profile, set via `glfwWindowHint`
- `GL_GLEXT_PROTOTYPES` + `<GL/glcorearb.h>` expose OpenGL 3.3+ functions directly — no manual function pointer loading needed

**Rendering**: VAO + VBO + EBO. The paddle is 4 vertices with 6 indices (two triangles). Shaders are loaded from `vert.glsl` / `frag.glsl` at startup via `read_file()` and compiled with `shader_compile()`.

**Coordinate system**: orthographic projection maps world space `[0,4] x [0,3]` to clip space. The `proj` uniform is set once; `model` is updated per object each frame.

## Keycodes

GLFW key events use named constants (`GLFW_KEY_W`, `GLFW_KEY_S`, `GLFW_KEY_ESCAPE`). Query state with `glfwGetKey(window, GLFW_KEY_X)`.
