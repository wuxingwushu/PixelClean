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

		mCamera->setCameraPos({0, 0, 300});
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
		for (auto& hf : mHalfFruits) {
			if (hf.body) {
				mPhysicsWorld->RemoveObject(hf.body);
			}
		}
		for (auto& d : mDebris) {
			if (d.body) {
				mPhysicsWorld->RemoveObject(d.body);
			}
		}
		mFruits.clear();
		mHalfFruits.clear();
		mDebris.clear();
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
		if (winwidth == 0 || winheight == 0) {
#if defined(__ANDROID__)
			winwidth = mWindow->getWidth();
			winheight = mWindow->getHeight();
#else
			glfwGetWindowSize(mWindow->getWindow(), &winwidth, &winheight);
#endif
		}
		glm::vec3 ray = get_ray_direction(CursorPosX, CursorPosY, winwidth, winheight, mCamera->getViewMatrix(), mCamera->getProjectMatrix());
		ray *= -mCamera->getCameraPos().z / ray.z;
		glm::vec2 worldPos(ray.x + mCamera->getCameraPos().x, ray.y + mCamera->getCameraPos().y);

		if (!mIsSwiping && mLeftMouseDown) {
			mIsSwiping = true;
			mSwipeTrail.clear();
			mLastCheckedTrailIndex = 0;
			mSwipeTrail.push_back({worldPos, (float)std::chrono::duration<float>(
				std::chrono::steady_clock::now().time_since_epoch()).count()});
		}

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
				if (!mLeftMouseDown) {
					mLeftMouseDown = true;
					mIsSwiping = true;
					mSwipeTrail.clear();
					mLastCheckedTrailIndex = 0;
					mSwipeTrail.push_back({mLastMouseWorldPos, (float)std::chrono::duration<float>(
						std::chrono::steady_clock::now().time_since_epoch()).count()});
				}
			}
			else if (State == InputState::Up) {
				if (mLeftMouseDown) {
					mLeftMouseDown = false;
					mIsSwiping = false;
					CheckCutting(0.016f);
				}
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
		if (camPos.z > 600.0f) camPos.z = 600.0f;
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
			UpdateHalfFruits(dt);
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
					if (mLastCheckedTrailIndex > 0) --mLastCheckedTrailIndex;
				}

				TryCutDuringSwipe();
			}

			CheckMissedFruits();
			RenderFruits();
			RenderHalfFruits();
			RenderDebris();
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
			UpdateHalfFruits(dt);
			RenderParticles();
			RenderScorePopups();
			RenderHalfFruits();
			RenderDebris();
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

	void FruitNinja::SpawnFruit(float dt)
	{
		float spawnRate = 0.8f;
		float velYMin = 28.0f;
		float velYMax = 44.0f;
		switch (mDifficulty) {
		case Difficulty::Easy:
			spawnRate = 1.5f;
			velYMin = 22.0f;
			velYMax = 38.0f;
			break;
		case Difficulty::Normal:
			spawnRate = 0.8f;
			velYMin = 28.0f;
			velYMax = 44.0f;
			break;
		case Difficulty::Hard:
			spawnRate = 0.4f;
			velYMin = 32.0f;
			velYMax = 54.0f;
			break;
		}

		mFruitSpawnTimer -= dt;

		if (mFruitSpawnTimer > 0) return;
		mFruitSpawnTimer = spawnRate * (0.6f + ((float)rand() / RAND_MAX) * 0.8f);

		FruitType type = (FruitType)(rand() % (int)FruitType::Count);

		PhysicsBlock::PhysicsShape* body = new PhysicsBlock::PhysicsShape({0, 0}, {PixelSize, PixelSize});
		SetupFruitShape(body, type);
		body->UpdateAll();

		float x = (VisibleHalfWidth - (float)body->radius) * (2.0f * (float)rand() / RAND_MAX - 1.0f);
		float velX = 8.0f * (2.0f * (float)rand() / RAND_MAX - 1.0f);
		float velY = velYMin + (velYMax - velYMin) * (float)rand() / RAND_MAX;

		body->pos = {x, SpawnY};
		body->OldPos = body->pos;
		body->speed = {velX, velY};
		body->angle = ((float)rand() / RAND_MAX) * 0.5f - 0.25f;
		body->angleSpeed = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
		mPhysicsWorld->AddObject(body);

		FruitData fruit;
		fruit.type = type;
		fruit.body = body;
		fruit.cut = false;
		mFruits.push_back(fruit);

		int activeCount = 0;
		for (const auto& f : mFruits) {
			if (!f.cut) ++activeCount;
		}
		int maxActive = (mDifficulty == Difficulty::Hard) ? 8 : ((mDifficulty == Difficulty::Easy) ? 3 : 5);
		if (activeCount > maxActive) {
			mFruitSpawnTimer += spawnRate * 0.5f;
		}
	}

	void FruitNinja::UpdateFruits(float dt)
	{
		SpawnFruit(dt);
	}

	void FruitNinja::TryCutDuringSwipe()
	{
		if (mSwipeTrail.size() < 3) return;

		size_t checkStart = mLastCheckedTrailIndex;
		if (checkStart < 1) checkStart = 1;

		for (size_t si = checkStart; si < mSwipeTrail.size(); ++si) {
			glm::vec2 segA = mSwipeTrail[si - 1].pos;
			glm::vec2 segB = mSwipeTrail[si].pos;
			float segLen = glm::length(segB - segA);
			if (segLen < 0.5f) continue;
			float segDt = mSwipeTrail[si].time - mSwipeTrail[si - 1].time;
			float segSpeed = segDt > 0.0005f ? segLen / segDt : 40.0f;

			for (size_t fi = 0; fi < mFruits.size(); ++fi) {
				if (mFruits[fi].cut) continue;
				glm::vec2 fruitPos(mFruits[fi].body->pos.x, mFruits[fi].body->pos.y);
				float radius = (float)mFruits[fi].body->radius;

				if (LineCircleIntersect(segA, segB, fruitPos, radius)) {
					glm::vec2 cutDir = segB - segA;
					if (segLen > 0.001f) cutDir /= segLen;
					else cutDir = {1.0f, 0.0f};
					CutFruit(fi, fruitPos, cutDir, segSpeed);
				}
			}
		}

		mLastCheckedTrailIndex = mSwipeTrail.size();
	}

	void FruitNinja::CheckCutting(float dt)
	{
		if (mSwipeTrail.size() < 6) return;

		float totalDist = 0;
		for (size_t i = 1; i < mSwipeTrail.size(); ++i) {
			totalDist += glm::length(mSwipeTrail[i].pos - mSwipeTrail[i-1].pos);
		}
		float dtSwipe = mSwipeTrail.back().time - mSwipeTrail.front().time;
		float cutSpeed = dtSwipe > 0.001f ? totalDist / dtSwipe : 20.0f;

		if (cutSpeed < 14.0f || totalDist < 3.5f) return;

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
				mFruits.erase(mFruits.begin() + i);
				continue;
			}
			bool missed = (f.body->pos.y < SpawnY - 1.0f && f.body->speed.y < -0.5f);
			bool outOfBounds = (f.body->pos.y < DeathY);
			if (missed || outOfBounds) {
				LoseLife();
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

		SpawnHalfFruits(f, cutPoint, cutDirection, cutSpeed);
		SpawnExplosionParticles(cutPoint, GetFruitColor(f.type), 40, 20.0f);
		SpawnFruitJuiceParticles(cutPoint, GetFruitColor(f.type), 30);

		mPhysicsWorld->RemoveObject(f.body);
		f.body = nullptr;
	}

	void FruitNinja::SpawnHalfFruits(const FruitData& fruit, glm::vec2 cutPoint, glm::vec2 cutDirection, float cutSpeed)
	{
		PhysicsBlock::PhysicsShape* srcBody = fruit.body;
		FruitType type = fruit.type;

		float cosA = cosf(-srcBody->angle);
		float sinA = sinf(-srcBody->angle);
		glm::vec2 localPt = cutPoint - glm::vec2(srcBody->pos.x, srcBody->pos.y);
		glm::vec2 localCP(
			localPt.x * cosA - localPt.y * sinA,
			localPt.x * sinA + localPt.y * cosA
		);
		glm::vec2 localCPGrid = localCP + glm::vec2(srcBody->CentreMass.x, srcBody->CentreMass.y);

		glm::vec2 localDir(
			cutDirection.x * cosA - cutDirection.y * sinA,
			cutDirection.x * sinA + cutDirection.y * cosA
		);

		float len = glm::length(localDir);
		if (len < 0.0001f) localDir = {1.0f, 0.0f};
		else localDir /= len;

		PhysicsBlock::PhysicsShape* halfA = new PhysicsBlock::PhysicsShape({0, 0}, {PixelSize, PixelSize});
		PhysicsBlock::PhysicsShape* halfB = new PhysicsBlock::PhysicsShape({0, 0}, {PixelSize, PixelSize});

		for (int gx = 0; gx < PixelSize; ++gx) {
			for (int gy = 0; gy < PixelSize; ++gy) {
				if (!(srcBody->at(gx, gy).Entity)) continue;

				float dx = gx + 0.5f - localCPGrid.x;
				float dy = gy + 0.5f - localCPGrid.y;
				float perp = -localDir.x * dy + localDir.y * dx;

				float perpDist = fabsf(perp);

				if (perp > 0.01f || (perpDist < 0.6f && perp >= -0.01f)) {
					halfA->at(gx, gy).Entity = true;
					halfA->at(gx, gy).Collision = true;
					halfA->at(gx, gy).mass = 1.0f;
				}
				if (perp < -0.01f) {
					halfB->at(gx, gy).Entity = true;
					halfB->at(gx, gy).Collision = true;
					halfB->at(gx, gy).mass = 1.0f;
				}
			}
		}

		halfA->angle = srcBody->angle;
		halfB->angle = srcBody->angle;
		halfA->UpdateAll();
		halfB->UpdateAll();

		halfA->pos = srcBody->pos;
		halfA->OldPos = halfA->pos;
		halfB->pos = srcBody->pos;
		halfB->OldPos = halfB->pos;

		glm::vec2 perpDir(-cutDirection.y, cutDirection.x);
		float sepSpeed = cutSpeed * 0.3f;
		halfA->speed = {srcBody->speed.x + perpDir.x * sepSpeed, srcBody->speed.y + perpDir.y * sepSpeed};
		halfB->speed = {srcBody->speed.x - perpDir.x * sepSpeed, srcBody->speed.y - perpDir.y * sepSpeed};
		halfA->angleSpeed = srcBody->angleSpeed + 3.0f;
		halfB->angleSpeed = srcBody->angleSpeed - 3.0f;

		mPhysicsWorld->AddObject(halfA);
		mPhysicsWorld->AddObject(halfB);

		HalfFruit hfA, hfB;
		hfA.body = halfA; hfA.type = type; hfA.lifetime = HalfFruitLifetime;
		hfB.body = halfB; hfB.type = type; hfB.lifetime = HalfFruitLifetime;
		mHalfFruits.push_back(hfA);
		mHalfFruits.push_back(hfB);
	}

	void FruitNinja::UpdateHalfFruits(float dt)
	{
		for (size_t i = 0; i < mHalfFruits.size(); ) {
			mHalfFruits[i].lifetime -= dt;
			if (mHalfFruits[i].lifetime <= 0) {
				mPhysicsWorld->RemoveObject(mHalfFruits[i].body);
				mHalfFruits.erase(mHalfFruits.begin() + i);
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
			mScorePopups[i].pos.y += 8.0f * dt;
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
		float speedMult = glm::clamp(cutSpeed / 20.0f, 0.5f, 3.0f);
		float comboMult = isCombo ? (1.0f + mCombo * 0.5f) : 1.0f;
		return (int)(base * speedMult * comboMult);
	}

	void FruitNinja::AddScore(int points, glm::vec2 pos)
	{
		mScore += points;
		ScorePopup pop;
		pop.pos = pos + glm::vec2(0, 4.0f);
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
			RenderPixelFruit(f);
		}
	}

	void FruitNinja::RenderHalfFruits()
	{
		for (size_t i = 0; i < mHalfFruits.size(); ++i) {
			const HalfFruit& hf = mHalfFruits[i];
			RenderPixelHalf(hf);
		}
	}

	void FruitNinja::RenderDebris()
	{
		for (size_t i = 0; i < mDebris.size(); ++i) {
			const DebrisFragment& d = mDebris[i];
			float alpha = d.lifetime / 0.8f;
			glm::vec4 color = d.color;
			color.a = alpha;
			mAuxiliaryVision->Circle(glm::dvec3(d.body->pos, 0.0), d.body->radius, color);
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
		double y = (double)(DeathY + 8.0f);
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
			p.size = 0.25f;
			p.maxLifetime = 0.5f + 0.3f * (float)rand() / RAND_MAX;
			p.lifetime = p.maxLifetime;
			mParticles.push_back(p);
		}
	}

	void FruitNinja::SpawnFruitJuiceParticles(glm::vec2 pos, glm::vec4 color, int count)
	{
		for (int i = 0; i < count; ++i) {
			float angle = -1.5708f + 1.5708f * (2.0f * (float)rand() / RAND_MAX - 1.0f);
			float spd = 12.0f + 16.0f * (float)rand() / RAND_MAX;
			ParticleEffect p;
			p.pos = pos;
			p.velocity = {cosf(angle) * spd, sinf(angle) * spd};
			p.color = color * 0.8f;
			p.size = 0.12f;
			p.maxLifetime = 0.3f + 0.3f * (float)rand() / RAND_MAX;
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

	void FruitNinja::DrawFruitPattern(PixelColorGrid& grid, FruitType type)
	{
		for (int x = 0; x < PixelSize; ++x)
			for (int y = 0; y < PixelSize; ++y)
				grid[x][y] = 0;

		int cx = PixelSize / 2;
		int cy = PixelSize / 2;

		int bodyR, innerR;

		switch (type) {
		case FruitType::Apple:
			bodyR = 14; innerR = 12;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - (cy + 1);
					float dist = sqrtf((float)(dx*dx + dy*dy));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 1.5f) ? 2 : 1;
					}
					if (dist <= innerR - 3 && dx > 3 && dy < -2) grid[x][y] = 3;
				}
			for (int x = cx - 1; x <= cx; ++x)
				for (int y = 0; y <= 4; ++y)
					grid[x][y] = 4;
			for (int x = cx + 1; x <= cx + 5; ++x)
				for (int y = 3; y <= 7; ++y) {
					int lx = x - (cx + 2), ly = y - 5;
					if (lx*lx*2 + ly*ly < 20) grid[x][y] = 5;
				}
			break;

		case FruitType::Watermelon:
			bodyR = 15; innerR = 13;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - cy;
					float dist = sqrtf((float)(dx*dx + dy*dy));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 1.5f) ? 2 : 1;
						if (dist < innerR - 3) grid[x][y] = 6;
						if (dist < innerR - 5) grid[x][y] = 7;
					}
				}
			break;

		case FruitType::Orange:
			bodyR = 14; innerR = 11;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - cy;
					float dist = sqrtf((float)(dx*dx + dy*dy));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 1.5f) ? 2 : 1;
					}
					if (dist <= innerR && dx > 3 && dy < -2) grid[x][y] = 3;
				}
			grid[cx][cy - bodyR + 3] = 4;
			break;

		case FruitType::Banana:
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int bx = x - 4, by = y - 4;
					float curveY = by - bx * bx * 0.008f;
					float dist = fabsf(curveY);
					if (x >= 4 && x <= 28 && dist <= 5.0f) {
						grid[x][y] = (dist > 3.5f) ? 2 : 1;
					}
				}
			for (int x = 4; x <= 8; ++x)
				for (int y = 0; y <= 6; ++y)
					if (sqrtf((float)((x-6)*(x-6) + (y-4)*(y-4))) < 3.5f)
						grid[x][y] = 4;
			for (int x = 24; x <= 29; ++x)
				for (int y = 0; y <= 6; ++y)
					if (sqrtf((float)((x-26)*(x-26) + (y-4)*(y-4))) < 3.5f)
						grid[x][y] = 4;
			break;

		case FruitType::Pineapple:
			bodyR = 12;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - (cy + 2);
					if ((float)(dx*dx)/(11.0f*11.0f) + (float)(dy*dy)/(14.0f*14.0f) <= 1.0f) {
						float dist = sqrtf((float)(dx*dx + dy*dy));
						grid[x][y] = (dist > bodyR - 2.0f) ? 2 : 1;
					}
				}
			for (int y = cx + 10; y <= cx + 14; ++y)
				for (int x = cx - 5; x <= cx + 5; ++x) {
					float ex = (float)(x - cx) / 5.0f;
					float ey = (float)(y - (cx + 11)) / 4.0f;
					if (ex*ex + ey*ey <= 1.0f) grid[x][y] = 5;
				}
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					if (grid[x][y] == 1 && (x + y) % 8 < 3) grid[x][y] = 8;
				}
			break;

		case FruitType::Kiwi:
			bodyR = 13; innerR = 10;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - cy;
					float dist = sqrtf((float)(dx*dx + dy*dy));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 2.0f) ? 2 : 1;
						if (dist < innerR) grid[x][y] = 5;
						if (dist < innerR - 2) grid[x][y] = 9;
					}
				}
			for (int x = cx - 4; x <= cx + 4; ++x)
				for (int y = cy - 4; y <= cy + 4; ++y)
					if ((x-cx)*(x-cx) + (y-cy)*(y-cy) < 9)
						grid[x][y] = 10;
			break;

		case FruitType::Peach:
			bodyR = 14;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - (cy + 1);
					float dist = sqrtf((float)(dx*dx + dy*dy * 1.1f));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 1.5f) ? 2 : 1;
					}
					if (dist < bodyR - 3 && dx > 3 && dy < -1) grid[x][y] = 3;
					if (dy > 6 && fabsf((float)dx) < 3) grid[x][y] = 2;
				}
			break;

		case FruitType::Grape: {
			struct { int gx, gy, r; } grapes[] = {
				{14, 18, 6}, {20, 16, 6}, {12, 12, 6}, {18, 10, 5},
				{14, 6, 5}, {21, 22, 5}, {11, 22, 4}
			};
			for (auto& g : grapes) {
				for (int x = 0; x < PixelSize; ++x)
					for (int y = 0; y < PixelSize; ++y) {
						int dx = x - g.gx, dy = y - g.gy;
						float dist = sqrtf((float)(dx*dx + dy*dy));
						if (dist <= g.r) {
							grid[x][y] = (dist > g.r - 1.0f) ? 2 : 1;
						}
						if (dist < g.r - 2 && dx > 0 && dy < -1) grid[x][y] = 3;
					}
			}
			for (int x = 14; x <= 18; ++x)
				for (int y = 25; y <= 28; ++y)
					grid[x][y] = 4;
			break;
		}

		case FruitType::Mango:
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - (cy + 1);
					float ex = (float)dx / 10.0f;
					float ey = (float)dy / 14.0f;
					float eDist = ex*ex + ey*ey;
					if (eDist <= 1.0f && y > 4) {
						grid[x][y] = (eDist > 0.75f) ? 2 : 1;
					}
					if (eDist < 0.4f && dx > 2 && dy < -2) grid[x][y] = 3;
				}
			for (int x = cx - 1; x <= cx + 1; ++x)
				for (int y = cy + 12; y <= cy + 15; ++y)
					grid[x][y] = 4;
			for (int x = cx - 3; x <= cx + 3 && x < PixelSize; ++x)
				for (int y = cy - 15; y <= cy - 12 && y >= 0; ++y)
					if (fabsf((float)(x - cx)) < 3.5f)
						grid[x][y] = 4;
			break;

		case FruitType::DragonFruit:
			bodyR = 13;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - cy;
					float dist = sqrtf((float)(dx*dx + dy*dy));
					if (dist <= bodyR) {
						grid[x][y] = (dist > bodyR - 1.5f) ? 2 : 1;
					}
				}
			for (int i = 0; i < 8; ++i) {
				float a = i * 0.785f;
				int sx = cx + (int)(cosf(a) * 11);
				int sy = cy + (int)(sinf(a) * 11);
				for (int x = sx - 2; x <= sx + 2; ++x)
					for (int y = sy - 2; y <= sy + 2; ++y)
						if (x >= 0 && x < PixelSize && y >= 0 && y < PixelSize)
							grid[x][y] = 5;
			}
			break;

		default:
			bodyR = 13;
			for (int x = 0; x < PixelSize; ++x)
				for (int y = 0; y < PixelSize; ++y) {
					int dx = x - cx, dy = y - cy;
					if (dx*dx + dy*dy <= bodyR*bodyR)
						grid[x][y] = 1;
				}
			break;
		}
	}

	glm::vec4 FruitNinja::MapPixelColor(uint8_t cell, glm::vec4 baseColor)
	{
		switch (cell) {
		case 0:  return {0, 0, 0, 0};
		case 1:  return baseColor;
		case 2:  return baseColor * 0.55f;
		case 3:  { glm::vec4 h = baseColor * 1.4f; h.a = 1.0f; return glm::clamp(h, 0.0f, 1.0f); }
		case 4:  return {0.35f, 0.2f, 0.1f, 1.0f};
		case 5:  return {0.15f, 0.55f, 0.1f, 1.0f};
		case 6:  return {0.1f, 0.45f, 0.1f, 1.0f};
		case 7:  return {0.85f, 0.25f, 0.25f, 1.0f};
		case 8:  return baseColor * 0.65f;
		case 9:  return {0.55f, 0.85f, 0.2f, 1.0f};
		case 10: return {0.8f, 0.9f, 0.5f, 1.0f};
		default: return baseColor;
		}
	}

	void FruitNinja::SetupFruitShape(PhysicsBlock::PhysicsShape* shape, FruitType type)
	{
		PixelColorGrid pattern;
		DrawFruitPattern(pattern, type);

		for (int x = 0; x < PixelSize; ++x) {
			for (int y = 0; y < PixelSize; ++y) {
				if (pattern[x][y] != 0) {
					shape->at(x, y).Entity = true;
					shape->at(x, y).Collision = true;
					shape->at(x, y).mass = 1.0f;
				}
			}
		}
	}

	void FruitNinja::RenderPixelFruit(const FruitData& f)
	{
		PixelColorGrid pattern;
		DrawFruitPattern(pattern, f.type);
		glm::vec4 baseColor = GetFruitColor(f.type);

		float cosA = cosf(f.body->angle);
		float sinA = sinf(f.body->angle);

		for (int gx = 0; gx < PixelSize; ++gx) {
			for (int gy = 0; gy < PixelSize; ++gy) {
				uint8_t cell = pattern[gx][gy];
				if (cell == 0) continue;

				glm::vec4 color = MapPixelColor(cell, baseColor);
				if (color.a < 0.01f) continue;

				float localX = gx + 0.5f - (float)f.body->CentreMass.x;
				float localY = gy + 0.5f - (float)f.body->CentreMass.y;

				float worldX = (float)f.body->pos.x + localX * cosA - localY * sinA;
				float worldY = (float)f.body->pos.y + localX * sinA + localY * cosA;

				mAuxiliaryVision->Spot(glm::dvec3(worldX, worldY, 0.0), 0.55f, color);
			}
		}
	}

	void FruitNinja::RenderPixelHalf(const HalfFruit& hf)
	{
		PixelColorGrid pattern;
		DrawFruitPattern(pattern, hf.type);
		glm::vec4 baseColor = GetFruitColor(hf.type);
		float alpha = hf.lifetime / HalfFruitLifetime;

		float cosA = cosf(hf.body->angle);
		float sinA = sinf(hf.body->angle);

		for (int gx = 0; gx < PixelSize; ++gx) {
			for (int gy = 0; gy < PixelSize; ++gy) {
				if (!(hf.body->at(gx, gy).Entity)) continue;

				uint8_t cell = pattern[gx][gy];
				if (cell == 0) continue;

				glm::vec4 color = MapPixelColor(cell, baseColor);
				color.a = alpha;

				float localX = gx + 0.5f - (float)hf.body->CentreMass.x;
				float localY = gy + 0.5f - (float)hf.body->CentreMass.y;

				float worldX = (float)hf.body->pos.x + localX * cosA - localY * sinA;
				float worldY = (float)hf.body->pos.y + localX * sinA + localY * cosA;

				mAuxiliaryVision->Spot(glm::dvec3(worldX, worldY, 0.0), 0.55f, color);
			}
		}
	}

}