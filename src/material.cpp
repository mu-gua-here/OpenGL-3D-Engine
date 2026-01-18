#include "../include/material.h"

// Function implementations
Material createDefaultMaterial() {
    Material default_mat;
    default_mat.name = "default";
    default_mat.albedo_map = default_texture_id;
    default_mat.base_color = {1.0f, 1.0f, 1.0f};
    default_mat.metallic = 0.0f;
    default_mat.roughness = 0.5f;
    default_mat.ao = 1.0f;
    return default_mat;
}
