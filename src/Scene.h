#ifndef _SCENE_INCLUDE
#define _SCENE_INCLUDE


#include <string>
#include <glm/glm.hpp>
#include "ShaderProgram.h"
#include "TileMap.h"
#include "Player.h"
#include "Enemy.h"
#include "Enemy2.h"
#include "Enemy3.h"
#include "Text.h"


// Scene contains all the entities of our game.
// It is responsible for updating and render them.


class Scene
{
	struct VfxParticle {
		glm::vec2 pos;
		glm::vec2 vel;
		glm::vec4 color;
		float lifeMs;
		float size;
	};

public:
	Scene();
	~Scene();

	void init();
	void update(int deltaTime);
	void render();
	bool isPlayerAlive() const { return player != nullptr && player->isAlive(); }
	void renderMenuScreen(int selection);
	void renderInstructionsScreen();
	void renderCreditsScreen();
	bool checkAABB(const glm::vec2& pos1, const glm::ivec2& size1, const glm::vec2& pos2, const glm::ivec2& size2);

	const std::vector<Sprite*>& getKeys() const { return keys; }
	const std::vector<Sprite*>& getHeals() const { return heals; }
	const std::vector<Sprite*>& getShields() const { return shields; }
	const std::vector<Sprite*>& getWeights() const { return weights; }
	const Sprite* getSword() const { return sword; }
	const std::vector<Sprite*>& getPortals() const { return portals; }
	const std::vector<Sprite*>& getDoors() const { return doors; }
	

private:
	void initShaders();
	void clearLevelEntities();
	void updateCamera();
	void updateVfx(int deltaTime);
	void saveCurrentRoomRuntimeState();
	int findPortalSpawnIndexForSide(const std::vector<glm::ivec2> &portalSpawns, int sideCode) const;
	void spawnExplosionParticles(const glm::vec2 &center);
	void spawnLandingDustParticles(const glm::vec2 &center, const glm::vec4 &baseColor, float impact);
	glm::vec4 sampleGroundDustColor(const glm::vec2 &playerPos) const;
	void scheduleTransitionToMap(const std::string &targetMap, bool enterSideRoom, int targetDoorIndex = -1);
	void scheduleTransitionToWorld(int targetRoomX, int targetRoomY);

private:
	static constexpr int TRANSITION_DELAY_MS = 1000;

	TileMap *map;
	Player *player;
	glm::vec2 playerInitPos;
	Enemy *enemy;
	Enemy2 *enemy2;
	Enemy3 *enemy3;
	ShaderProgram texProgram;
	float currentTime;
	glm::mat4 projection;
	float cameraX = 0.f;
	float cameraY = 0.f;
	float viewWidth = 960.f;
	float viewHeight = 480.f;

	Texture texKey, texHeal, texShield, texWeight, texSword, texSpring, texDash;
	std::vector<Sprite*> keys;
	std::vector<glm::vec2> keyBasePositions;
	std::vector<Sprite*> heals;
	std::vector<glm::vec2> healBasePositions;
	Sprite* sword;
	glm::vec2 swordBasePosition;
	bool swordHasBasePosition = false;
	std::vector<Sprite*> shields;
	std::vector<glm::vec2> shieldBasePositions;
	std::vector<Sprite*> weights;
	std::vector<float> weightVelocities;
	std::vector<int> weightSpringCooldownMs;
	std::vector<int> weightDashCooldownMs;
	std::vector<float> weightDashVelocities;
	std::vector<float> weightDashVelocityStarts;
	std::vector<int> weightDashTimeLeftMs;
	std::vector<Sprite*> springs;
	std::vector<Sprite*> dashes;

	Texture texDoor, texPortal;
	std::vector<Sprite*> portals;
	std::vector<Sprite*> doors;

	ShaderProgram bgProgram;
	ShaderProgram particleProgram;
	GLuint bgVao, bgVbo;
	GLuint particleVao = 0, particleVbo = 0;

	bool transitionPending = false;
	int transitionDelayMs = 0;
	bool transitionToSideRoom = false;
	bool transitionToWorldRoom = false;
	std::string pendingTargetMap;
	int pendingTargetDoorIndex = -1;
	int pendingRoomX = 0;
	int pendingRoomY = 0;
	bool upWasDown = false;

	bool attackHitThisSwing;
	bool attackHitEnemy2ThisSwing;
	bool attackHitEnemy3ThisSwing;
	bool enemy2HitPlayerThisSwing;
	bool enemy3HitPlayerThisSwing;

	bool enemyActivated = false;
	bool enemy2Activated = false;
	bool enemy3Activated = false;
	glm::vec2 enemySpawnPos;
	glm::vec2 enemy2SpawnPos;
	glm::vec2 enemy3SpawnPos;
	Text hudText;
	bool hudReady = false;
	std::vector<VfxParticle> vfxParticles;
	int explosionFlashMs = 0;
	int parryFlashMs = 0;
	int teleportWarpMs = 0;
	float grayscaleAmount = 0.0f;

};


#endif // _SCENE_INCLUDE

