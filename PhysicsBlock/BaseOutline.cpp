#include "BaseOutline.hpp"

namespace PhysicsBlock
{

    BaseOutline::BaseOutline(const unsigned int Width, const unsigned int Height) : BaseGrid(Width, Height)
    {
        int size = Width * Height * 3;
        if (size < 4){
            size = 4;
        }
        FrictionSet = new FLOAT_[size];
        OutlineSet = new Vec2_[size];
    }

    BaseOutline::~BaseOutline()
    {
        delete OutlineSet;
        delete FrictionSet;
    }

    void BaseOutline::UpdateOutline(Vec2_ CentreMass_)
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    LightweightOutlineUnit(x, y);
                }
            }
        }
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
        }
    }

    void BaseOutline::UpdateLightweightOutline(Vec2_ CentreMass_)
    {
        OutlineSize = 0;
        for (size_t x = 0; x < width; ++x)
        {
            for (size_t y = 0; y < height; ++y)
            {
                if (at(x, y).Collision)
                {
                    LightweightOutlineUnit(x, y);
                }
            }
        }
        for (size_t i = 0; i < OutlineSize; i++)
        {
            OutlineSet[i] -= CentreMass_;
        }
    }

    void BaseOutline::OutlineUnit(int x, int y)
    {
        // 左上角
        if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1) || !GetCollision(x - 1, y - 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y + 1};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右上角
        if (!(GetCollision(x + 1, y - 1) || GetCollision(x + 1, y)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 右下角
        if (!(GetCollision(x + 1, y) || GetCollision(x + 1, y + 1) || GetCollision(x, y + 1)))
        {
            OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
    }

    void BaseOutline::LightweightOutlineUnit(int x, int y)
    {
        // 左上角
        if (!GetCollision(x - 1, y - 1))
        {
            if (GetCollision(x - 1, y) == GetCollision(x, y - 1))
            {
                OutlineSet[OutlineSize] = Vec2_{x, y};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        else if (!GetCollision(x - 1, y) || !GetCollision(x, y - 1))
        {
            OutlineSet[OutlineSize] = Vec2_{x, y};
            FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
            ++OutlineSize;
        }
        // 左下角
        if (!GetCollision(x, y + 1)) {
            if (!(GetCollision(x - 1, y) && !GetCollision(x - 1, y + 1))) {
                OutlineSet[OutlineSize] = Vec2_{x, y + 1};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右上角
        if (!GetCollision(x + 1, y)) {
            if ((!GetCollision(x, y - 1) && !GetCollision(x + 1, y - 1))) {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
        // 右下角
        if (!GetCollision(x + 1, y + 1)) {
            if ((GetCollision(x + 1, y) == GetCollision(x, y + 1)) && !GetCollision(x + 1, y))
            {
                OutlineSet[OutlineSize] = Vec2_{x + 1, y + 1};
                FrictionSet[OutlineSize] = at(x, y).FrictionFactor;
                ++OutlineSize;
            }
        }
    }

}
