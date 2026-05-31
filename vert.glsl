#version 330 core

layout(location = 0) in vec2 position;
layout(location = 1) in vec4 color;
layout(location = 2) in mat4 view;
uniform mat4 proj;

out vec4 v_color;

void main() {

    v_color = color;
    gl_Position = proj * view * vec4(position, 0.0, 1.0);
}
