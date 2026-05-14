#include "FruitNinja.h"
#include "../../NetworkTCP/Server.h"
#include "../../NetworkTCP/Client.h"
#include "../../PhysicsBlock/BaseCalculate.hpp"
#include "../../SoundEffect/SoundEffect.h"
#include "../../GlobalVariable.h"
#include "../../DebugLog.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace GAME
{

	FruitNinja::FruitNinja(Configuration wConfiguration) : Configuration{wConfiguration}
	{
		LOGD("FruitNinja::FruitNinja constructor");
		mAuxiliaryVision = new VulKan::AuxiliaryVision(mDevice, mPipelineS, 1000);
		mAuxiliaryVision->initUniformManager(
			mSwapChain->getImageCount(),
			mCameraVPMatricesBuffer);
		mAuxiliaryVision->RecordingCommandBuffer(mRenderPass, mSwapChain);

		InitGame();
		RenderBoundary();

		mCamera->setCameraPos({0, 0, 28});
		mCamera->update();

		mLastFrameTime = std::chrono::steady_clock::now();
	}

	FruitNinja::~FruitNinja()
	{
		LOGD("FruitNinja::~FruitNinja destructor");
		delete mAuxiliaryVision;
		if (mPhysicsWorld) {
			delete mPhysicsWorld;
			mPhysicsWorld = nullptr;
		}
	}

	void FruitNinja::InitGame()
	{
		if (mPhysicsWorld) {
			delete mPhysicsWorld;
		}
		mPhysicsWorld = new PhysicsBlock::PhysicsWorld({0.0, -40.0}, false);

		PhysicsBlock::MapStatic* map = new PhysicsBlock::MapStatic(20, 20);
		for (int i = 0; i < 20 * 20; ++i) {
			map->at(i).Entity = false;
			map->at(i).Collision = false;
			map->at(i).mass = 1.0;
		}
		map->SetCentrality({10.0, 10.0});
		mPhysicsWorld->SetMapFormwork(map);

		ResetGame();
	}

	void FruitNinja::ResetGame()
	{
		for (auto& f : mFruits) {
			if (f.body && !f.cut) {
				mPhysicsWorld->RemoveObject(f.body);
			}
		}
		for (auto& frag : mFragments) {
			if (frag.body) {
				mPhysicsWorld->RemoveObject(frag.body);
			}
		}
		mFruits.clear();
		mFragments.clear();
		mParticles.clear();
		mScorePopups.clear();
		mSwipeTrail.clear();

		mScore = 0;
		mLives = MaxLives;
		mCombo = 0;
		mComboTimer = 0;
		mGameTimeElapsed = 0;
		mFruitSpawnTimer = 0;
		mIsSwiping = false;
		mLeftMouseDown = false;
	}

	void FruitNinja::StartGame()
	{
		ResetGame();
		mGameState = GameState::Playing;
		mLastFrameTime = std::chrono::steady_clock::now();

		SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, false, Global::SoundEffectsVolume * 0.3f, 0.0f);
	}

	void FruitNinja::MouseMove(double xpos, double ypos)
	{
		CursorPosX = xpos;
		CursorPosY = ypos;
#if defined(__ANDROID__)
		mWinWidth = mWindow->getWidth();
		mWinHeight = mWindow->getHeight();
#else
		glfwGetWindowSize(mWindow->getWindow(), &mWinWidth, &mWinHeight);
#endif

		if (mGameState != GameState::Playing) return;

		int winwidth = mWinWidth, winheight = mWinHeight;
		glm::vec3 ray = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		ray *= -mCamera->getCameraPos().z / ray.z;
		glm::vec2 worldPos(ray.x + mCamera->getCameraPos().x, ray.y + mCamera->getCameraPos().y);

		if (mIsSwiping) {
			mSwipeTrail.push_back({worldPos, (float)std::chrono::duration<float>(
				std::chrono::steady_clock::now().time_since_epoch()).count()});
		}

		mLastMouseWorldPos = worldPos;
	}

	void FruitNinja::MouseButton(MouseBtn button, InputState State)
	{
		if (mGameState != GameState::Playing) return;

		switch (button) {
		case MouseBtn::Left:
			if (State == InputState::Down) {
				mLeftMouseDown = true;
				mIsSwiping = true;
				mSwipeTrail.clear();
				mSwipeTrail.push_back({mLastMouseWorldPos, (float)std::chrono::duration<float>(
					std::chrono::steady_clock::now().time_since_epoch()).count()});
			}
			else if (State == InputState::Up) {
				mLeftMouseDown = false;
				mIsSwiping = false;
				static float lastFrameTime = 0;
				float now = (float)std::chrono::duration<float>(
					std::chrono::steady_clock::now().time_since_epoch()).count();
				float dt = now - lastFrameTime;
				lastFrameTime = now;
				CheckCutting(dt > 0 ? dt : 0.016f);
			}
			break;
		case MouseBtn::Right:
			break;
		default:
			break;
		}
	}

	void FruitNinja::MouseRoller(int z)
	{
		if (Global::ClickWindow) return;

		glm::vec3 camPos = mCamera->getCameraPos();
		if (camPos.z <= 10) {
			camPos.z += (camPos.z / 2) * z;
		}
		else {
			camPos.z += z * 5;
		}
		if (camPos.z <= 0.1f) camPos.z = 0.1f;
		if (camPos.z > 80.0f) camPos.z = 80.0f;
		mCamera->setCameraPos(camPos);
		mCamera->update();
	}

	void FruitNinja::KeyDown(GameKeyEnum moveDirection)
	{
		switch (moveDirection)
		{
		case GameKeyEnum::ESC:
			if (Global::ConsoleBool) {
				Global::ConsoleBool = false;
				InterFace->ConsoleFocusHere = true;
			}
			else {
				InterFace->SetInterFaceBool();
			}
			break;
		case GameKeyEnum::SPACE:
			if (mGameState == GameState::MainMenu) {
				StartGame();
			}
			else if (mGameState == GameState::GameOver) {
				mGameState = GameState::MainMenu;
			}
			break;
		default:
			break;
		}
	}

	void FruitNinja::GameCommandBuffers(unsigned int Format_i)
	{
		mAuxiliaryVision->GetCommandBuffer(wThreadCommandBufferS, Format_i);
	}

	void FruitNinja::GameLoop(unsigned int mCurrentFrame)
	{
		auto now = std::chrono::steady_clock::now();
		float dt = std::chrono::duration<float>(now - mLastFrameTime).count();
		mLastFrameTime = now;
		dt = glm::clamp(dt, 0.005f, 0.05f);

		mAuxiliaryVision->Begin();

		if (mGameState == GameState::Playing) {
			mGameTimeElapsed += dt;

			if (mGameTimeElapsed >= GameDuration) {
				mGameState = GameState::GameOver;
				if (mScore > mHighScore) mHighScore = mScore;
				SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, false, Global::SoundEffectsVolume * 0.5f, 0.0f);
			}

			UpdateFruits(dt);
			UpdateFragments(dt);
			UpdateParticles(dt);
			UpdateScorePopups(dt);

			mComboTimer -= dt;
			if (mComboTimer <= 0) {
				mCombo = 0;
			}

			if (mIsSwiping) {
				float nowTime = (float)std::chrono::duration<float>(
					std::chrono::steady_clock::now().time_since_epoch()).count();
				while (!mSwipeTrail.empty() && (nowTime - mSwipeTrail.front().time) > SwipeTrailMaxAge) {
					mSwipeTrail.erase(mSwipeTrail.begin());
				}
			}

			CheckMissedFruits();
			RenderFruits();
			RenderFragments();
			RenderSwipeTrail();
			RenderParticles();
			RenderScorePopups();

			mPhysicsWorld->PhysicsEmulator(dt);
		}
		else if (mGameState == GameState::MainMenu) {
			mPhysicsWorld->PhysicsInformationUpdate();
		}
		else if (mGameState == GameState::GameOver) {
			mPhysicsWorld->PhysicsInformationUpdate();
			UpdateParticles(dt);
			UpdateScorePopups(dt);
			UpdateFragments(dt);
			RenderParticles();
			RenderScorePopups();
			RenderFragments();
		}

		mAuxiliaryVision->End();

		mCamera->update();
		VPMatrices* mVPMatrices = (VPMatrices*)mCameraVPMatricesBuffer[mCurrentFrame]->getupdateBufferByMap();
		mVPMatrices->mViewMatrix = mCamera->getViewMatrix();
		mCameraVPMatricesBuffer[mCurrentFrame]->endupdateBufferByMap();
	}

	void FruitNinja::GameRecordCommandBuffers()
	{
		mAuxiliaryVision->initCommandBuffer();
	}

	void FruitNinja::GameStopInterfaceLoop(unsigned int mCurrentFrame)
	{
	}

	void FruitNinja::GameTCPLoop()
	{
		if (Global::MultiplePeopleMode) {
			if (Global::ServerOrClient) {
				event_base_loop(server::GetServer()->GetEvent_Base(), EVLOOP_NONBLOCK);
			}
			else {
				event_base_loop(client::GetClient()->GetEvent_Base(), EVLOOP_ONCE);
			}
		}
	}

	void FruitNinja::SpawnFruit()
	{
		float spawnRate = 0.8f;
		float velYMin = 18.0f;
		float velYMax = 28.0f;
		switch (mDifficulty) {
		case Difficulty::Easy:
			spawnRate = 1.5f;
			velYMin = 14.0f;
			velYMax = 22.0f;
			break;
		case Difficulty::Normal:
			spawnRate = 0.8f;
			velYMin = 18.0f;
			velYMax = 28.0f;
			break;
		case Difficulty::Hard:
			spawnRate = 0.4f;
			velYMin = 22.0f;
			velYMax = 35.0f;
			break;
		}

		mFruitSpawnTimer -= spawnRate;

		if (mFruitSpawnTimer > 0) return;
		mFruitSpawnTimer = spawnRate * (0.6f + ((float)rand() / RAND_MAX) * 0.8f);

		FruitType type = (FruitType)(rand() % (int)FruitType::Count);
		float radius = GetFruitRadius(type);
		float x = (VisibleHalfWidth - radius) * (2.0f * (float)rand() / RAND_MAX - 1.0f);
		float y = SpawnY;
		float velX = 4.0f * (2.0f * (float)rand() / RAND_MAX - 1.0f);
		float velY = velYMin + (velYMax - velYMin) * (float)rand() / RAND_MAX;

		FLOAT_ mass = 3.14159f * radius * radius;
		PhysicsBlock::PhysicsCircle* body = new PhysicsBlock::PhysicsCircle({x, y}, radius, mass, 0.3f);
		body->speed = {velX, velY};
		mPhysicsWorld->AddObject(body);

		FruitData fruit;
		fruit.type = type;
		fruit.body = body;
		fruit.cut = false;
		mFruits.push_back(fruit);

		int maxActive = (mDifficulty == Difficulty::Hard) ? 8 : ((mDifficulty == Difficulty::Easy) ? 3 : 5);
		if ((int)mFruits.size() > maxActive) {
			size_t extraCount = mFruits.size() - maxActive;
			float nowTime = (float)std::chrono::duration<float>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
			for (size_t i = 0; i < extraCount; ++i) {
				mFruitSpawnTimer += spawnRate * 0.5f;
			}
		}
	}

	void FruitNinja::UpdateFruits(float dt)
	{
		SpawnFruit();
	}

	void FruitNinja::CheckCutting(float dt)
	{
		if (mSwipeTrail.size() < 6) return;

		float totalDist = 0;
		for (size_t i = 1; i < mSwipeTrail.size(); ++i) {
			totalDist += glm::length(mSwipeTrail[i].pos - mSwipeTrail[i-1].pos);
		}
		float dtSwipe = mSwipeTrail.back().time - mSwipeTrail.front().time;
		float cutSpeed = dtSwipe > 0.001f ? totalDist / dtSwipe : 10.0f;

		if (cutSpeed < 3.0f || totalDist < 0.5f) return;

		glm::vec2 cutDir = glm::normalize(mSwipeTrail.back().pos - mSwipeTrail.front().pos);

		bool cutAny = false;
		for (size_t i = 0; i < mFruits.size(); ++i) {
			if (mFruits[i].cut) continue;
			glm::vec2 fruitPos(mFruits[i].body->pos.x, mFruits[i].body->pos.y);
			float radius = (float)mFruits[i].body->radius;

			bool hit = false;
			for (size_t j = 1; j < mSwipeTrail.size(); ++j) {
				if (LineCircleIntersect(mSwipeTrail[j-1].pos, mSwipeTrail[j].pos, fruitPos, radius)) {
					hit = true;
					break;
				}
			}

			if (hit) {
				cutAny = true;
				CutFruit(i, fruitPos, cutDir, cutSpeed);
			}
		}

		if (cutAny) {
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, false, Global::SoundEffectsVolume * 0.4f, 0.0f);
		}
	}

	void FruitNinja::CheckMissedFruits()
	{
		for (size_t i = 0; i < mFruits.size(); ) {
			FruitData& f = mFruits[i];
			if (f.cut) {
				++i;
				continue;
			}
			bool missed = (f.body->pos.y < SpawnY - 1.0f && f.body->speed.y < -0.5f);
			bool outOfBounds = (f.body->pos.y < DeathY);
			if (missed || outOfBounds) {
				if (!f.cut) {
					LoseLife();
				}
				mPhysicsWorld->RemoveObject(f.body);
				mFruits.erase(mFruits.begin() + i);
				continue;
			}
			++i;
		}
	}

	void FruitNinja::CutFruit(size_t index, glm::vec2 cutPoint, glm::vec2 cutDirection, float cutSpeed)
	{
		FruitData& f = mFruits[index];
		f.cut = true;

		bool isCombo = (mCombo > 0);
		mCombo++;
		mComboTimer = ComboTimeout;

		int score = CalculateScore(f.type, cutSpeed, isCombo);
		AddScore(score, cutPoint);

		SpawnCutFragments(cutPoint, cutDirection, f, cutSpeed);
		SpawnExplosionParticles(cutPoint, GetFruitColor(f.type), 20, 8.0f);
		SpawnFruitJuiceParticles(cutPoint, GetFruitColor(f.type), 15);

		mPhysicsWorld->RemoveObject(f.body);
	}

	void FruitNinja::SpawnCutFragments(glm::vec2 pos, glm::vec2 direction, const FruitData& fruit, float cutSpeed)
	{
		float radius = GetFruitRadius(fruit.type);
		float halfR = radius * 0.65f;
		glm::vec4 color = GetFruitColor(fruit.type);

		glm::vec2 perpDir(-direction.y, direction.x);

		for (int half = 0; half < 2; ++half) {
			FLOAT_ fragR = halfR * (0.7f + 0.3f * (float)rand() / RAND_MAX);
			FLOAT_ fragMass = 3.14159f * fragR * fragR;
			glm::vec2 offset = perpDir * (half == 0 ? halfR * 0.5f : -halfR * 0.5f);
			glm::vec2 fragVel = direction * cutSpeed * (0.3f + 0.4f * (float)rand() / RAND_MAX)
				+ offset * (0.5f + 1.5f * (float)rand() / RAND_MAX);

			PhysicsBlock::PhysicsCircle* frag = new PhysicsBlock::PhysicsCircle(
				{pos.x + offset.x, pos.y + offset.y}, fragR, fragMass, 0.3f);
			frag->speed = {fragVel.x, fragVel.y};
			mPhysicsWorld->AddObject(frag);

			CutFragment cf;
			cf.body = frag;
			cf.lifetime = 0.8f;
			cf.color = color;
			mFragments.push_back(cf);
		}
	}

	void FruitNinja::UpdateFragments(float dt)
	{
		for (size_t i = 0; i < mFragments.size(); ) {
			mFragments[i].lifetime -= dt;
			if (mFragments[i].lifetime <= 0) {
				mPhysicsWorld->RemoveObject(mFragments[i].body);
				mFragments.erase(mFragments.begin() + i);
				continue;
			}
			++i;
		}
	}

	void FruitNinja::UpdateParticles(float dt)
	{
		for (size_t i = 0; i < mParticles.size(); ) {
			mParticles[i].pos += mParticles[i].velocity * dt;
			mParticles[i].lifetime -= dt;
			if (mParticles[i].lifetime <= 0) {
				mParticles.erase(mParticles.begin() + i);
				continue;
			}
			++i;
		}
	}

	void FruitNinja::UpdateScorePopups(float dt)
	{
		for (size_t i = 0; i < mScorePopups.size(); ) {
			mScorePopups[i].pos.y += 2.0f * dt;
			mScorePopups[i].lifetime -= dt;
			if (mScorePopups[i].lifetime <= 0) {
				mScorePopups.erase(mScorePopups.begin() + i);
				continue;
			}
			++i;
		}
	}

	int FruitNinja::CalculateScore(FruitType type, float cutSpeed, bool isCombo)
	{
		int base = GetFruitBaseScore(type);
		float speedMult = glm::clamp(cutSpeed / 8.0f, 0.5f, 3.0f);
		float comboMult = isCombo ? (1.0f + mCombo * 0.5f) : 1.0f;
		return (int)(base * speedMult * comboMult);
	}

	void FruitNinja::AddScore(int points, glm::vec2 pos)
	{
		mScore += points;
		ScorePopup pop;
		pop.pos = pos + glm::vec2(0, 1.0f);
		pop.score = points;
		pop.maxLifetime = 0.8f;
		pop.lifetime = pop.maxLifetime;
		mScorePopups.push_back(pop);
	}

	void FruitNinja::LoseLife()
	{
		mLives--;
		mCombo = 0;
		mComboTimer = 0;
		if (mLives <= 0) {
			mGameState = GameState::GameOver;
			if (mScore > mHighScore) mHighScore = mScore;
			SoundEffect::SoundEffect::GetSoundEffect()->Play("Impact", MP3, false, Global::SoundEffectsVolume * 0.5f, 0.0f);
		}
	}

	void FruitNinja::RenderFruits()
	{
		for (size_t i = 0; i < mFruits.size(); ++i) {
			const FruitData& f = mFruits[i];
			if (f.cut) continue;
			glm::vec4 color = GetFruitColor(f.type);
			FLOAT_ r = f.body->radius;
			mAuxiliaryVision->Circle(glm::dvec3(f.body->pos, 0.0), r, color);

			glm::vec4 innerColor = color * 0.7f;
			innerColor.a = 1.0f;
			mAuxiliaryVision->Circle(glm::dvec3(f.body->pos, 0.0), r * 0.5f, innerColor);

			glm::vec4 highlight = color * 1.3f;
			highlight.a = 0.6f;
			double hlX = (double)f.body->pos.x - (double)r * 0.25;
			double hlY = (double)f.body->pos.y + (double)r * 0.25;
			mAuxiliaryVision->Circle(glm::dvec3(hlX, hlY, 0.0), r * 0.25f, highlight);
		}
	}

	void FruitNinja::RenderFragments()
	{
		for (size_t i = 0; i < mFragments.size(); ++i) {
			const CutFragment& frag = mFragments[i];
			float alpha = frag.lifetime / 0.8f;
			glm::vec4 color = frag.color;
			color.a = alpha;
			mAuxiliaryVision->Circle(glm::dvec3(frag.body->pos, 0.0), frag.body->radius, color);
		}
	}

	void FruitNinja::RenderSwipeTrail()
	{
		if (mSwipeTrail.size() < 2) return;

		float nowTime = (float)std::chrono::duration<float>(
			std::chrono::steady_clock::now().time_since_epoch()).count();

		for (size_t i = 1; i < mSwipeTrail.size(); ++i) {
			float age = nowTime - mSwipeTrail[i].time;
			float alpha = 1.0f - glm::clamp(age / SwipeTrailMaxAge, 0.0f, 1.0f);
			glm::vec4 color = {1.0f, 1.0f, 1.0f, alpha * 0.8f};
			mAuxiliaryVision->Line(
				glm::dvec3(glm::dvec2(mSwipeTrail[i-1].pos), 0.0), color,
				glm::dvec3(glm::dvec2(mSwipeTrail[i].pos), 0.0), color);
		}
	}

	void FruitNinja::RenderParticles()
	{
		for (size_t i = 0; i < mParticles.size(); ++i) {
			const ParticleEffect& p = mParticles[i];
			float alpha = p.lifetime / p.maxLifetime;
			glm::vec4 color = p.color;
			color.a = alpha;
			mAuxiliaryVision->Spot(glm::dvec3(glm::dvec2(p.pos), 0.0), p.size * alpha, color);
		}
	}

	void FruitNinja::RenderScorePopups()
	{
		for (size_t i = 0; i < mScorePopups.size(); ++i) {
			const ScorePopup& sp = mScorePopups[i];
			float alpha = sp.lifetime / sp.maxLifetime;
			mAuxiliaryVision->Spot(glm::dvec3(glm::dvec2(sp.pos), 0.0), 0.2f * (1.0f + alpha), {1.0f, 1.0f, 0.3f, alpha});
		}
	}

	void FruitNinja::RenderBoundary()
	{
		glm::vec4 boundaryColor = {0.8f, 0.2f, 0.2f, 0.8f};
		double y = (double)(DeathY + 1.0f);
		mAuxiliaryVision->AddStaticLine(glm::dvec3(-(double)VisibleHalfWidth, y, 0.0), glm::dvec3((double)VisibleHalfWidth, y, 0.0), boundaryColor);

		glm::vec4 topColor = {0.2f, 0.8f, 0.2f, 0.4f};
		double ty = (double)TopBoundaryY;
		mAuxiliaryVision->AddStaticLine(glm::dvec3(-(double)VisibleHalfWidth, ty, 0.0), glm::dvec3((double)VisibleHalfWidth, ty, 0.0), topColor);
	}

	void FruitNinja::SpawnExplosionParticles(glm::vec2 pos, glm::vec4 color, int count, float speed)
	{
		for (int i = 0; i < count; ++i) {
			float angle = 6.28318f * (float)rand() / RAND_MAX;
			float spd = speed * (0.3f + 0.7f * (float)rand() / RAND_MAX);
			ParticleEffect p;
			p.pos = pos;
			p.velocity = {cosf(angle) * spd, sinf(angle) * spd};
			p.color = color;
			p.size = 0.08f;
			p.maxLifetime = 0.4f + 0.3f * (float)rand() / RAND_MAX;
			p.lifetime = p.maxLifetime;
			mParticles.push_back(p);
		}
	}

	void FruitNinja::SpawnFruitJuiceParticles(glm::vec2 pos, glm::vec4 color, int count)
	{
		for (int i = 0; i < count; ++i) {
			float angle = -1.5708f + 1.5708f * (2.0f * (float)rand() / RAND_MAX - 1.0f);
			float spd = 4.0f + 6.0f * (float)rand() / RAND_MAX;
			ParticleEffect p;
			p.pos = pos;
			p.velocity = {cosf(angle) * spd, sinf(angle) * spd};
			p.color = color * 0.8f;
			p.size = 0.04f;
			p.maxLifetime = 0.3f + 0.2f * (float)rand() / RAND_MAX;
			p.lifetime = p.maxLifetime;
			mParticles.push_back(p);
		}
	}

	glm::vec4 FruitNinja::GetFruitColor(FruitType type)
	{
		switch (type) {
		case FruitType::Apple:        return {1.0f, 0.15f, 0.15f, 1.0f};
		case FruitType::Watermelon:   return {0.15f, 0.75f, 0.15f, 1.0f};
		case FruitType::Orange:       return {1.0f, 0.6f, 0.1f, 1.0f};
		case FruitType::Banana:       return {1.0f, 0.9f, 0.2f, 1.0f};
		case FruitType::Pineapple:    return {0.8f, 0.6f, 0.15f, 1.0f};
		case FruitType::Kiwi:         return {0.4f, 0.7f, 0.15f, 1.0f};
		case FruitType::Peach:        return {1.0f, 0.7f, 0.65f, 1.0f};
		case FruitType::Grape:        return {0.5f, 0.15f, 0.7f, 1.0f};
		case FruitType::Mango:        return {1.0f, 0.7f, 0.15f, 1.0f};
		case FruitType::DragonFruit:  return {0.9f, 0.2f, 0.7f, 1.0f};
		default:                      return {0.5f, 0.5f, 0.5f, 1.0f};
		}
	}

	float FruitNinja::GetFruitRadius(FruitType type)
	{
		switch (type) {
		case FruitType::Apple:        return 1.0f;
		case FruitType::Watermelon:   return 1.5f;
		case FruitType::Orange:       return 0.9f;
		case FruitType::Banana:       return 0.7f;
		case FruitType::Pineapple:    return 1.2f;
		case FruitType::Kiwi:         return 0.8f;
		case FruitType::Peach:        return 0.9f;
		case FruitType::Grape:        return 0.6f;
		case FruitType::Mango:        return 0.95f;
		case FruitType::DragonFruit:  return 1.1f;
		default:                      return 1.0f;
		}
	}

	int FruitNinja::GetFruitBaseScore(FruitType type)
	{
		switch (type) {
		case FruitType::Apple:        return 10;
		case FruitType::Watermelon:   return 25;
		case FruitType::Orange:       return 15;
		case FruitType::Banana:       return 8;
		case FruitType::Pineapple:    return 20;
		case FruitType::Kiwi:         return 12;
		case FruitType::Peach:        return 14;
		case FruitType::Grape:        return 18;
		case FruitType::Mango:        return 16;
		case FruitType::DragonFruit:  return 22;
		default:                      return 10;
		}
	}

	const char* FruitNinja::GetFruitName(FruitType type)
	{
		switch (type) {
		case FruitType::Apple:        return u8"苹果";
		case FruitType::Watermelon:   return u8"西瓜";
		case FruitType::Orange:       return u8"橙子";
		case FruitType::Banana:       return u8"香蕉";
		case FruitType::Pineapple:    return u8"菠萝";
		case FruitType::Kiwi:         return u8"猕猴桃";
		case FruitType::Peach:        return u8"桃子";
		case FruitType::Grape:        return u8"葡萄";
		case FruitType::Mango:        return u8"芒果";
		case FruitType::DragonFruit:  return u8"火龙果";
		default:                      return u8"水果";
		}
	}

	bool FruitNinja::LineCircleIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 center, float radius)
	{
		glm::vec2 ab = b - a;
		glm::vec2 ac = center - a;
		float t = glm::dot(ac, ab) / glm::dot(ab, ab);
		t = glm::clamp(t, 0.0f, 1.0f);
		glm::vec2 closest = a + ab * t;
		return glm::length(closest - center) <= radius;
	}

	void FruitNinja::GameUI()
	{
		switch (mGameState) {
		case GameState::MainMenu:
			RenderMainMenu();
			break;
		case GameState::Playing:
			RenderHUD();
			break;
		case GameState::GameOver:
			RenderGameOver();
			break;
		}
	}

	void FruitNinja::RenderMainMenu()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		ImVec2 center(Global::mWidth * 0.5f, Global::mHeight * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		ImGui::Begin(u8"水果忍者", nullptr, flags);

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::TextColored({1.0f, 0.3f, 0.1f, 1.0f}, u8"🍉 水果忍者 🍉");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, u8"难度选择:");
		const char* difficulties[] = {u8"简单", u8"普通", u8"困难"};
		int diffIdx = (int)mDifficulty;
		if (ImGui::Combo(u8"##Difficulty", &diffIdx, difficulties, 3)) {
			mDifficulty = (Difficulty)diffIdx;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(u8"开始游戏", ImVec2(200, 50))) {
			StartGame();
		}

		ImGui::Spacing();
		ImGui::TextColored({0.5f, 0.5f, 0.5f, 1.0f}, u8"按空格键开始");
		ImGui::Spacing();

		if (mHighScore > 0) {
			ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, u8"最高分: %d", mHighScore);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored({0.5f, 0.5f, 0.6f, 1.0f}, u8"操作说明:");
		ImGui::BulletText(u8"鼠标滑动切割水果");
		ImGui::BulletText(u8"不要漏掉掉落的水果!");
		ImGui::BulletText(u8"漏掉3个水果则游戏结束");
		ImGui::BulletText(u8"连续切割可触发连击加分");

		ImGui::PopFont();
		ImGui::End();
	}

	void FruitNinja::RenderHUD()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
		ImGui::Begin(u8"HUD", nullptr, flags);

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		ImGui::TextColored({1.0f, 0.9f, 0.2f, 1.0f}, u8"得分: %d", mScore);
		ImGui::TextColored({0.6f, 0.8f, 1.0f, 1.0f}, u8"时间: %.0fs", GameDuration - mGameTimeElapsed);

		ImGui::Text(u8"生命: ");
		ImGui::SameLine();
		for (int i = 0; i < MaxLives; ++i) {
			if (i < mLives) {
				ImGui::TextColored({1.0f, 0.2f, 0.2f, 1.0f}, u8"♥ ");
			}
			else {
				ImGui::TextColored({0.3f, 0.3f, 0.3f, 1.0f}, u8"♥ ");
			}
			if (i < MaxLives - 1) ImGui::SameLine();
		}

		if (mCombo >= 2) {
			ImGui::TextColored({1.0f, 0.6f, 0.1f, 1.0f}, u8"连击 x%d!", mCombo);
		}

		float timeProgress = 1.0f - (mGameTimeElapsed / GameDuration);
		ImVec4 barColor = timeProgress > 0.3f ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) :
			(timeProgress > 0.1f ? ImVec4(1.0f, 0.7f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
		ImGui::ProgressBar(timeProgress, ImVec2(150, 8), "");
		ImGui::PopStyleColor();

		ImGui::End();
	}

	void FruitNinja::RenderGameOver()
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize;

		ImVec2 center(Global::mWidth * 0.5f, Global::mHeight * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		ImGui::Begin(u8"游戏结束", nullptr, flags);

		ImVec2 window_pos = ImGui::GetWindowPos();
		ImVec2 window_size = ImGui::GetWindowSize();
		if (((window_pos.x < CursorPosX) && ((window_pos.x + window_size.x) > CursorPosX)) &&
			((window_pos.y < CursorPosY) && ((window_pos.y + window_size.y) > CursorPosY))) {
			Global::ClickWindow = true;
		}

		ImGui::TextColored({1.0f, 0.3f, 0.1f, 1.0f}, u8"游戏结束!");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored({1.0f, 0.9f, 0.2f, 1.0f}, u8"最终得分: %d", mScore);

		if (mScore >= mHighScore && mScore > 0) {
			ImGui::TextColored({1.0f, 0.7f, 0.1f, 1.0f}, u8"★ 新最高分! ★");
		}
		ImGui::TextColored({0.8f, 0.8f, 0.8f, 1.0f}, u8"最高分: %d", mHighScore);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(u8"重新开始", ImVec2(200, 50))) {
			StartGame();
		}

		ImGui::Spacing();
		if (ImGui::Button(u8"返回主菜单", ImVec2(200, 30))) {
			mGameState = GameState::MainMenu;
			ResetGame();
		}

		ImGui::Spacing();
		ImGui::TextColored({0.5f, 0.5f, 0.5f, 1.0f}, u8"按空格键返回菜单");

		ImGui::End();
	}

}