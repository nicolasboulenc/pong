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

**Rendering**: VAO + two VBOs. 6 explicit vertices per instance (two triangles, no EBO). All 4 entities drawn in one `glDrawArraysInstanced` call. Shaders are loaded from `vert.glsl` / `frag.glsl` at startup via `read_file()` and compiled with `shader_compile()`.

**Coordinate system**: orthographic projection maps world space `[0,4] x [0,3]` to clip space. The `proj` uniform is set once; `model` is updated per object each frame.

## Learning context

This project is a stepping stone toward a more complex game. When the user asks about applying a pattern or technique, don't dismiss it with "for a game this size" — the point is to learn and practice the idea here before using it in a larger project.

## Rendering techniques — concepts to explore

**Reducing draw calls**

Instanced rendering is implemented (single `glDrawArraysInstanced` call, model matrices as `divisor=1` attributes), but it is the wrong fit for this game. Instancing pays off with many identical instances (particles, bullets, trees) where sharing vertex data matters. For 4 different entities it adds constraints without meaningful savings:

- `divisor=0` shares data across all instances — no per-vertex-per-instance data via vertex attributes
- `divisor=1` gives one value per instance — fine for a single flat color, but nothing richer
- Per-vertex-per-instance data requires TBO or UBO tricks in GL 3.3

**Geometry batching** is the better approach here: expand all vertices into one flat VBO (4 entities × 6 vertices = 24), each vertex carries its own position, color, and matrix (or a model index into a UBO). Single `glDrawArrays(GL_TRIANGLES, 0, 24)` call, full per-vertex control, no divisor machinery.

**Vertex attribute divisor behavior**: `divisor=0` means the attribute is shared across all instances — the same `count` values are re-read for every instance. It does *not* advance across instance boundaries. `divisor=1` gives one value per instance, applied to all vertices of that instance. There is no vertex attribute divisor that gives per-vertex-per-instance data. For flat per-instance color, use `divisor=1` with one color per instance. For true per-vertex-per-instance data in GL 3.3, the options are a TBO (`samplerBuffer` + `texelFetch`) or a UBO/uniform array indexed by `gl_InstanceID * 6 + gl_VertexID`; SSBOs are GL 4.3+. Note: `gl_VertexID` resets to 0 for each instance in `glDrawArraysInstanced`.

**When UBOs are worth it**

A single `glUniformMatrix4fv` call is fine for one matrix on one shader program. UBOs become the right tool when you have a set of per-frame data shared across multiple shader programs — for example a "camera" block containing view matrix, projection matrix, and eye position. Instead of calling `glUniform*` 3+ times on every program each frame, you update one buffer once and every program that binds the same binding point sees the new values automatically. The binding point is the key: it decouples the buffer from any specific program.

**std140 layout**

Data inside a UBO must follow std140 alignment rules. `mat4` is straightforward (16-byte aligned, 64 bytes). Scalars, `vec3`, and arrays have non-obvious padding that causes silent bugs if ignored — worth learning carefully before using mixed-type UBOs.

## GL types vs C types

Use GL types (`GLfloat`, `GLuint`, `GLint`) at the OpenGL boundary — buffer contents uploaded to the GPU, object handles returned by OpenGL (`vao`, `vbo`, `prog`), and data passed directly to `glBufferData` / `glVertexAttribPointer`. Use plain C types (`float`, `unsigned int`, `int`) for application logic — physics, game state, CPU-side math, structs that never leave the CPU.

## Keycodes

GLFW key events use named constants (`GLFW_KEY_W`, `GLFW_KEY_S`, `GLFW_KEY_ESCAPE`). Query state with `glfwGetKey(window, GLFW_KEY_X)`.
