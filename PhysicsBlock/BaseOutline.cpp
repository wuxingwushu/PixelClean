#include "BaseOutline.hpp"

namespace PhysicsBlock
{

    BaseOutline::BaseOutline(const unsigned int Width, const unsigned int Height) : BaseGrid(Width, Height)
    {
        OutlineSet = new glm::dvec2[Width * Height * 3];
    }

    BaseOutline::~BaseOutline()
    {
        delete OutlineSet;
    }

    void BaseOutline::UpdateOutline()
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    OutlineUnit(x, y);
                }
            }
        }
    }

    void BaseOutline::OutlineUnit(int x, int y)
    {
        // 左上角
        if (!Collision(x - 1, y) || !Collision(x, y - 1) || !Collision(x - 1, y - 1))
        {
            OutlineSet[OutlineSize] = glm::dvec2{x, y};
            ++OutlineSize;
        }
        // 左下角
        if (!Collision(x, y + 1))
        {
            OutlineSet[OutlineSize] = glm::dvec2{x, y + 1};
            ++OutlineSize;
        }
        // 右上角
        if (!(Collision(x + 1, y - 1) || Collision(x + 1, y)))
        {
            OutlineSet[OutlineSize] = glm::dvec2{x + 1, y};
            ++OutlineSize;
        }
        // 右下角
        if (!(Collision(x + 1, y) || Collision(x + 1, y + 1) || Collision(x, y + 1)))
        {
            OutlineSet[OutlineSize] = glm::dvec2{x + 1, y + 1};
            ++OutlineSize;
        }
    }

}