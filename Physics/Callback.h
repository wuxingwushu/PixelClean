#pragma once

namespace SquarePhysics {

	//声明
	class ObjectDecorator;
	class GridDecorator;

	//破坏模式回调函数
	typedef bool (*_DestroyModeCallback)(int x, int y, bool Bool, float Angle, ObjectDecorator* Object, GridDecorator* Grid, void* Data);

	//破坏点回调函数的类型
	typedef void (*_TerrainCollisionCallback)(int x, int y, bool Bool, ObjectDecorator* mObject, void* mclass);

}