/*	Aluno: Rodrigo Ferreira Pires
    Trabalho: M4
	Disciplina: Computação Gráfica
	Professora: Rossana B Queiroz
*/

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

// Estrutura para objeto 3D
struct Objeto3D {
    GLuint vao;
    GLuint textura;
    int nVertices;
    glm::vec3 pos{0.0f};
    glm::vec3 rot{0.0f};
    glm::vec3 escala{1.0f};
};

vector<Objeto3D> cena;
int objetoAtual = 0;

const GLuint WIDTH = 800, HEIGHT = 800;

// Vertex Shader com Phong
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 cor;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vNormal;
out vec3 vFragPos;
out vec3 vColor;
out vec2 vTexCoord;

void main() {
    vFragPos = vec3(model * vec4(pos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * normal;
    vColor = cor;
    vTexCoord = texCoord;
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
)";

// Fragment Shader com Phong
const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vNormal;
in vec3 vFragPos;
in vec3 vColor;
in vec2 vTexCoord;

out vec4 FragColor;

uniform sampler2D tex;
uniform vec3 lightPos;
uniform vec3 camPos;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float ns;

void main() {
    vec3 lightColor = vec3(1.0);
    vec3 ambient = ka * lightColor;

    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    vec3 viewDir = normalize(camPos - vFragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), ns);
    vec3 specular = ks * spec * lightColor;

    vec3 texColor = texture(tex, vTexCoord).rgb;
    vec3 result = (ambient + diffuse + specular) * texColor;
    FragColor = vec4(result, 1.0);
}
)";

// Protótipos
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint criarShader();
GLuint carregarTextura(const char* caminho);
int carregarOBJ(const string& objPath, int& nVertices, GLuint& texID, float& ka, float& kd, float& ks, float& ns);

int main() {
    if (!glfwInit()) {
        cerr << "Erro ao inicializar GLFW" << endl;
        return -1;
    }
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "M4 - Rodrigo Pires", nullptr, nullptr);
    if (!window) {
        cerr << "Erro ao criar janela" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Erro ao inicializar GLAD" << endl;
        return -1;
    }
    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shader = criarShader();
    glUseProgram(shader);

    // Carregar apenas Suzanne
    int nVertices; GLuint texID;
    float ka, kd, ks, ns;
    GLuint vao = carregarOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, texID, ka, kd, ks, ns);
    cena.push_back({vao, texID, nVertices});

    // Uniforms fixos
    glm::vec3 lightPos(2.0f, 2.0f, 2.0f);
    glm::vec3 camPos(0.0f, 0.0f, 3.0f);
    glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, &lightPos[0]);
    glUniform3fv(glGetUniformLocation(shader, "camPos"), 1, &camPos[0]);
    glUniform1f(glGetUniformLocation(shader, "ka"), ka);
    glUniform1f(glGetUniformLocation(shader, "kd"), kd);
    glUniform1f(glGetUniformLocation(shader, "ks"), ks);
    glUniform1f(glGetUniformLocation(shader, "ns"), ns);
    glUniform1i(glGetUniformLocation(shader, "tex"), 0);

    // Matrizes
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH/HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0,1,0));
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);
        for (const auto& obj : cena) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, obj.pos);
            model = glm::rotate(model, obj.rot.x, glm::vec3(1,0,0));
            model = glm::rotate(model, obj.rot.y, glm::vec3(0,1,0));
            model = glm::rotate(model, obj.rot.z, glm::vec3(0,0,1));
            model = glm::scale(model, obj.escala);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, obj.textura);
            glBindVertexArray(obj.vao);
            glDrawArrays(GL_TRIANGLES, 0, obj.nVertices);
        }
        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    Objeto3D& obj = cena[objetoAtual];
    switch (key) {
        case GLFW_KEY_X: obj.rot.x += glm::radians(10.0f); break;
        case GLFW_KEY_Y: obj.rot.y += glm::radians(10.0f); break;
        case GLFW_KEY_Z: obj.rot.z += glm::radians(10.0f); break;
        case GLFW_KEY_W: obj.pos.z -= 0.1f; break;
        case GLFW_KEY_S: obj.pos.z += 0.1f; break;
        case GLFW_KEY_A: obj.pos.x -= 0.1f; break;
        case GLFW_KEY_D: obj.pos.x += 0.1f; break;
        case GLFW_KEY_Q: obj.pos.y += 0.1f; break;
        case GLFW_KEY_E: obj.pos.y -= 0.1f; break;
        case GLFW_KEY_KP_ADD: obj.escala *= 1.1f; break;
        case GLFW_KEY_KP_SUBTRACT: obj.escala *= 0.9f; break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); break;
    }
}

GLuint criarShader() {
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vertexShaderSource, nullptr);
    glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fragmentShaderSource, nullptr);
    glCompileShader(f);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

GLuint carregarTextura(const char* caminho) {
    GLuint texID;
    glGenTextures(1, &texID);
    int w, h, c;
    unsigned char* data = stbi_load(caminho, &w, &h, &c, 0);
    if (data) {
        GLenum format = (c == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        cout << "Falha ao carregar textura: " << caminho << endl;
        stbi_image_free(data);
    }
    return texID;
}

int carregarOBJ(const string& objPath, int& nVertices, GLuint& texID, float& ka, float& kd, float& ks, float& ns) {
    vector<glm::vec3> pos;
    vector<glm::vec3> norm;
    vector<glm::vec2> tex;
    vector<GLfloat> buffer;
    string mtlFile, texFile;
    ka = 0.1f; kd = 0.7f; ks = 0.5f; ns = 32.0f;
    ifstream arq(objPath);
    if (!arq.is_open()) return -1;
    string line;
    while (getline(arq, line)) {
        istringstream iss(line);
        string t; iss >> t;
        if (t == "mtllib") {
            iss >> mtlFile;
            ifstream mtl("../assets/Modelos3D/" + mtlFile);
            if (mtl.is_open()) {
                string l;
                while (getline(mtl, l)) {
                    istringstream mtliss(l);
                    string tk; mtliss >> tk;
                    if (tk == "map_Kd") mtliss >> texFile;
                    else if (tk == "Ka") mtliss >> ka;
                    else if (tk == "Kd") mtliss >> kd;
                    else if (tk == "Ks") mtliss >> ks;
                    else if (tk == "Ns") mtliss >> ns;
                }
            }
        } else if (t == "v") {
            glm::vec3 v; iss >> v.x >> v.y >> v.z; pos.push_back(v);
        } else if (t == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z; norm.push_back(n);
        } else if (t == "vt") {
            glm::vec2 vt; iss >> vt.x >> vt.y; tex.push_back(vt);
        } else if (t == "f") {
            for (int i = 0; i < 3; ++i) {
                string f; iss >> f;
                int vi, ti, ni;
                sscanf(f.c_str(), "%d/%d/%d", &vi, &ti, &ni);
                vi--; ti--; ni--;
                glm::vec3 v = pos[vi];
                glm::vec3 n = norm[ni];
                glm::vec2 t = tex[ti];
                t.y = 1.0f - t.y;
                buffer.push_back(v.x); buffer.push_back(v.y); buffer.push_back(v.z);
                buffer.push_back(1.0f); buffer.push_back(1.0f); buffer.push_back(1.0f);
                buffer.push_back(n.x); buffer.push_back(n.y); buffer.push_back(n.z);
                buffer.push_back(t.x); buffer.push_back(t.y);
            }
        }
    }
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(GLfloat), buffer.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(9 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    glBindVertexArray(0);
    nVertices = buffer.size() / 11;
    if (!texFile.empty()) texID = carregarTextura(("../assets/Modelos3D/" + texFile).c_str());
    else texID = carregarTextura("../assets/tex/pixelWall.png");
    return VAO;
}
