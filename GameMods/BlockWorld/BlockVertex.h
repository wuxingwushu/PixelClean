#pragma once

#include <glm/glm.hpp>

namespace GAME {

// BlockWorld 专用顶点结构（含法线，用于光照计算）
struct BlockVertex {
    glm::vec3 Pos{};
    glm::vec4 Color{};
    glm::vec3 Normal{};
};

} // namespace GAME
