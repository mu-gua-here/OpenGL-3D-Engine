#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdio>

class Shader {
private:
    GLuint program_id = 0;
    mutable std::unordered_map<std::string, GLint> uniform_cache;
    
public:
    Shader(const std::string& vertex_source, const std::string& fragment_source) {
        program_id = createShaderProgram(vertex_source, fragment_source);
        if (program_id == 0) {
            throw std::runtime_error("Failed to create shader program");
        }
    }
    
    ~Shader() {
        if (program_id != 0) {
            glDeleteProgram(program_id);
        }
    }
    
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    void use() const {
        glUseProgram(program_id);
    }
    
    GLuint getProgram() const { return program_id; }
    
    GLint getUniformLocation(const std::string& name) const {
        if (auto it = uniform_cache.find(name); it != uniform_cache.end()) {
            return it->second;
        }
        
        GLint location = glGetUniformLocation(program_id, name.c_str());
        uniform_cache[name] = location;
        
        if (location == -1) {
            printf("Warning: Uniform '%s' not found in shader\n", name.c_str());
        }
        
        return location;
    }
    
    void setMat4(const std::string& name, const glm::mat4& value) const {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    
    void setMat3(const std::string& name, const glm::mat3& value) const {
        glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
    
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
    }
    
    void setInt(const std::string& name, int value) const {
        glUniform1i(getUniformLocation(name), value);
    }
    
    void setFloat(const std::string& name, float value) const {
        glUniform1f(getUniformLocation(name), value);
    }
    
    void setVec3Array(const std::string& name, const std::vector<glm::vec3>& values) const {
        glUniform3fv(getUniformLocation(name), static_cast<GLsizei>(values.size()),
                     reinterpret_cast<const GLfloat*>(values.data()));
    }
    
    void setFloatArray(const std::string& name, const std::vector<float>& values) const {
        glUniform1fv(getUniformLocation(name), static_cast<GLsizei>(values.size()), values.data());
    }

private:
    GLuint createShaderProgram(const std::string& vertex_source, const std::string& fragment_source) {
        GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_source);
        if (vertex_shader == 0) return 0;
        
        GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_source);
        if (fragment_shader == 0) {
            glDeleteShader(vertex_shader);
            return 0;
        }
        
        GLuint program = linkShaderProgram(vertex_shader, fragment_shader);
        
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        
        return program;
    }
    
    GLuint compileShader(GLenum shader_type, const std::string& source) {
        GLuint shader = glCreateShader(shader_type);
        const char* source_cstr = source.c_str();
        glShaderSource(shader, 1, &source_cstr, nullptr);
        glCompileShader(shader);
        
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLchar info_log[512];
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            printf("Shader compilation failed (%s): %s\n",
                   (shader_type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT",
                   info_log);
            glDeleteShader(shader);
            return 0;
        }
        
        return shader;
    }
    
    GLuint linkShaderProgram(GLuint vertex_shader, GLuint fragment_shader) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glLinkProgram(program);
        
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLchar info_log[512];
            glGetProgramInfoLog(program, 512, nullptr, info_log);
            printf("Shader program linking failed: %s\n", info_log);
            glDeleteProgram(program);
            return 0;
        }
        
        return program;
    }
};
