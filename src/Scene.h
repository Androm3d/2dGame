#ifndef _SCENE_INCLUDE
#define _SCENE_INCLUDE


#include <string>
#include <glm/glm.hpp>
#include "ShaderProgram.h"
#include "TileMap.h"
#include "Player.h"
#include "Enemy.h"
#include "Text.h"


// Scene contains all the entities of our game.
// It is responsible for updating and render them.


class Scene
{

public:
	Scene();
	~Scene();

	void init();
	void update(int deltaTime);
	void render();
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
	void scheduleTransitionToMap(const std::string &targetMap, bool enterSideRoom, int targetDoorIndex = -1);
	void scheduleTransitionToWorld(int targetRoomX, int targetRoomY);

private:
	static constexpr int TRANSITION_DELAY_MS = 1000;

	TileMap *map;
	Player *player;
	glm::vec2 playerInitPos;
	Enemy *enemy;
	ShaderProgram texProgram;
	float currentTime;
	glm::mat4 projection;
	float cameraX = 0.f;
	float cameraY = 0.f;
	float viewWidth = 960.f;
	float viewHeight = 480.f;

	Texture texKey, texHeal, texShield, texWeight, texSword;
	std::vector<Sprite*> keys;
	std::vector<Sprite*> heals;
	Sprite* sword;
	std::vector<Sprite*> shields;
	std::vector<Sprite*> weights;

	Texture texDoor, texPortal;
	std::vector<Sprite*> portals;
	std::vector<Sprite*> doors;

	ShaderProgram bgProgram;
	GLuint bgVao, bgVbo;

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
	Text hudText;
	bool hudReady = false;

};


#endif // _SCENE_INCLUDE

