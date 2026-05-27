
#define _POSIX_C_SOURCE 199309L
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


struct App {
    GLFWwindow *window;
    int window_width;
    int window_height;
    struct timespec timer;
};

struct v2 {
    float x;
    float y;
};

struct d2 {
    float w;
    float h;
};

struct Player {
    struct v2 position;
    struct d2 dimension;
    int velocity;
};

struct Background {
    struct d2 dimension;
};

struct Ball {
    struct v2 position;
    int velocity;
};

struct App app;
struct Player player1;
struct Player player2;
struct Ball ball;
struct Background background;


static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}


static GLuint shader_compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}


static void window_onresize(GLFWwindow *win, int width, int height) {

    float target = 4.0f / 3.0f;
    float actual = (float)width / (float)height;
    int vw, vh, vx, vy;
    if (actual > target) {
        vh = height; 
        vw = (int)(height * target);
        vx = (width - vw) / 2; 
        vy = 0;
    } else {
        vw = width; 
        vh = (int)(width / target);
        vx = 0; 
        vy = (height - vh) / 2;
    }
    glViewport(vx, vy, vw, vh);
}


GLFWwindow *window_create(void) {

    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return NULL; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *win = glfwCreateWindow(1920, 1080, "pong", NULL, NULL);
    if (!win) { fprintf(stderr, "glfwCreateWindow failed\n"); glfwTerminate(); return NULL; }

    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, window_onresize);
    return win;
}


void window_destroy(GLFWwindow *win) {

    glfwDestroyWindow(win);
    glfwTerminate();
}


int entity_data_append(struct d2 dim, GLfloat *buffer, GLuint offset, GLuint *indices) {
    buffer[0] = 0.0f;   buffer[1] = 0.0f;   // top-left     0
    buffer[2] = dim.w;  buffer[3] = 0.0f;   // top-right    1
    buffer[4] = dim.w;  buffer[5] = dim.h;  // bottom-right 2
    buffer[6] = 0.0f;   buffer[7] = dim.h;  // bottom-left  3
    
    indices[0] = 0 + offset; indices[1] = 1 + offset; indices[2] = 2 + offset;
    indices[3] = 3 + offset; indices[4] = 2 + offset; indices[5] = 0 + offset;

    return 8;
}


int main() {

    app.window = window_create();
    if (!app.window) return 1;

    glfwGetFramebufferSize(app.window, &app.window_width, &app.window_height);
    window_onresize(app.window, app.window_width, app.window_height);

    player1.position = (struct v2){ .x = 0.2f, .y = 1.5f };
    player1.dimension = (struct d2){ .w = 0.15f, .h = 0.6f };
    player1.velocity = 1;

    player2.position = (struct v2){ .x = 4 - 0.15f - 0.2f, .y = 1.5f };
    player2.dimension = (struct d2){ .w = 0.15f, .h = 0.6f };
    player2.velocity = 1;

    GLfloat data[16];
    GLuint indices[12];
    entity_data_append(player1.dimension,   data +  0,  0, indices + 0);
    entity_data_append(player2.dimension,   data +  8,  4, indices + 6);
    // entity_data_append(background.dimension,data + 16, 12, indices);


    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // create shader program
    char *vert_src = read_file("vert.glsl");
    char *frag_src = read_file("frag.glsl");
    GLuint vs = shader_compile(GL_VERTEX_SHADER, vert_src);
    GLuint fs = shader_compile(GL_FRAGMENT_SHADER, frag_src);
    free(vert_src);
    free(frag_src);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // orthographic projection for world space [0,4] x [0,3]
    float proj[16] = {
        0.5f,   0.0f,       0.0f, 0.0f,
        0.0f,   2.0f/3.0f,  0.0f, 0.0f,
        0.0f,   0.0f,      -1.0f, 0.0f,
       -1.0f,  -1.0f,       0.0f, 1.0f,
    };
    glUseProgram(prog);
    GLint proj_loc  = glGetUniformLocation(prog, "proj");
    GLint model_loc = glGetUniformLocation(prog, "model");
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, proj);

    float player1_model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        player1.position.x, player1.position.y, 0.0f, 1.0f,
    };

    float player2_model[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        player2.position.x, player2.position.y, 0.0f, 1.0f,
    };


    while (!glfwWindowShouldClose(app.window)) {

        struct timespec tn;
        clock_gettime(CLOCK_MONOTONIC, &tn);
        double dt = (tn.tv_sec - app.timer.tv_sec) + (tn.tv_nsec - app.timer.tv_nsec) * 1e-9;
        app.timer = tn;


        // inputs
        glfwPollEvents();
        if (glfwGetKey(app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(app.window, 1);
        if (glfwGetKey(app.window, GLFW_KEY_W) == GLFW_PRESS) {
            player1.position.y += player1.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_S) == GLFW_PRESS) {
            player1.position.y -= player1.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_UP) == GLFW_PRESS) {
            player2.position.y += player2.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            player2.position.y -= player2.velocity * dt;
        }

        
        // update
        if(player1.position.y < 0) {
            player1.position.y = 0;
        }
        if(player1.position.y + player1.dimension.h > 3) {
            player1.position.y = 3 - player1.dimension.h;
        }
        player1_model[13] = player1.position.y;

        if(player2.position.y < 0) {
            player2.position.y = 0;
        }
        if(player2.position.y + player2.dimension.h > 3) {
            player2.position.y = 3 - player2.dimension.h;
        }
        player2_model[13] = player2.position.y;


        // render
        glClearColor(0.1f, 0.0f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);

        glUniformMatrix4fv(model_loc, 1, GL_FALSE, player1_model);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (GLvoid *) 0);

        glUniformMatrix4fv(model_loc, 1, GL_FALSE, player2_model);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (GLvoid *) (6 * sizeof(GLuint)));

        glfwSwapBuffers(app.window);
    }

    window_destroy(app.window);

    return 0;
}
