/*	Aluno: Rodrigo Ferreira Pires
    Trabalho: M6
	Disciplina: Computação Gráfica
	Professora: Rossana B Queiroz
*/

/*
CONTROLES DE TRAJETÓRIA:
- T: Ativar/Desativar modo de edição de trajetória
- ESPAÇO: Adicionar ponto de controle na posição atual da câmera (apenas no modo edição)
- P: Ativar/Desativar trajetória do objeto selecionado
- C: Limpar todos os pontos de controle do objeto selecionado
- V: Mostrar/Ocultar pontos de controle visuais
- TAB: Alternar entre objetos da cena
- F5: Salvar trajetória do objeto atual em arquivo
- F9: Carregar trajetória do objeto atual de arquivo

CONTROLES DE CÂMERA:
- WASD: Mover câmera horizontalmente
- Q/E: Mover câmera verticalmente
- Mouse: Rotacionar câmera

CONTROLES DE OBJETO:
- X/Y/Z: Rotacionar objeto nos eixos X, Y, Z
- +/- (teclado numérico): Escalar objeto
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
    
    // Sistema de trajetória
    vector<glm::vec3> pontosControle;
    int pontoAtual;
    float tempoTrajetoria;
    bool trajetoriaAtiva;
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

// Classe de câmera em primeira pessoa
class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    float yaw, pitch;
    float speed;
    float sensitivity;

    Camera(glm::vec3 startPos)
        : position(startPos), worldUp(0.0f, 1.0f, 0.0f), yaw(-90.0f), pitch(0.0f), speed(2.5f), sensitivity(0.1f)
    {
        updateVectors();
    }

    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(char direction, float deltaTime) {
        float velocity = speed * deltaTime;
        if (direction == 'W') position += front * velocity;
        if (direction == 'S') position -= front * velocity;
        if (direction == 'A') position -= right * velocity;
        if (direction == 'D') position += right * velocity;
        if (direction == 'Q') position += worldUp * velocity;
        if (direction == 'E') position -= worldUp * velocity;
    }

    void processMouse(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw += xoffset;
        pitch += yoffset;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }

private:
    void updateVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

// Variáveis globais para controle de tempo e mouse
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.016f;
float lastFrame = 0.0f;

// Modo de edição de trajetória
bool modoEdicaoTrajetoria = false;
bool mostrarPontosControle = true;

// Protótipos
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
GLuint criarShader();
GLuint carregarTextura(const char* caminho);
int carregarOBJ(const string& objPath, int& nVertices, GLuint& texID, float& ka, float& kd, float& ks, float& ns);

// Funções de trajetória
void adicionarPontoControle(Objeto3D& obj, const glm::vec3& ponto);
void atualizarTrajetoria(Objeto3D& obj, float deltaTime);
void salvarTrajetoria(const Objeto3D& obj, const string& nomeArquivo);
void carregarTrajetoria(Objeto3D& obj, const string& nomeArquivo);
void desenharPontosControle(const vector<glm::vec3>& pontos);

int main() {
    if (!glfwInit()) {
        cerr << "Erro ao inicializar GLFW" << endl;
        return -1;
    }
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "M4 - Phong", nullptr, nullptr);
    if (!window) {
        cerr << "Erro ao criar janela" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Erro ao inicializar GLAD" << endl;
        return -1;
    }
    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shader = criarShader();
    glUseProgram(shader);

    // Carregar Suzanne
    int nVertices; GLuint texID;
    float ka, kd, ks, ns;
    GLuint vao = carregarOBJ("../assets/Modelos3D/Suzanne.obj", nVertices, texID, ka, kd, ks, ns);
    cena.push_back({vao, texID, nVertices});
    
    // Inicializar variáveis de trajetória
    cena[0].pontoAtual = 0;
    cena[0].tempoTrajetoria = 0.0f;
    cena[0].trajetoriaAtiva = false;

    // Uniforms fixos
    glm::vec3 lightPos(2.0f, 2.0f, 2.0f);
    glUniform3fv(glGetUniformLocation(shader, "lightPos"), 1, &lightPos[0]);
    glUniform1f(glGetUniformLocation(shader, "ka"), ka);
    glUniform1f(glGetUniformLocation(shader, "kd"), kd);
    glUniform1f(glGetUniformLocation(shader, "ks"), ks);
    glUniform1f(glGetUniformLocation(shader, "ns"), ns);
    glUniform1i(glGetUniformLocation(shader, "tex"), 0);

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)WIDTH/HEIGHT, 0.1f, 100.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        glfwPollEvents();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);
        glm::mat4 view = camera.getViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        
        // Atualizar trajetórias
        for (auto& obj : cena) {
            if (obj.trajetoriaAtiva && !obj.pontosControle.empty()) {
                atualizarTrajetoria(obj, deltaTime);
            }
        }
        
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
        
        // Desenhar pontos de controle se estiver no modo de edição
        if (modoEdicaoTrajetoria && mostrarPontosControle && !cena[objetoAtual].pontosControle.empty()) {
            desenharPontosControle(cena[objetoAtual].pontosControle);
        }
        
        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    Objeto3D& obj = cena[objetoAtual];
    switch (key) {
        // Controles de câmera
        case GLFW_KEY_W: camera.processKeyboard('W', deltaTime); break;
        case GLFW_KEY_S: camera.processKeyboard('S', deltaTime); break;
        case GLFW_KEY_A: camera.processKeyboard('A', deltaTime); break;
        case GLFW_KEY_D: camera.processKeyboard('D', deltaTime); break;
        case GLFW_KEY_Q: camera.processKeyboard('Q', deltaTime); break;
        case GLFW_KEY_E: camera.processKeyboard('E', deltaTime); break;
        
        // Controles de objeto (rotação e escala)
        case GLFW_KEY_X: obj.rot.x += glm::radians(10.0f); break;
        case GLFW_KEY_Y: obj.rot.y += glm::radians(10.0f); break;
        case GLFW_KEY_Z: obj.rot.z += glm::radians(10.0f); break;
        case GLFW_KEY_KP_ADD: obj.escala *= 1.1f; break;
        case GLFW_KEY_KP_SUBTRACT: obj.escala *= 0.9f; break;
        
        // Controles de trajetória
        case GLFW_KEY_T: 
            modoEdicaoTrajetoria = !modoEdicaoTrajetoria;
            cout << "Modo edição trajetória: " << (modoEdicaoTrajetoria ? "ATIVADO" : "DESATIVADO") << endl;
            break;
        case GLFW_KEY_SPACE:
            if (modoEdicaoTrajetoria) {
                adicionarPontoControle(obj, camera.position);
                cout << "Ponto adicionado na posição: (" << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << ")" << endl;
            }
            break;
        case GLFW_KEY_P:
            obj.trajetoriaAtiva = !obj.trajetoriaAtiva;
            cout << "Trajetória: " << (obj.trajetoriaAtiva ? "ATIVADA" : "DESATIVADA") << endl;
            break;
        case GLFW_KEY_C:
            obj.pontosControle.clear();
            obj.pontoAtual = 0;
            obj.tempoTrajetoria = 0.0f;
            cout << "Pontos de controle limpos" << endl;
            break;
        case GLFW_KEY_V:
            mostrarPontosControle = !mostrarPontosControle;
            break;
        
        // Navegação entre objetos
        case GLFW_KEY_TAB:
            objetoAtual = (objetoAtual + 1) % cena.size();
            cout << "Objeto selecionado: " << objetoAtual << endl;
            break;
        
        // Salvar/Carregar trajetória
        case GLFW_KEY_F5:
            salvarTrajetoria(obj, "trajetoria_" + to_string(objetoAtual) + ".txt");
            break;
        case GLFW_KEY_F9:
            carregarTrajetoria(obj, "trajetoria_" + to_string(objetoAtual) + ".txt");
            break;
        
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

// Callback de mouse
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // invertido
    lastX = xpos;
    lastY = ypos;
    camera.processMouse(xoffset, yoffset);
}

// Funções de trajetória
void adicionarPontoControle(Objeto3D& obj, const glm::vec3& ponto) {
    obj.pontosControle.push_back(ponto);
    cout << "Ponto " << obj.pontosControle.size() << " adicionado" << endl;
}

void atualizarTrajetoria(Objeto3D& obj, float deltaTime) {
    if (obj.pontosControle.size() < 2) return;
    
    const float velocidade = 2.0f; // unidades por segundo
    obj.tempoTrajetoria += deltaTime * velocidade;
    
    // Calcular posição atual na trajetória
    float tempoTotal = obj.pontosControle.size();
    float tempoNormalizado = fmod(obj.tempoTrajetoria, tempoTotal);
    
    int indiceAtual = static_cast<int>(tempoNormalizado);
    int indiceProximo = (indiceAtual + 1) % obj.pontosControle.size();
    
    float t = tempoNormalizado - indiceAtual;
    
    // Interpolação linear entre pontos
    glm::vec3 posAtual = obj.pontosControle[indiceAtual];
    glm::vec3 posProximo = obj.pontosControle[indiceProximo];
    obj.pos = glm::mix(posAtual, posProximo, t);
}

void salvarTrajetoria(const Objeto3D& obj, const string& nomeArquivo) {
    ofstream arquivo(nomeArquivo);
    if (arquivo.is_open()) {
        for (const auto& ponto : obj.pontosControle) {
            arquivo << ponto.x << " " << ponto.y << " " << ponto.z << endl;
        }
        arquivo.close();
        cout << "Trajetória salva em: " << nomeArquivo << endl;
    } else {
        cout << "Erro ao salvar trajetória" << endl;
    }
}

void carregarTrajetoria(Objeto3D& obj, const string& nomeArquivo) {
    ifstream arquivo(nomeArquivo);
    if (arquivo.is_open()) {
        obj.pontosControle.clear();
        float x, y, z;
        while (arquivo >> x >> y >> z) {
            obj.pontosControle.push_back(glm::vec3(x, y, z));
        }
        arquivo.close();
        obj.pontoAtual = 0;
        obj.tempoTrajetoria = 0.0f;
        cout << "Trajetória carregada de: " << nomeArquivo << " (" << obj.pontosControle.size() << " pontos)" << endl;
    } else {
        cout << "Erro ao carregar trajetória: " << nomeArquivo << endl;
    }
}

void desenharPontosControle(const vector<glm::vec3>& pontos) {
    // Desabilitar textura para desenhar pontos
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Desenhar linhas conectando os pontos
    if (pontos.size() > 1) {
        glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 1.0f, 0.0f); // Verde para linhas
        for (const auto& ponto : pontos) {
            glVertex3f(ponto.x, ponto.y, ponto.z);
        }
        // Fechar o loop conectando o último ponto ao primeiro
        if (pontos.size() > 2) {
            glVertex3f(pontos.front().x, pontos.front().y, pontos.front().z);
        }
        glEnd();
    }
    
    // Desenhar cada ponto como um pequeno cubo
    for (const auto& ponto : pontos) {
        glPushMatrix();
        glTranslatef(ponto.x, ponto.y, ponto.z);
        glColor3f(1.0f, 0.0f, 0.0f); // Vermelho para pontos
        
        // Desenhar um pequeno cubo
        float size = 0.05f;
        glBegin(GL_QUADS);
        // Frente
        glVertex3f(-size, -size, size);
        glVertex3f(size, -size, size);
        glVertex3f(size, size, size);
        glVertex3f(-size, size, size);
        // Traseira
        glVertex3f(-size, -size, -size);
        glVertex3f(-size, size, -size);
        glVertex3f(size, size, -size);
        glVertex3f(size, -size, -size);
        // Topo
        glVertex3f(-size, size, -size);
        glVertex3f(-size, size, size);
        glVertex3f(size, size, size);
        glVertex3f(size, size, -size);
        // Base
        glVertex3f(-size, -size, -size);
        glVertex3f(size, -size, -size);
        glVertex3f(size, -size, size);
        glVertex3f(-size, -size, size);
        // Direita
        glVertex3f(size, -size, -size);
        glVertex3f(size, size, -size);
        glVertex3f(size, size, size);
        glVertex3f(size, -size, size);
        // Esquerda
        glVertex3f(-size, -size, -size);
        glVertex3f(-size, -size, size);
        glVertex3f(-size, size, size);
        glVertex3f(-size, size, -size);
        glEnd();
        
        glPopMatrix();
    }
    
    glColor3f(1.0f, 1.0f, 1.0f); // Restaurar cor branca
}
