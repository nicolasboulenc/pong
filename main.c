
#define _POSIX_C_SOURCE 199309L
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>
#include <GL/glcorearb.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define INSTANCE_COUNT 4
#define TRIANGLE_COUNT_PER_INSTANCE 2
#define VERTEX_COUNT_PER_TRIANGLE 3
#define VERTEX_COUNT_PER_INSTANCE 6
#define FLOAT_COUNT_PER_VERTEX 2
#define FLOAT_COUNT_PER_COLOR 4
#define FLOAT_COUNT_PER_MATRIX 16

#define VERTEX_COUNT_PER_QUAD_BATCHED 4
#define INDEX_COUNT_PER_QUAD 6

#define POSITION_LOC 0
#define COLOR_LOC 1
#define VIEW_LOC 2

#define WORLD_W 4.0f
#define WORLD_H 3.0f


typedef struct v2_Tag {
    float x;
    float y;
} v2;

typedef struct c4_Tag {
    float r;
    float g;
    float b;
    float a;
} c4;

typedef struct d2_Tag {
    float w;
    float h;
} d2;

typedef struct App_Tag {
    GLFWwindow *window;
    int window_width;
    int window_height;
    struct timespec timer;
    d2 viewport;
    int is_paused;
} App;

typedef struct Entity_Tag {
    // physics
    v2 position;
    float rotation;
    v2 scale;
    d2 dimension;
    v2 direction;
    float velocity;
    // rendering
    float transform[16];
    c4 colors[4];
    unsigned int ebo_offset;
} Entity;

typedef struct Quad_Batched_Tag {
    // physics
    v2 pos;
    float rotation;
    v2 sca;
    d2 dim;
    v2 dir;
    float velocity;
    // rendering
    float transform[16];
    c4 colors[4];
    unsigned int ebo_offset;
} Quad_Batched;

typedef struct Geometry_Tag {
    GLfloat *vertices;          // vertices shared for all instances
    GLuint *indices;            // vertex indices
    GLfloat *vertex_data;       // vertices data per instance, color, etc
    GLfloat *instance_data;     // instance data, matrix, etc
    size_t vertex_data_count;
    size_t instance_data_count;
} Geometry;

typedef struct Geometry_Batched_Tag {
    GLfloat *vertex_data;   // vertices data per instance, color, etc
    GLuint *index_data;     // vertex indices
    size_t vertex_count;
    size_t index_count;
} Geometry_Batched;

typedef struct Wall_Tag {
    v2 a;
    v2 b;
    v2 normal;
} Wall;

App app = {
    .is_paused = 1,
    .viewport.w = 1920,
    .viewport.h = 1080
};





char *read_file(const char *path) {
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


GLuint shader_compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}


void window_onresize(GLFWwindow *win, int width, int height) {

    float target = (float)WORLD_W / (float)WORLD_H;
    float actual = (float)width / (float)height;
    int vw, vh, vx, vy;
    if (actual > target) {
        vh = height; 
        vw = (int)(height * target);
        vx = (width - vw) / 2; 
        vy = 0;
    } 
    else {
        vw = width; 
        vh = (int)(width / target);
        vx = 0; 
        vy = (height - vh) / 2;
    }
    app.viewport.w = roundf(vw);
    app.viewport.h = roundf(vh);
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


void geometry_init(Geometry *geo) {

    GLfloat *data = geo->vertices;
    // GLuint *indices = geo->indices;
    d2 dim = { .w=1, .h=1 };

    data[0] = -dim.w / 2.0f; data[1] = -dim.h / 2.0f;     // top-left     0
    data[2] =  dim.w / 2.0f; data[3] = -dim.h / 2.0f;     // top-right    1
    data[4] =  dim.w / 2.0f; data[5] =  dim.h / 2.0f;     // bottom-right 2

    data[6] =  -dim.w / 2.0f; data[7] =   dim.h / 2.0f;   // bottom-left  3
    data[8] =   dim.w / 2.0f; data[9] =   dim.h / 2.0f;   // bottom-right 2
    data[10] = -dim.w / 2.0f; data[11] = -dim.h / 2.0f;   // top-left     0

    // indices[0] = 0; indices[1] = 1; indices[2] = 2;
    // indices[3] = 3; indices[4] = 2; indices[5] = 0;
}

void geometry_batched_init(Geometry_Batched *geo) {
    geo->vertex_count = 0;
    geo->index_count = 0;
}

void geometry_batched_append(Geometry_Batched *geo, const Quad_Batched *quad) {

    GLfloat *data = geo->vertex_data + geo->vertex_count * (FLOAT_COUNT_PER_VERTEX + FLOAT_COUNT_PER_COLOR + FLOAT_COUNT_PER_MATRIX);
    const d2 *dim = &quad->dim;
    const c4 *colors = quad->colors;

    int i = 0;
    // top-left
    data[i++] = -dim->w / 2.0f; data[i++] = -dim->h / 2.0f;
    data[i++] = colors[0].r; data[i++] = colors[0].g; data[i++] = colors[0].b; data[i++] = colors[0].a;
    for(int j=0; j<FLOAT_COUNT_PER_MATRIX; j++) {
        data[i++] = quad->transform[j];
    }
    // top-right
    data[i++] = dim->w / 2.0f; data[i++] = -dim->h / 2.0f;
    data[i++] = colors[1].r; data[i++] = colors[1].g; data[i++] = colors[1].b; data[i++] = colors[1].a;
    for(int j=0; j<FLOAT_COUNT_PER_MATRIX; j++) {
        data[i++] = quad->transform[j];
    }
    // bottom-right
    data[i++] = dim->w / 2.0f; data[i++] =  dim->h / 2.0f;
    data[i++] = colors[2].r; data[i++] = colors[2].g; data[i++] = colors[2].b; data[i++] = colors[2].a;
    for(int j=0; j<FLOAT_COUNT_PER_MATRIX; j++) {
        data[i++] = quad->transform[j];
    }
    // bottom-left
    data[i++] = -dim->w / 2.0f; data[i++] = dim->h / 2.0f;
    data[i++] = colors[3].r; data[i++] = colors[3].g; data[i++] = colors[3].b; data[i++] = colors[3].a;
    for(int j=0; j<FLOAT_COUNT_PER_MATRIX; j++) {
        data[i++] = quad->transform[j];
    }

    GLuint base = geo->vertex_count;
    GLuint *indices = geo->index_data + geo->index_count;
    indices[0] = base + 0; indices[1] = base + 1; indices[2] = base + 2;
    indices[3] = base + 3; indices[4] = base + 2; indices[5] = base + 0;

    geo->vertex_count += VERTEX_COUNT_PER_QUAD_BATCHED;
    geo->index_count += INDEX_COUNT_PER_QUAD;
}

void geometry_vertex_data_append(Geometry *geo, const c4 *color) {

    GLfloat *data = geo->vertex_data + geo->vertex_data_count * FLOAT_COUNT_PER_COLOR;

    // 6 vertices * 4 colors
    data[0]  = color->r; data[1]  = color->g; data[2]  = color->b; data[3]  = color->a;
    data[4]  = color->r; data[5]  = color->g; data[6]  = color->b; data[7]  = color->a;
    data[8]  = color->r; data[9]  = color->g; data[10] = color->b; data[11] = color->a;

    data[12] = color->r; data[13] = color->g; data[14] = color->b; data[15] = color->a;
    data[16] = color->r; data[17] = color->g; data[18] = color->b; data[19] = color->a;
    data[20] = color->r; data[21] = color->g; data[22] = color->b; data[23] = color->a;

    geo->vertex_data_count += VERTEX_COUNT_PER_INSTANCE;
}

void geometry_instance_data_append(Geometry *geo, const float mat[16]) {

    GLfloat *data = geo->instance_data + geo->instance_data_count * FLOAT_COUNT_PER_MATRIX;

    for(int i=0; i<FLOAT_COUNT_PER_MATRIX; i++) {
        data[i] = mat[i];
    }
    geo->instance_data_count++;
}


void mat4_identity(float m[16]) {

    m[0] =  1.0f; m[1] =  0.0f; m[2] =  0.0f; m[3] =  0.0f;
    m[4] =  0.0f; m[5] =  1.0f; m[6] =  0.0f; m[7] =  0.0f;
    m[8] =  0.0f; m[9] =  0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void mat4_translate(float m[16], float tx, float ty) {

    m[0] =  1.0f; m[1] =  0.0f; m[2] =  0.0f; m[3] =  0.0f;
    m[4] =  0.0f; m[5] =  1.0f; m[6] =  0.0f; m[7] =  0.0f;
    m[8] =  0.0f; m[9] =  0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = tx;   m[13] = ty;   m[14] = 0.0f; m[15] = 1.0f;
}

void mat4_scale(float m[16], float sx, float sy) {

    m[0] =  sx;   m[1] =  0.0f; m[2] =  0.0f; m[3] =  0.0f;
    m[4] =  0.0f; m[5] =  sy;   m[6] =  0.0f; m[7] =  0.0f;
    m[8] =  0.0f; m[9] =  0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void mat4_rotate(float m[16], float rz) {

    m[0] =  cosf(rz); m[1] = -sinf(rz); m[2] =  0.0f; m[3] =  0.0f;
    m[4] =  sinf(rz); m[5] =  cosf(rz); m[6] =  0.0f; m[7] =  0.0f;
    m[8] =  0.0f;     m[9] =  0.0f;     m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f;     m[13] = 0.0f;     m[14] = 0.0f; m[15] = 1.0f;
}

void mat4_mul(float out[16], float a[16], float b[16]) {

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++)
                s += a[k*4 + row] * b[col*4 + k];
            out[col*4 + row] = s;
        }
    }
}

void mat4_trs(float m[16], float tx, float ty, float rz, float sx, float sy) {
    float c = cosf(rz), s = sinf(rz);
    m[0] = sx*c;  m[1] = -sx*s; m[2] =  0.0f; m[3] =  0.0f;
    m[4] = sy*s;  m[5] =  sy*c; m[6] =  0.0f; m[7] =  0.0f;
    m[8] = 0.0f;  m[9] =  0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = tx;   m[13] = ty;   m[14] = 0.0f; m[15] = 1.0f;
}


float dst_squared(float ax, float ay, float bx, float by) {
    return (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
} 


float vec2_cross(float ax, float ay, float bx, float by) {
    return ax * by - ay * bx;
}

float sign(float x) { 
    return x > 0 ? 1 : x < 0 ? -1 : 0;
}


int segments_intersect( float ax, float ay, float bx, float by,   // segment AB
                        float cx, float cy, float dx, float dy)   // segment CD
{
    // Vectors have to have the same origin for the cross product to work
    // hence the - cx and - cy, c is chosen as origin
    float a_side_of_cd = vec2_cross(dx-cx, dy-cy, ax-cx, ay-cy);
    float b_side_of_cd = vec2_cross(dx-cx, dy-cy, bx-cx, by-cy);
    float c_side_of_ab = vec2_cross(bx-ax, by-ay, cx-ax, cy-ay);
    float d_side_of_ab = vec2_cross(bx-ax, by-ay, dx-ax, dy-ay);

    if (sign(a_side_of_cd) != sign(b_side_of_cd) && sign(c_side_of_ab) != sign(d_side_of_ab))
        return 1;

    return 0;
}

int main() {

    c4 magenta  = { .r = 1.0f, .g = 1.0f, .b = 1.0f, .a = 1.0f };
    c4 red      = { .r = 1.0f, .g = 0.2f, .b = 0.2f, .a = 1.0f };
    c4 gray     = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f };

    Quad_Batched p1 = { 
        .pos.x = 0.2f, .pos.y = 1.5f,
        .dim.w = 0.15f, .dim.h = 0.6f,
        .rotation = 0.0f,
        .sca.x = 1.0f, .sca.y = 1.0f,
        .dir.x = 0.0f, .dir.y = 1.0f,
        .colors = { magenta, magenta, magenta, magenta },
        .velocity = 1.2f,
    };

    Quad_Batched p2 = { 
        .pos.x = WORLD_W - 0.15f - 0.2f, .pos.y = 1.5f,
        .dim.w = 0.15f, .dim.h = 0.6f,
        .rotation = 0.0f,
        .sca.x = 1.0f, .sca.y = 1.0f,
        .dir.x = 0.0f, .dir.y = 1.0f,
        .colors = { magenta, magenta, magenta, magenta },
        .velocity = 1.2f,
    };

    Quad_Batched ball = { 
        .pos.x = WORLD_W / 2.0f, .pos.y = WORLD_H /  2.0f,
        .dim.w = 0.15f, .dim.h = 0.15f,
        .rotation = 0.0f,
        .sca.x = 1.0f, .sca.y = 1.0f,
        .dir.x = -0.4f, .dir.y = 0.6f,
        .colors = { red, red, red, red },
        .velocity = 2.0f,
    };
    float ball_radius = ball.dim.w / 2.0f;


    Quad_Batched bg = {
        .pos.x = WORLD_W / 2.0f, .pos.y = WORLD_H /  2.0f,
        .dim.w = WORLD_W, .dim.h = WORLD_H,
        .rotation = 0.0f,
        .sca.x = 1.0f, .sca.y = 1.0f,
        .dir.x = 0.0f, .dir.y = 0.0f,
        .colors = { gray, gray, gray, gray },
        .velocity = 0.0f,
    };

    // for instance rendering
    // GLfloat vertices[VERTEX_COUNT_PER_INSTANCE * FLOAT_COUNT_PER_VERTEX];
    // GLfloat instance_data[INSTANCE_COUNT * FLOAT_COUNT_PER_MATRIX];
    // Geometry geometry = { vertices, indices, vertex_data, instance_data, 0, 0 };
    // geometry_init(&geometry);
    
    GLfloat vertex_data[INSTANCE_COUNT * VERTEX_COUNT_PER_QUAD_BATCHED * (FLOAT_COUNT_PER_VERTEX + FLOAT_COUNT_PER_COLOR + FLOAT_COUNT_PER_MATRIX)];
    GLuint index_data[INSTANCE_COUNT * INDEX_COUNT_PER_QUAD];
    Geometry_Batched geometry_batched = { .vertex_data = vertex_data, .index_data = index_data, .vertex_count = 0, .index_count = 0 };
    geometry_batched_append(&geometry_batched, &bg);
    geometry_batched_append(&geometry_batched, &p1);
    geometry_batched_append(&geometry_batched, &p2);
    geometry_batched_append(&geometry_batched, &ball);


    float proj_m[16];
    float scale_m[16];
    float trans_m[16];
    mat4_scale(scale_m, 2.0f / 4.0f, 2.0f / 3.0f);
    mat4_translate(trans_m, -1.0f, -1.0f);
    mat4_mul(proj_m, trans_m, scale_m); // T * S * vertex


    app.window = window_create();
    if (!app.window) return 1;
    app.is_paused = 1;

    glfwGetFramebufferSize(app.window, &app.window_width, &app.window_height);
    window_onresize(app.window, app.window_width, app.window_height);

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

    glUseProgram(prog);
    GLint proj_loc = glGetUniformLocation(prog, "proj");

    GLuint vao;
    // for instanced rendering
    // GLuint vbo_vertices;
    // GLuint vbo_instance_data;
    // for batched rendering
    GLuint vbo_vertex_data;
    GLuint ebo_index_data;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // for batch rendering
    glGenBuffers(1, &ebo_index_data);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_index_data);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_data), geometry_batched.index_data, GL_STATIC_DRAW);

    glGenBuffers(1, &vbo_vertex_data);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_data);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), geometry_batched.vertex_data, GL_DYNAMIC_DRAW);

    GLsizei stride = (FLOAT_COUNT_PER_VERTEX + FLOAT_COUNT_PER_COLOR + FLOAT_COUNT_PER_MATRIX) * sizeof(GLfloat);
    glVertexAttribPointer(POSITION_LOC, 2, GL_FLOAT, GL_FALSE, stride, (void*)(0));
    glEnableVertexAttribArray(POSITION_LOC);
    glVertexAttribPointer(COLOR_LOC, 4, GL_FLOAT, GL_FALSE, stride, (void*)(FLOAT_COUNT_PER_VERTEX * sizeof(GLfloat)));
    glEnableVertexAttribArray(COLOR_LOC);
    for (int i = 0; i < 4; i++) {
        glVertexAttribPointer(VIEW_LOC + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)((FLOAT_COUNT_PER_VERTEX + FLOAT_COUNT_PER_COLOR + i*4) * sizeof(GLfloat)));
        glEnableVertexAttribArray(VIEW_LOC + i);
    }

    // for instanced rendering
    // glGenBuffers(1, &vbo_vertices);
    // glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // glVertexAttribPointer(POSITION_LOC, 2, GL_FLOAT, GL_FALSE, sizeof(v2), 0);
    // glEnableVertexAttribArray(POSITION_LOC);

    // glGenBuffers(1, &vbo_instance_data);
    // glBindBuffer(GL_ARRAY_BUFFER, vbo_instance_data);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(instance_data), instance_data, GL_DYNAMIC_DRAW);

    // needed as maximum size of a slot is vec4
    // for(int i = 0; i < 4; i++) {
    //     glVertexAttribPointer(VIEW_LOC + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (void*)(i * 4 * sizeof(float)));
    //     glVertexAttribDivisor(VIEW_LOC + i, 1);
    //     glEnableVertexAttribArray(VIEW_LOC + i);
    // }

    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, proj_m);


    // so far only 1 progam and 1 vao for the whole game
    glUseProgram(prog);
    glBindVertexArray(vao);

    int space_prev = GLFW_RELEASE;


    while (!glfwWindowShouldClose(app.window)) {

        struct timespec tn;
        clock_gettime(CLOCK_MONOTONIC, &tn);
        double dt = (tn.tv_sec - app.timer.tv_sec) + (tn.tv_nsec - app.timer.tv_nsec) * 1e-9;
        app.timer = tn;

        // inputs
        glfwPollEvents();
        if (glfwGetKey(app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(app.window, 1);
        }

        int space_curr = glfwGetKey(app.window, GLFW_KEY_SPACE);
        if (space_curr == GLFW_PRESS && space_prev == GLFW_RELEASE) {
            app.is_paused = !app.is_paused;
            fprintf(stderr, "game p\n");
        }     
        space_prev = space_curr;

        if (glfwGetKey(app.window, GLFW_KEY_W) == GLFW_PRESS) {
            p1.pos.y += p1.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_S) == GLFW_PRESS) {
            p1.pos.y -= p1.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_UP) == GLFW_PRESS) {
            p2.pos.y += p2.velocity * dt;
        }
        if (glfwGetKey(app.window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            p2.pos.y -= p2.velocity * dt;
        }


        // update
        mat4_trs(bg.transform, bg.pos.x, bg.pos.y, bg.rotation, bg.sca.x, bg.sca.y);

        if(p1.pos.y - p1.dim.h / 2.0f < 0) {
            p1.pos.y = p1.dim.h / 2.0f;
        }
        if(p1.pos.y + p1.dim.h / 2.0f > WORLD_H) {
            p1.pos.y = WORLD_H - p1.dim.h / 2.0f;
        }
        mat4_trs(p1.transform, p1.pos.x, p1.pos.y, p1.rotation, p1.sca.x, p1.sca.y);

        if(p2.pos.y - p2.dim.h / 2.0f < 0) {
            p2.pos.y = p2.dim.h / 2.0f;
        }
        if(p2.pos.y + p2.dim.h / 2.0f > WORLD_H) {
            p2.pos.y = WORLD_H - p2.dim.h / 2.0f;
        }
        mat4_trs(p2.transform, p2.pos.x, p2.pos.y, p2.rotation, p2.sca.x, p2.sca.y);

        if(app.is_paused == 0) {
            float prev_x = ball.pos.x;
            float prev_y = ball.pos.y;
            ball.pos.x += ball.dir.x * ball.velocity * dt;
            ball.pos.y += ball.dir.y * ball.velocity * dt;

            // collision left wall
            if(segments_intersect(  ball_radius, 0.0f, ball_radius, (float)WORLD_H,
                                    prev_x, prev_y, ball.pos.x, ball.pos.y) != 0) {
                if(ball.dir.x < 0) {
                    ball.pos.x = ball_radius;
                    ball.dir.x *= -1.0f;
                }
            }
            // collision right wall
            else if(segments_intersect( (float)WORLD_W - ball_radius, 0.0f, (float)WORLD_W - ball_radius, (float)WORLD_H,
                                        prev_x, prev_y, ball.pos.x, ball.pos.y) != 0) {
                if(ball.dir.x > 0) {
                    ball.pos.x = WORLD_W - ball_radius;
                    ball.dir.x *= -1.0f;
                }
            }
            // collision bottom wall (y=WORLD_H, visual top)
            if(segments_intersect(  0.0f, (float)WORLD_H - ball_radius, (float)WORLD_W, (float)WORLD_H - ball_radius,
                                    prev_x, prev_y, ball.pos.x, ball.pos.y) != 0) {
                if(ball.dir.y > 0) {
                    ball.pos.y = WORLD_H - ball_radius;
                    ball.dir.y *= -1.0f;
                }
            }
            // collision top wall (y=0, visual bottom)
            if(segments_intersect(  0.0f, ball_radius, (float)WORLD_W, ball_radius,
                                    prev_x, prev_y, ball.pos.x, ball.pos.y) != 0) {
                if(ball.dir.y < 0) {
                    ball.pos.y = ball_radius;
                    ball.dir.y *= -1.0f;
                }
            }
        }

        ball.rotation += 0.18f;
        mat4_trs(ball.transform, ball.pos.x, ball.pos.y, ball.rotation, ball.sca.x, ball.sca.y);


        // render
        glClearColor(0.1f, 0.0f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // upload the new color data
        geometry_batched.vertex_count = 0;
        geometry_batched.index_count = 0;
        geometry_batched_append(&geometry_batched, &bg);
        geometry_batched_append(&geometry_batched, &p1);
        geometry_batched_append(&geometry_batched, &p2);
        geometry_batched_append(&geometry_batched, &ball);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_data);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex_data), geometry_batched.vertex_data);

        glDrawElements(GL_TRIANGLES, INSTANCE_COUNT * INDEX_COUNT_PER_QUAD, GL_UNSIGNED_INT, 0);

        // for instance rendering
        // glBindBuffer(GL_ARRAY_BUFFER, vbo_vertex_data);
        // glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertex_data), geometry.vertex_data);

        // // upload the new matrix data
        // geometry.instance_data_count = 0;
        // geometry_instance_data_append(&geometry, bg.transform);
        // geometry_instance_data_append(&geometry, p1.transform);
        // geometry_instance_data_append(&geometry, p2.transform);
        // geometry_instance_data_append(&geometry, ball.transform);

        // glBindBuffer(GL_ARRAY_BUFFER, vbo_instance_data);
        // glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(instance_data), geometry.instance_data);

        // glDrawArraysInstanced(GL_TRIANGLES, 0, VERTEX_COUNT_PER_INSTANCE, INSTANCE_COUNT);


        glfwSwapBuffers(app.window);
    }

    window_destroy(app.window);

    return 0;
}
