#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Shader_m.h"
#include "philosopher.h"

// ---------- 窗口回调 ----------
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// ---------- 创建圆形 VAO（带纹理坐标） ----------
GLuint createCircleVAOWithTex(int segments, float radius) {
    std::vector<float> vertices; // x, y, u, v

    // 中心点
    vertices.push_back(0.0f); vertices.push_back(0.0f);
    vertices.push_back(0.5f); vertices.push_back(0.5f);

    for(int i=0;i<=segments;i++){
        float theta = 2.0f * M_PI * i / segments;
        float x = radius * cos(theta);
        float y = radius * sin(theta);
        float u = 0.5f + 0.5f * cos(theta);
        float v = 0.5f + 0.5f * sin(theta);
        vertices.push_back(x); vertices.push_back(y);
        vertices.push_back(u); vertices.push_back(v);
    }

    GLuint VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(float),vertices.data(),GL_STATIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    return VAO;
}

// ---------- 创建矩形 VAO ----------
GLuint createRectangleVAO(float width, float height) {
    float vertices[] = {
        -width/2,-height/2,
         width/2,-height/2,
         width/2, height/2,
        -width/2, height/2
    };
    GLuint indices[] = {0,1,2,2,3,0};
    GLuint VAO,VBO,EBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1,&EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    return VAO;
}

// ---------- 创建带纹理坐标的矩形 VAO ----------
GLuint createBillboardVAO(float width, float height) {
    float vertices[] = {
        // x, y, u, v
        -width/2, -height/2, 0.0f, 0.0f,
         width/2, -height/2, 1.0f, 0.0f,
         width/2,  height/2, 1.0f, 1.0f,
        -width/2,  height/2, 0.0f, 1.0f
    };

    GLuint indices[] = {0, 1, 2, 2, 3, 0};

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    return VAO;
}

// ---------- 加载纹理 ----------
GLuint loadTexture(const char* path){
    GLuint textureID;
    glGenTextures(1,&textureID);
    glBindTexture(GL_TEXTURE_2D,textureID);

    /* 原实现依赖默认的 unpack 对齐为 4，窄纹理会出现行尾填充导致采样失败
    // glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    */
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);  // 改为逐字节对齐，保证任意宽度图片正常上传

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    int width,height,nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path,&width,&height,&nrChannels,0);
    if(data){
        GLenum format = (nrChannels==4)?GL_RGBA:GL_RGB;
        glTexImage2D(GL_TEXTURE_2D,0,format,width,height,0,format,GL_UNSIGNED_BYTE,data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// ---------- 绘制对象 ----------
void drawObject(GLuint shader, GLuint VAO, glm::mat4 transform, bool indexed, int vertexCount, GLuint textureID=0){
    glUseProgram(shader);
    GLint transLoc = glGetUniformLocation(shader,"transform");
    if(transLoc!=-1) glUniformMatrix4fv(transLoc,1,GL_FALSE,glm::value_ptr(transform));

    bool hasTexture = textureID != 0;
    GLint hasTexLoc = glGetUniformLocation(shader,"hasTexture");
    if(hasTexLoc!=-1){
        glUniform1i(hasTexLoc, hasTexture ? 1 : 0);
    }

    GLint solidLoc = glGetUniformLocation(shader,"solidColor");
    if(solidLoc!=-1){
        if(hasTexture){
            glUniform4f(solidLoc,1.0f,1.0f,1.0f,1.0f);
        }else{
            glUniform4f(solidLoc,0.7f,0.5f,0.3f,1.0f);
        }
    }

    if(hasTexture){
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,textureID);
        GLint texLoc = glGetUniformLocation(shader,"texSampler");
        if(texLoc!=-1) glUniform1i(texLoc,0);
    }
    glBindVertexArray(VAO);
    if(indexed){
        glDrawElements(GL_TRIANGLES,vertexCount,GL_UNSIGNED_INT,0);
    } else {
        glDrawArrays(GL_TRIANGLE_FAN,0,vertexCount);
    }
    if(hasTexture) glBindTexture(GL_TEXTURE_2D,0);
}

// ---------- 占位文字渲染 ----------
void drawText(float x,float y,const std::string &text){
    // TODO: 使用 LearnOpenGL FreeType 渲染文字
}

int main(){
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800,800,"哲学家进餐模拟器",NULL,NULL);
    if(!window){ std::cerr<<"Failed to create window"<<std::endl; return -1;}
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr<<"Failed to initialize GLAD"<<std::endl;
        return -1;
    }

    Shader ourShader(
    "/home/zxy/Development/OShomeworks/philosophers_sim/shaders/basic.vs",
    "/home/zxy/Development/OShomeworks/philosophers_sim/shaders/basic.fs"
);

    const int circleSegments = 50;
    GLuint circleVAO = createCircleVAOWithTex(circleSegments,0.08f);
    GLuint rectVAO = createRectangleVAO(0.03f,0.2f);
    GLuint iconVAO = createBillboardVAO(0.18f,0.12f);

    GLuint tableTexture = loadTexture("/home/zxy/Development/OShomeworks/philosophers_sim/Images/table.jpg");
    std::vector<GLuint> philosopherTextures;
    for(int i=1;i<=5;i++){
        //std::string path = "philosopher" + std::to_string(i) + ".png";
        std::string path = "/home/zxy/Development/OShomeworks/philosophers_sim/Images/philosopher.jpeg";
        philosopherTextures.push_back(loadTexture(path.c_str()));
    }
    GLuint thinkingTexture = loadTexture("/home/zxy/Development/OShomeworks/philosophers_sim/Images/thinking.png");
    GLuint eatingTexture   = loadTexture("/home/zxy/Development/OShomeworks/philosophers_sim/Images/eating.png");

    PhilosopherManager manager(5);
    manager.start();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float rotation = 0.0f;

    std::vector<glm::vec2> chopstickPositions;
    chopstickPositions.resize(manager.getNumPhilosophers(), glm::vec2(0.0f));
    bool chopsticksInitialized = false;

    while(!glfwWindowShouldClose(window)){
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS)
            glfwSetWindowShouldClose(window,true);

        glClearColor(0.2f,0.3f,0.3f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        int n = manager.getNumPhilosophers();
        float radius = 0.75f;

        // --- 绘制桌子 ---
        glm::mat4 tableTransform = glm::mat4(1.0f);
        /* 原始桌面尺寸较小，通过缩放矩阵放大整体桌面 */
        tableTransform = glm::scale(tableTransform, glm::vec3(2.5f,2.5f,1.0f));
        drawObject(ourShader.ID,circleVAO,tableTransform,false,circleSegments+2,tableTexture);

        // --- 绘制筷子 ---
        for(int i=0;i<n;i++){
            /* 旧实现：仅根据哲学家状态粗略增加半径
            float angle = 2*M_PI*i/n + M_PI/n;
            glm::mat4 transform = glm::mat4(1.0f);
            float chopstickRadius = radius - 0.15f;
            if(manager.getPhilosopherState(i) == PhilosopherState::EATING ||
               manager.getPhilosopherState((i+n-1)%n) == PhilosopherState::EATING){
                chopstickRadius += 0.08f;
            }
            transform = glm::rotate(transform,angle,glm::vec3(0,0,1));
            transform = glm::translate(transform,glm::vec3(0,chopstickRadius,0));
            */

            float angleCurrent = 2*M_PI*i/n;
            float angleNext = 2*M_PI*((i+1)%n)/n;

            glm::vec2 posCurrent(radius*cos(angleCurrent), radius*sin(angleCurrent));
            glm::vec2 posNext(radius*cos(angleNext), radius*sin(angleNext));
            glm::vec2 defaultMid = (posCurrent + posNext) * 0.5f;

            if(!chopsticksInitialized){
                chopstickPositions[i] = defaultMid;
            }

            glm::vec2 targetPos = defaultMid;
            int owner = manager.getChopstickOwner(i);
            if(owner >= 0){
                float ownerAngle = 2*M_PI*owner/n;
                glm::vec2 ownerPos(radius*cos(ownerAngle), radius*sin(ownerAngle));
                targetPos = defaultMid + (ownerPos - defaultMid) * 0.5f;
            }

            chopstickPositions[i] += (targetPos - chopstickPositions[i]) * 0.15f;

            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(chopstickPositions[i].x, chopstickPositions[i].y, 0.0f));

            glm::vec2 toCenter = -chopstickPositions[i];
            float orientation = std::atan2(toCenter.y, toCenter.x) - glm::half_pi<float>();
            transform = glm::rotate(transform, orientation, glm::vec3(0,0,1));

            drawObject(ourShader.ID,rectVAO,transform,true,6);
        }
        chopsticksInitialized = true;

        // --- 绘制哲学家 ---
        for(int i=0;i<n;i++){
            float angle = 2*M_PI*i/n;
            glm::vec2 pos(radius*cos(angle),radius*sin(angle));
            glm::mat4 transform = glm::mat4(1.0f);
            transform = glm::translate(transform,glm::vec3(pos.x,pos.y,0.0f));
            drawObject(ourShader.ID,circleVAO,transform,false,circleSegments+2,philosopherTextures[i]);

            PhilosopherState state = manager.getPhilosopherState(i);
            GLuint stateTexture = 0;
            if(state == PhilosopherState::THINKING){
                stateTexture = thinkingTexture;
            } else if(state == PhilosopherState::EATING){
                stateTexture = eatingTexture;
            }

            if(stateTexture != 0){
                glm::vec2 direction = glm::length(pos) > 0.0f ? glm::normalize(pos) : glm::vec2(0.0f, 1.0f);
                glm::vec3 iconPos = glm::vec3(pos + direction * 0.18f, 0.0f);

                glm::mat4 iconTransform = glm::mat4(1.0f);
                iconTransform = glm::translate(iconTransform, iconPos);
                drawObject(ourShader.ID, iconVAO, iconTransform, true, 6, stateTexture);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    manager.stop();
    glDeleteVertexArrays(1,&circleVAO);
    glDeleteVertexArrays(1,&rectVAO);
    glDeleteVertexArrays(1,&iconVAO);
    glDeleteTextures(1,&thinkingTexture);
    glDeleteTextures(1,&eatingTexture);

    glfwTerminate();
    return 0;
}
