#pragma once

#include "VBO.h"


class VBOInstanced : public VBO
{
public:
    VBOInstanced() = default;
    VBOInstanced(std::vector<glm::mat4>& mat4);
    ~VBOInstanced() { this->Destroy(); }

    VBOInstanced(const VBO&) = delete;
    VBOInstanced& operator=(const VBO&) = delete;

    VBOInstanced(const VBOInstanced&) = delete;
    VBOInstanced& operator=(const VBOInstanced&) = delete;

    VBOInstanced(VBO&&) noexcept = delete;
    VBOInstanced& operator=(VBO&&) noexcept = delete;

    VBOInstanced(VBOInstanced&&) noexcept;
    VBOInstanced& operator=(VBOInstanced&&) noexcept;

    void Initialize(std::vector<glm::mat4>& mat4);
    
    void UploadData(const void* data, GLsizeiptr size) const;
    void UploadData(const void* data, GLsizeiptr size, size_t offset) const;

private:
    void Swap(VBOInstanced& other) noexcept;

private:
    size_t nbInstances = 0;
};