#include "asset_loader.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <cstdio>

// Default texture ID (defined in main.cpp)
extern GLuint default_texture_id;

GLuint loadTexture(const std::string& path) {
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    
    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
    
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : (nrChannels == 3) ? GL_RGB : GL_RED;
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        stbi_image_free(data);
        printf("Loaded texture: %s\n", path.c_str());
    } else {
        printf("Failed to load texture: %s\n", path.c_str());
        return default_texture_id;
    }
    
    return textureID;
}

GLuint loadTextureFromMemory(unsigned char* data, unsigned int size) {
    if (!data) return default_texture_id;
    
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    
    int width, height, nrChannels;
    unsigned char* image_data = stbi_load_from_memory(data, size, &width, &height, &nrChannels, 0);
    
    if (image_data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : (nrChannels == 3) ? GL_RGB : GL_RED;
        
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        stbi_image_free(image_data);
    } else {
        printf("Failed to load texture from memory\n");
        return default_texture_id;
    }
    
    return textureID;
}

GLuint loadTextureFromARGB(aiTexel* data, unsigned int width, unsigned int height) {
    if (!data) return default_texture_id;
    
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    
    // Convert ARGB to RGBA
    unsigned char* rgba_data = new unsigned char[width * height * 4];
    for (unsigned int i = 0; i < width * height; ++i) {
        rgba_data[i * 4 + 0] = data[i].r;
        rgba_data[i * 4 + 1] = data[i].g;
        rgba_data[i * 4 + 2] = data[i].b;
        rgba_data[i * 4 + 3] = data[i].a;
    }
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    delete[] rgba_data;
    return textureID;
}

GLuint loadCubemap(const char* faces[6]) {
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    
    for (unsigned int i = 0; i < 6; ++i) {
        int width, height, nrChannels;
        unsigned char* data = stbi_load(faces[i], &width, &height, &nrChannels, 0);
        
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : (nrChannels == 3) ? GL_RGB : GL_RED;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            printf("Failed to load cubemap face: %s\n", faces[i]);
        }
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    return textureID;
}
