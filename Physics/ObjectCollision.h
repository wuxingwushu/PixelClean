#pragma once
#include "StructuralComponents.h"
#include "ObjectDecorator.h"


namespace SquarePhysics {

	class ObjectCollision :public ObjectDecorator
	{
	public:
		ObjectCollision(
			unsigned int x,
			unsigned int y,
			unsigned int SideLength);

		~ObjectCollision();

		[[nodiscard]] unsigned int GetObjectX() { return mNumberX; }

		[[nodiscard]] unsigned int GetObjectY() { return mNumberY; }

		[[nodiscard]] PixelAttribute** GetPixelAttribute() { return mPixelAttribute; }


	private:
		unsigned int mNumberX;
		unsigned int mNumberY;
		unsigned int mSideLength;
		PixelAttribute** mPixelAttribute;
	};

}
