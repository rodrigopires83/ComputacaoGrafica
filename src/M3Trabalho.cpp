/*	Aluno: Rodrigo Ferreira Pires
    Trabalho: M3
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

// Classe para representar um objeto 3D
class Objeto3D {
private:
    GLuint vaoHandle;
    GLuint textureHandle;
    int vertexCount;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

public:
    Objeto3D() : vaoHandle(0), textureHandle(0), vertexCount(0), 
                 position(0.0f), rotation(0.0f), scale(1.0f) {}
    
    void setVAO(GLuint vao) { vaoHandle = vao; }
    void setTexture(GLuint tex) { textureHandle = tex; }
    void setVertexCount(int count) { vertexCount = count; }
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setRotation(const glm::vec3& rot) { rotation = rot; }
    void setScale(const glm::vec3& scl) { scale = scl; }
    
    GLuint getVAO() const { return vaoHandle; }
    GLuint getTexture() const { return textureHandle; }
    int getVertexCount() const { return vertexCount; }
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getRotation() const { return rotation; }
    const glm::vec3& getScale() const { return scale; }
    
    void rotateX(float angle) { rotation.x += angle; }
    void rotateY(float angle) { rotation.y += angle; }
    void rotateZ(float angle) { rotation.z += angle; }
    void moveX(float delta) { position.x += delta; }
    void moveY(float delta) { position.y += delta; }
    void moveZ(float delta) { position.z += delta; }
    void scaleBy(float factor) { scale *= factor; }
};

// Configurações
const GLuint WINDOW_WIDTH = 500, WINDOW_HEIGHT = 500;
vector<Objeto3D> sceneObjects;
int currentObjectIndex = 0;

// Shaders com abordagem diferente
const char* vertexShaderCode = R"(
#version 330 core
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;
layout(location = 2) in vec2 textureCoordinate;

out vec3 fragmentColor;
out vec2 textureCoord;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

void main() {
    fragmentColor = vertexColor;
    textureCoord = textureCoordinate;
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vertexPosition, 1.0);
}
)";

const char* fragmentShaderCode = R"(
#version 330 core
in vec3 fragmentColor;
in vec2 textureCoord;
out vec4 finalColor;

uniform sampler2D textureSampler;

void main() {
    finalColor = texture(textureSampler, textureCoord);
}
)";

// Protótipos
void handleInput(GLFWwindow* window, int key, int scancode, int action, int mods);
GLuint createShaderProgram();
GLuint loadTextureFile(const char* filename);
int loadOBJModel(string filename, int& vertexCount, GLuint& textureID);

int main() {
    // Inicialização
    if (glfwInit() != GLFW_TRUE) {
        cerr << "GLFW initialization failed" << endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "M3 - Rodrigo Pires", nullptr, nullptr);
    if (!window) {
        cerr << "Window creation failed" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, handleInput);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "GLAD initialization failed" << endl;
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // Criação do programa de shader
    GLuint shaderProgram = createShaderProgram();
    
    // Localização dos uniforms
    GLint modelLoc = glGetUniformLocation(shaderProgram, "modelMatrix");
    GLint viewLoc = glGetUniformLocation(shaderProgram, "viewMatrix");
    GLint projLoc = glGetUniformLocation(shaderProgram, "projectionMatrix");

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "textureSampler"), 0);

    // Matrizes de transformação
    glm::mat4 viewMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);

    // Lista de modelos para carregar
    vector<string> modelFiles = {
        "../assets/Modelos3D/Cube.obj",
        "../assets/Modelos3D/Suzanne.obj"
    };

    // Carregamento dos objetos
    for (const string& file : modelFiles) {
        Objeto3D obj;
        GLuint textureID;
        int vertices;
        
        obj.setVAO(loadOBJModel(file, vertices, textureID));
        obj.setTexture(textureID);
        obj.setVertexCount(vertices);
        
        sceneObjects.push_back(obj);
    }

    // Loop principal
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.85f, 0.85f, 0.95f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

        // Renderização dos objetos
        for (const Objeto3D& obj : sceneObjects) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, obj.getPosition());
            modelMatrix = glm::rotate(modelMatrix, obj.getRotation().x, glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, obj.getRotation().y, glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, obj.getRotation().z, glm::vec3(0.0f, 0.0f, 1.0f));
            modelMatrix = glm::scale(modelMatrix, obj.getScale());

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, obj.getTexture());
            glBindVertexArray(obj.getVAO());
            glDrawArrays(GL_TRIANGLES, 0, obj.getVertexCount());
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}

void handleInput(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    
    Objeto3D& currentObj = sceneObjects[currentObjectIndex];

    switch (key) {
        case GLFW_KEY_TAB: 
            currentObjectIndex = (currentObjectIndex + 1) % sceneObjects.size(); 
            break;
        case GLFW_KEY_X: 
            currentObj.rotateX(glm::radians(10.0f)); 
            break;
        case GLFW_KEY_Y: 
            currentObj.rotateY(glm::radians(10.0f)); 
            break;
        case GLFW_KEY_Z: 
            currentObj.rotateZ(glm::radians(10.0f)); 
            break;
        case GLFW_KEY_W: 
            currentObj.moveZ(-0.1f); 
            break;
        case GLFW_KEY_S: 
            currentObj.moveZ(0.1f); 
            break;
        case GLFW_KEY_A: 
            currentObj.moveX(-0.1f); 
            break;
        case GLFW_KEY_D: 
            currentObj.moveX(0.1f); 
            break;
        case GLFW_KEY_Q: 
            currentObj.moveY(0.1f); 
            break;
        case GLFW_KEY_E: 
            currentObj.moveY(-0.1f); 
            break;
        case GLFW_KEY_KP_ADD: 
            currentObj.scaleBy(1.1f); 
            break;
        case GLFW_KEY_KP_SUBTRACT: 
            currentObj.scaleBy(0.9f); 
            break;
        case GLFW_KEY_ESCAPE: 
            glfwSetWindowShouldClose(window, true); 
            break;
    }
}

GLuint createShaderProgram() {
    // Vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderCode, nullptr);
    glCompileShader(vertexShader);

    // Fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderCode, nullptr);
    glCompileShader(fragmentShader);

    // Program creation and linking
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Cleanup
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

GLuint loadTextureFile(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

    if (data) {
        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        cout << "Failed to load texture: " << filename << endl;
        stbi_image_free(data);
    }

    return textureID;
}

int loadOBJModel(string filename, int& vertexCount, GLuint& textureID) {
    vector<glm::vec3> vertexPositions;
    vector<glm::vec2> textureCoordinates;
    vector<glm::vec3> normalVectors;
    vector<GLfloat> vertexBuffer;
    string mtlFilename;
    string textureFilename;

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error opening file: " << filename << endl;
        return -1;
    }

    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string token;
        iss >> token;

        if (token == "mtllib") {
            iss >> mtlFilename;
            
            // Parse MTL file
            ifstream mtlFile("../assets/Modelos3D/" + mtlFilename);
            if (mtlFile.is_open()) {
                string mtlLine;
                while (getline(mtlFile, mtlLine)) {
                    istringstream mtlIss(mtlLine);
                    string mtlToken;
                    mtlIss >> mtlToken;

                    if (mtlToken == "map_Kd") {
                        mtlIss >> textureFilename;
                    }
                }
            }
        }

        if (token == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            vertexPositions.push_back(vertex);
        } else if (token == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.x >> texCoord.y;
            textureCoordinates.push_back(texCoord);
        } else if (token == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normalVectors.push_back(normal);
        } else if (token == "f") {
            while (iss >> token) {
                int vIdx, tIdx, nIdx;
                sscanf(token.c_str(), "%d/%d/%d", &vIdx, &tIdx, &nIdx);
                vIdx--; tIdx--; nIdx--;

                glm::vec3 vertex = vertexPositions[vIdx];
                glm::vec2 texCoord = textureCoordinates[tIdx];
                texCoord.y = 1.0f - texCoord.y; // Flip Y coordinate

                // Position
                vertexBuffer.push_back(vertex.x);
                vertexBuffer.push_back(vertex.y);
                vertexBuffer.push_back(vertex.z);

                // Color (white)
                vertexBuffer.push_back(1.0f);
                vertexBuffer.push_back(1.0f);
                vertexBuffer.push_back(1.0f);

                // Texture coordinates
                vertexBuffer.push_back(texCoord.x);
                vertexBuffer.push_back(texCoord.y);
            }
        }
    }

    // Create VBO and VAO
    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(GLfloat), vertexBuffer.data(), GL_STATIC_DRAW);

    // Vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    vertexCount = vertexBuffer.size() / 8;

    // Load texture
    if (!textureFilename.empty()) {
        textureID = loadTextureFile(("../assets/Modelos3D/" + textureFilename).c_str());
    } else {
        textureID = loadTextureFile("../assets/tex/pixelWall.png");
    }

    return VAO;
}
