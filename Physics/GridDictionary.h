#pragma once
#include "ObjectDecorator.h"
#include <vector>

namespace SquarePhysics {

	class GridDictionary
	{
	private:
		unsigned int mNumberX;
		unsigned int mNumberY;
		//ObjectCollision
		//PixelCollision
		//ObjectDecorator
		std::vector<std::vector<ObjectDecorator*>> mGridDictionary;
	public:
		GridDictionary(unsigned int x, unsigned int y)
			:mNumberX(x), mNumberY(y)
		{
			mGridDictionary.resize(mNumberX * mNumberY);
		}

		~GridDictionary() {

		}

		inline std::vector<ObjectDecorator*> GetObjectDecorator(unsigned int x, unsigned int y) noexcept {
			return mGridDictionary[x * mNumberY + y];
		}
	};

}