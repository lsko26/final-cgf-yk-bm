// main.cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------- Camera ----------------
struct Camera {
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 5.0f);
    float yaw = -90.0f, pitch = 0.0f;
    float speed = 2.5f, sensitivity = 0.1f;
    bool firstMouse = true;
    float lastX = 400, lastY = 300;

    glm::mat4 getView() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        return glm::lookAt(pos, pos + glm::normalize(front), glm::vec3(0, 1, 0));
    }
} camera;

void processInput(GLFWwindow* w, float dt) {
    float v = camera.speed * dt;
    glm::vec3 front(
        cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch)),
        sin(glm::radians(camera.pitch)),
        sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch))
    );
    front = glm::normalize(front);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) camera.pos += v * front;
    if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) camera.pos -= v * front;
    if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) camera.pos -= v * right;
    if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) camera.pos += v * right;
}
void mouse_callback(GLFWwindow*, double x, double y) {
    if (camera.firstMouse) { camera.lastX = x; camera.lastY = y; camera.firstMouse = false; }
    float xoff = x - camera.lastX; float yoff = camera.lastY - y;
    camera.lastX = x; camera.lastY = y;
    xoff *= camera.sensitivity; yoff *= camera.sensitivity;
    camera.yaw += xoff; camera.pitch += yoff;
    if (camera.pitch > 89) camera.pitch = 89; if (camera.pitch < -89) camera.pitch = -89;
}
void framebuffer_size_callback(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

// ---------------- Texture ----------------
GLuint loadTexture(const char* path) {
    int w, h, c; stbi_set_flip_vertically_on_load(true);
    unsigned char* d = stbi_load(path, &w, &h, &c, 4);
    if (!d) { std::cout << "Failed to load texture " << path << std::endl; return 0; }
    GLuint id; glGenTextures(1, &id); glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(d);
    std::cout << "Loaded texture: " << w << "x" << h << std::endl;
    return id;
}

// ---------------- Shaders ----------------
GLuint createShader(GLenum t, const char* s) {
    if (!s) return 0;
    GLuint sh = glCreateShader(t);
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);
    GLint ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char l[1024]; glGetShaderInfoLog(sh, 1024, nullptr, l);
        std::cout << "Shader error:\n" << l << std::endl;
    }
    return sh;
}
GLuint createProgram(const char* vs, const char* gs, const char* fs) {
    GLuint p = glCreateProgram();
    if (vs) { GLuint v = createShader(GL_VERTEX_SHADER, vs); glAttachShader(p, v); glDeleteShader(v); }
    if (gs) { GLuint g = createShader(GL_GEOMETRY_SHADER, gs); glAttachShader(p, g); glDeleteShader(g); }
    if (fs) { GLuint f = createShader(GL_FRAGMENT_SHADER, fs); glAttachShader(p, f); glDeleteShader(f); }
    glLinkProgram(p);
    return p;
}

// ---------------- Font shaders (fixed) ----------------
// Vertex shader: now passes char code and per-char offset
const char* vsrc = R"(
#version 330 core
layout(location=0) in vec3 aCenter;   // center position of the whole label
layout(location=1) in float aChar;    // ASCII code
layout(location=2) in float aOffset;  // local X offset (in character units)
out float ch;
out float off;
void main() {
    // We put the center into gl_Position; geometry shader will compute final quad
    gl_Position = vec4(aCenter, 1.0);
    ch = aChar;
    off = aOffset;
}
)";

// Geometry shader: construct the quad using camera right/up and the per-char offset
const char* gsrc = R"(
#version 330 core
layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in float ch[];
in float off[];

out vec2 uv;

uniform mat4 view;
uniform mat4 projection;
uniform float size;
uniform float cols;
uniform float rows;
uniform vec3 camPos;

void main() {
    // camera right and up extracted from view matrix (camera space to world)
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]) * size;
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]) * size;

    vec3 center = gl_in[0].gl_Position.xyz;
    float offset = off[0]; // local X offset per character (signed)
    // Optional distance-based scaling
    float dist = length(camPos - center);
    float scale = clamp(5.0 / dist, 0.45, 1.3);
    right *= scale;
    up *= scale;

    // compute UV for character in atlas
    int i = int(ch[0]) - 32;
    int col = i % int(cols);
    int row = i / int(cols);
    vec2 base = vec2(float(col) / cols, 1.0 - float(row + 1) / rows);
    vec2 step = vec2(1.0 / cols, 1.0 / rows);

    // build quad centered at `center`, shifted by right * offset
    vec3 shift = right * offset;

    vec4 p;
    p = vec4(center + shift - right * 0.5 - up * 0.5, 1.0); // bottom-left
    gl_Position = projection * view * p; uv = base; EmitVertex();

    p = vec4(center + shift + right * 0.5 - up * 0.5, 1.0); // bottom-right
    gl_Position = projection * view * p; uv = base + vec2(step.x, 0); EmitVertex();

    p = vec4(center + shift - right * 0.5 + up * 0.5, 1.0); // top-left
    gl_Position = projection * view * p; uv = base + vec2(0, step.y); EmitVertex();

    p = vec4(center + shift + right * 0.5 + up * 0.5, 1.0); // top-right
    gl_Position = projection * view * p; uv = base + step; EmitVertex();

    EndPrimitive();
}
)";

// Fragment shader: same as before
const char* fsrc = R"(
#version 330 core
in vec2 uv;
out vec4 FragColor;
uniform sampler2D fontTexture;
void main() {
    vec4 c = texture(fontTexture, uv);
    if (c.a < 0.1) discard;
    // keep sampled alpha but ensure visible color (font atlas might have grayscale)
    FragColor = vec4(c.rgb, c.a);
}
)";

// ---------------- Cube shaders ----------------
const char* cubeVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model, view, projection;
void main(){ gl_Position = projection * view * model * vec4(aPos, 1.0); }
)";

const char* cubeFS = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main(){ FragColor = vec4(color, 1.0); }
)";

// ---------------- Label helpers ----------------
struct Label {
    std::string text;
    glm::vec3 center; // center position above model
};
// makeLabelVertices now emits per-character: center.x,y,z, charCode, offsetX
// offsetX is measured in character spacing units (e.g. -1.2, -0.6, 0.0, 0.6 ...)
std::vector<float> makeLabelVertices(const Label& L) {
    std::vector<float> v;
    float spacing = 0.6f; // base spacing; geometry shader applies actual size
    int n = (int)L.text.size();
    for (int i = 0; i < n; ++i) {
        float offsetUnits = (i - (n - 1) / 2.0f) * spacing; // center the word
        v.push_back(L.center.x); // aCenter.x
        v.push_back(L.center.y); // aCenter.y
        v.push_back(L.center.z); // aCenter.z
        v.push_back((float)L.text[i]); // aChar
        v.push_back(offsetUnits); // aOffset (local units)
    }
    return v;
}

// ---------------- Main ----------------
int main() {
    // GLFW + GLAD init
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* w = glfwCreateWindow(800, 600, "Nicknames - fixed ordering", nullptr, nullptr);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(w, framebuffer_size_callback);
    glfwSetCursorPosCallback(w, mouse_callback);
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // resources
    GLuint tex = loadTexture("Libraries/Textures/font_atlas.png");
    GLuint prog = createProgram(vsrc, gsrc, fsrc);
    GLuint cubeProg = createProgram(cubeVS, nullptr, cubeFS);

    // font uniforms
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "fontTexture"), 0);
    glUniform1f(glGetUniformLocation(prog, "size"), 0.18f);
    glUniform1f(glGetUniformLocation(prog, "cols"), 16.0f);
    glUniform1f(glGetUniformLocation(prog, "rows"), 6.0f);

    // ---- Cube setup ----
    float cubeVerts[] = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,  0.5f,0.5f,-0.5f,  -0.5f,0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,
        -0.5f,-0.5f,0.5f,   0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,   0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,   -0.5f,-0.5f,0.5f,
        -0.5f,0.5f,0.5f,    -0.5f,0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f,0.5f, -0.5f,0.5f,0.5f,
        0.5f,0.5f,0.5f,     0.5f,0.5f,-0.5f,   0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,0.5f,  0.5f,0.5f,0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,  0.5f,-0.5f,0.5f,  -0.5f,-0.5f,0.5f,  -0.5f,-0.5f,-0.5f,
        -0.5f,0.5f,-0.5f,   0.5f,0.5f,-0.5f,   0.5f,0.5f,0.5f,   0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,   -0.5f,0.5f,-0.5f
    };
    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // ---- Models ----
    struct Model { glm::vec3 pos, color; std::string name; };
    std::vector<Model> models = {
        {{-2,0,-8}, {1,0,0}, "PLAYER1"},
        {{3,1,-10}, {0,1,0}, "ENEMY"},
        {{-4,-1,-6}, {0,0,1}, "ALLY"},
        {{1.5f,2.0f,-12}, {1,1,0}, "TARGET"}
    };

    // ---- Label setup (VBO layout: center.x,y,z, char, offset) ----
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // stride = 5 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); // aCenter
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // aChar
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float))); // aOffset
    glEnableVertexAttribArray(2);

    glm::mat4 projection = glm::perspective(glm::radians(60.0f), 800.f / 600.f, 0.1f, 100.f);
    float last = 0;

    while (!glfwWindowShouldClose(w)) {
        float now = glfwGetTime(); float dt = now - last; last = now;
        processInput(w, dt);
        glClearColor(0.05f, 0.05f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 view = camera.getView();

        // Animate models slightly
        for (auto& m : models)
            m.pos.y = 0.5f * sin(now + m.pos.x);

        // Draw cubes
        glUseProgram(cubeProg);
        for (auto& m : models) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), m.pos);
            glUniformMatrix4fv(glGetUniformLocation(cubeProg, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(cubeProg, "view"), 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(cubeProg, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
            glUniform3fv(glGetUniformLocation(cubeProg, "color"), 1, glm::value_ptr(m.color));
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // Build label vertex data: center + char + offset
        std::vector<float> verts;
        for (auto& m : models) {
            Label L = { m.name, m.pos + glm::vec3(0, 1.2f, 0) };
            auto lv = makeLabelVertices(L);
            verts.insert(verts.end(), lv.begin(), lv.end());
        }
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

        // Draw labels
        glUseProgram(prog);
        glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(prog, "camPos"), 1, glm::value_ptr(camera.pos));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glBindVertexArray(VAO);
        // number of points = verts.size() / 5
        glDrawArrays(GL_POINTS, 0, (GLsizei)(verts.size() / 5));

        glfwSwapBuffers(w); glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
