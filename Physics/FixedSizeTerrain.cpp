#include "FixedSizeTerrain.h"

namespace SquarePhysics {

	FixedSizeTerrain::FixedSizeTerrain(
		unsigned int x, 
		unsigned int y, 
		unsigned int SideLength
	) : GridDecorator(x, y, SideLength)
	{
	}

	FixedSizeTerrain::~FixedSizeTerrain()
	{
		for (size_t i = 0; i < mNumberX; i++)
		{
			delete[] mPixelAttributeS[i];
		}
		delete[] mPixelAttributeS;
	}

	[[nodiscard]] CollisionInfo FixedSizeTerrain::RadialCollisionDetection(glm::ivec2 Start, glm::ivec2 End)
	{
		int dx = abs(End.x - Start.x);
		int dy = abs(End.y - Start.y);
		int sx = (Start.x < End.x) ? 1 : -1;
		int sy = (Start.y < End.y) ? 1 : -1;
		int err = dx - dy;
		int e2;
		while (true) {
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			e2 = 2 * err;
			if (e2 > -dy) {
				err -= dy;
				Start.x += sx;
			}
			if (GetFixedCollisionBool(Start)) {
				return { true, Start };
			}
			if (Start.x == End.x && Start.y == End.y) {
				return { false, Start };
			}
			if (e2 < dx) {
				err += dx;
				Start.y += sy;
			}
		}
	}

}