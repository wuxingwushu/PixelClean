#pragma once


enum class InputState {
	None = 0,
	Down,
	Hold,
	Up
};

enum GameModsEnum
{
	Maze_ = 0,
	TankTrouble_,
	PhysicsTest_,
	SoloudTest_,
	RadianceCascades_,
	FruitNinja_,
	WFCTest_,
	BlockWorld_,
	Infinite_,
};

static const char* GameModsEnumName[] = {
	u8"迷宫",
	u8"坦克动荡",
	u8"物理测试",
	u8"SoLoud音效",
	u8"辐射级联GI",
	u8"水果忍者",
	u8"WFC测试",
	u8"Minecraft区块世界",
	u8"无限",
};

inline int GetGameModeStableId(GameModsEnum mode) {
	switch (mode) {
		case Maze_:             return 0;
		case TankTrouble_:      return 1;
		case PhysicsTest_:      return 2;
		case SoloudTest_:       return 3;
		case RadianceCascades_: return 4;
		case FruitNinja_:       return 5;
		case WFCTest_:         return 6;
		case BlockWorld_:       return 7;
		case Infinite_:         return 200;
	}
	return -1;
}

inline bool GameModsSupportsMultiplayer(GameModsEnum mode) {
	switch (mode) {
		case Maze_:           return true;
		case TankTrouble_:    return false;
		case PhysicsTest_:    return false;
		case SoloudTest_:     return false;
		case RadianceCascades_: return false;
		case FruitNinja_:     return false;
		case WFCTest_:       return false;
		case BlockWorld_:     return false;
		case Infinite_:       return false;
	}
	return false;
}

enum class GameKeyEnum : unsigned
{
	MOVE_LEFT = 0,
	MOVE_RIGHT,
	MOVE_FRONT,
	MOVE_BACK,
	MOVE_UP,
	MOVE_DOWN,
	ESC,
	Key_1,
	Key_2,
	SPACE,
};

enum class MouseBtn : unsigned
{
	Left = 0,
	Right,
	Middle,
};

enum class TouchStateEnum {
	None = 0,
	PrimaryDown,
	SecondaryDown,
	TertiaryDown,
	MultiTouch,
};

class GameMods
{
public:
	//每帧调用：鼠标位置（屏幕坐标）
	virtual void MouseMove(double xpos, double ypos) {};

	//鼠标滚轮
	virtual void MouseRoller(int z) {};

	//鼠标按键
	virtual void MouseButton(MouseBtn button, InputState State) {};

	//键盘事件
	virtual void KeyDown(GameKeyEnum moveDirection) {};

	//游戏循环
	virtual void GameLoop(unsigned int mCurrentFrame) = 0;

	//获取 CommandBuffer
	virtual void GameCommandBuffers(unsigned int Format_i) = 0;

	//录制 CommandBuffer
	virtual void GameRecordCommandBuffers() = 0;

	//游戏停止界面循环
	virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame) = 0;

	//游戏 TCP事件
	virtual void GameTCPLoop() = 0;

	//游戏界面
	virtual void GameUI() {};
};