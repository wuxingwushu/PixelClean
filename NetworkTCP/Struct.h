#pragma once
#include "../Character/Crowd.h"
#include "../Arms/Arms.h"
#include "../ImGui/Interface.h"
#include "../GameMods/MazeMods/Labyrinth.h"

class SynchronizeClass
{
public:
	void SetArms(GAME::Arms* Arms) {
		mArms = Arms;
	}

	void SetCrowd(GAME::Crowd* Crowd) {
		mCrowd = Crowd;
	}

	void SetGamePlayer(GAME::GamePlayer* GamePlayer) {
		mGamePlayer = GamePlayer;
	}

	void SetLabyrinth(GAME::Labyrinth* Labyrinth) {
		mLabyrinth = Labyrinth;
	}

	void SetInterFace(GAME::ImGuiInterFace* interFace) {
		mInterFace = interFace;
	}

	[[nodiscard]] constexpr GAME::Arms* GetArms() const noexcept {
		return mArms;
	}

	[[nodiscard]] constexpr GAME::Crowd* GetCrowd() const noexcept {
		return mCrowd;
	}

	[[nodiscard]] constexpr GAME::GamePlayer* GetGamePlayer() const noexcept {
		return mGamePlayer;
	}

	[[nodiscard]] constexpr GAME::Labyrinth* GetLabyrinth() const noexcept {
		return mLabyrinth;
	}

	[[nodiscard]] constexpr GAME::ImGuiInterFace* GetInterFace() const noexcept {
		return mInterFace;
	}

	void AddSynchronizeMap(unsigned int I, _Synchronize synchronize) {
		mSynchronizeMap.insert(std::make_pair(I, synchronize));
	}

	_Synchronize GetSynchronizeMap(unsigned int I) {
		if (mSynchronizeMap.find(I) == mSynchronizeMap.end()) {
			return _Synchronize{ 0 };
		}
		return mSynchronizeMap[I];
	}

private:
	GAME::Arms* mArms = nullptr;//武器
	GAME::Crowd* mCrowd = nullptr;//玩家群
	GAME::GamePlayer* mGamePlayer = nullptr;//玩家
	GAME::Labyrinth* mLabyrinth = nullptr;//迷宫
	GAME::ImGuiInterFace* mInterFace = nullptr;//界面

	//标签事件储存入口数据
	std::map<unsigned int, _Synchronize> mSynchronizeMap = {};
};