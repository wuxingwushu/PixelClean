#pragma once
#include "../Configuration.h"
#include "../GameMods.h"
#include "../../PhysicsBlock/PhysicsWorld.hpp"
#include "../../PhysicsBlock/MapStatic.hpp"
#include <vector>

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
		enum class FruitType {
			Apple = 0, Watermelon, Orange, Banana, Pineapple,
			Kiwi, Peach, Grape, Mango, DragonFruit, Count
		};

		struct FruitData {
			FruitType type;
			PhysicsBlock::PhysicsCircle* body;
			bool cut;
		};

		struct CutFragment {
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

		void SpawnFruit();
		void UpdateFruits(float dt);
		void CheckCutting(float dt);
		void CheckMissedFruits();
		void CutFruit(size_t index, glm::vec2 cutPoint, glm::vec2 cutDirection, float cutSpeed);
		void SpawnCutFragments(glm::vec2 pos, glm::vec2 direction, const FruitData& fruit, float cutSpeed);
		void UpdateFragments(float dt);
		void UpdateParticles(float dt);
		void UpdateScorePopups(float dt);

		int CalculateScore(FruitType type, float cutSpeed, bool isCombo);
		void AddScore(int points, glm::vec2 pos);
		void LoseLife();

		void RenderFruits();
		void RenderFragments();
		void RenderSwipeTrail();
		void RenderParticles();
		void RenderScorePopups();
		void RenderBoundary();

		void RenderMainMenu();
		void RenderHUD();
		void RenderGameOver();

		glm::vec4 GetFruitColor(FruitType type);
		float GetFruitRadius(FruitType type);
		int GetFruitBaseScore(FruitType type);
		const char* GetFruitName(FruitType type);
		bool LineCircleIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 center, float radius);

		void SpawnExplosionParticles(glm::vec2 pos, glm::vec4 color, int count, float speed);
		void SpawnFruitJuiceParticles(glm::vec2 pos, glm::vec4 color, int count);

		PhysicsBlock::PhysicsWorld* mPhysicsWorld = nullptr;
		VulKan::AuxiliaryVision* mAuxiliaryVision = nullptr;

		std::vector<FruitData> mFruits;
		std::vector<CutFragment> mFragments;
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

		static constexpr float VisibleHalfWidth = 12.0f;
		static constexpr float VisibleHalfHeight = 9.0f;
		static constexpr float SpawnY = -7.0f;
		static constexpr float DeathY = -11.0f;
		static constexpr float TopBoundaryY = 10.0f;
		static constexpr float ComboTimeout = 1.0f;
		static constexpr float GameDuration = 60.0f;
		static constexpr int MaxLives = 3;
		static constexpr float SwipeTrailMaxAge = 0.15f;

		std::chrono::steady_clock::time_point mLastFrameTime;
	};

}