#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"
#include "../../PhysicsBlock/PhysicsShape.hpp"
#include <vector>
#include <array>
#include <cstdint>

namespace GAME
{

	class FruitNinja : public GameMods, Configuration
	{
	public:
		FruitNinja(Configuration wConfiguration);
		~FruitNinja();

		virtual void MouseMove(double xpos, double ypos);
		virtual void MouseRoller(int z);
		virtual void MouseButton(MouseBtn button, InputState State);
		virtual void KeyDown(GameKeyEnum moveDirection);
		virtual void GameCommandBuffers(unsigned int Format_i);
		virtual void GameLoop(unsigned int mCurrentFrame);
		virtual void GameRecordCommandBuffers();
		virtual void GameStopInterfaceLoop(unsigned int mCurrentFrame);
		virtual void GameTCPLoop();
		virtual void GameUI();

	private:
		static constexpr int PixelSize = 32;

		enum class FruitType {
			Apple = 0, Watermelon, Orange, Banana, Pineapple,
			Kiwi, Peach, Grape, Mango, DragonFruit, Count
		};

		using PixelColorGrid = std::array<std::array<uint8_t, PixelSize>, PixelSize>;

		struct FruitData {
			FruitType type;
			PhysicsBlock::PhysicsShape* body;
			bool cut;
		};

		struct HalfFruit {
			PhysicsBlock::PhysicsShape* body;
			FruitType type;
			float lifetime;
		};

		struct DebrisFragment {
			PhysicsBlock::PhysicsCircle* body;
			float lifetime;
			glm::vec4 color;
		};

		struct ParticleEffect {
			glm::vec2 pos;
			glm::vec2 velocity;
			float lifetime;
			float maxLifetime;
			glm::vec4 color;
			float size;
		};

		struct ScorePopup {
			glm::vec2 pos;
			int score;
			float lifetime;
			float maxLifetime;
		};

		enum class GameState { MainMenu, Playing, GameOver };
		enum class Difficulty { Easy, Normal, Hard };

		void InitGame();
		void ResetGame();
		void StartGame();

		void SpawnFruit(float dt);
		void UpdateFruits(float dt);
		void CheckCutting(float dt);
		void CheckMissedFruits();
		void CutFruit(size_t index, glm::vec2 cutPoint, glm::vec2 cutDirection, float cutSpeed);
		void SpawnHalfFruits(const FruitData& fruit, glm::vec2 cutPoint, glm::vec2 cutDirection, float cutSpeed);
		void UpdateHalfFruits(float dt);
		void UpdateParticles(float dt);
		void UpdateScorePopups(float dt);

		int CalculateScore(FruitType type, float cutSpeed, bool isCombo);
		void AddScore(int points, glm::vec2 pos);
		void LoseLife();

		void RenderFruits();
		void RenderHalfFruits();
		void RenderDebris();
		void RenderSwipeTrail();
		void RenderParticles();
		void RenderScorePopups();
		void RenderBoundary();

		void RenderMainMenu();
		void RenderHUD();
		void RenderGameOver();

		glm::vec4 GetFruitColor(FruitType type);
		int GetFruitBaseScore(FruitType type);
		const char* GetFruitName(FruitType type);
		bool LineCircleIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 center, float radius);

		void SpawnExplosionParticles(glm::vec2 pos, glm::vec4 color, int count, float speed);
		void SpawnFruitJuiceParticles(glm::vec2 pos, glm::vec4 color, int count);

		void SetupFruitShape(PhysicsBlock::PhysicsShape* shape, FruitType type);
		void DrawFruitPattern(PixelColorGrid& grid, FruitType type);
		glm::vec4 MapPixelColor(uint8_t cell, glm::vec4 baseColor);
		void RenderPixelFruit(const FruitData& f);
		void RenderPixelHalf(const HalfFruit& hf);
		void TryCutDuringSwipe();

		PhysicsBlock::PhysicsWorld* mPhysicsWorld = nullptr;
		VulKan::AuxiliaryVision* mAuxiliaryVision = nullptr;

		std::vector<FruitData> mFruits;
		std::vector<HalfFruit> mHalfFruits;
		std::vector<DebrisFragment> mDebris;
		std::vector<ParticleEffect> mParticles;
		std::vector<ScorePopup> mScorePopups;

		struct SwipePoint {
			glm::vec2 pos;
			float time;
		};
		std::vector<SwipePoint> mSwipeTrail;
		bool mIsSwiping = false;
		bool mLeftMouseDown = false;
		glm::vec2 mLastMouseWorldPos = {0, 0};
		size_t mLastCheckedTrailIndex = 0;

		GameState mGameState = GameState::MainMenu;
		Difficulty mDifficulty = Difficulty::Normal;
		int mScore = 0;
		int mHighScore = 0;
		int mLives = 3;
		int mCombo = 0;
		float mComboTimer = 0;
		float mGameTimeElapsed = 0;
		float mFruitSpawnTimer = 0;

		int mWinWidth = 0;
		int mWinHeight = 0;

		static constexpr float VisibleHalfWidth = 130.0f;
		static constexpr float VisibleHalfHeight = 96.0f;
		static constexpr float SpawnY = -75.0f;
		static constexpr float DeathY = -135.0f;
		static constexpr float TopBoundaryY = 108.0f;
		static constexpr float ComboTimeout = 1.0f;
		static constexpr float GameDuration = 60.0f;
		static constexpr int MaxLives = 3;
		static constexpr float SwipeTrailMaxAge = 0.35f;
		static constexpr float HalfFruitLifetime = 1.5f;

		std::chrono::steady_clock::time_point mLastFrameTime;
	};

}