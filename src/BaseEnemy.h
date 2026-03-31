#ifndef _BASE_ENEMY_INCLUDE
#define _BASE_ENEMY_INCLUDE

#include <vector>
#include <glm/glm.hpp>
#include "TileMap.h"
#include "EnemyNavigator.h"

class Sprite;

struct BaseEnemyMoveConfig
{
	int hitboxWidth;
	int hitboxHeight;
	int moveSpeed;
	float jumpVelocity;
	int dashDurationMs;
	int runAnim;
	int climbAnim;
	int jumpUpAnim;
	int jumpFallAnim;
	int pathRecalcFrames;
	float renderOffsetX;
	float renderOffsetY;
};

class BaseEnemy
{
public:
	BaseEnemy();
	virtual ~BaseEnemy() = default;

	void setTileMap(TileMap *tileMap);
	void setActive(bool value);
	glm::ivec2 getPosition() const { return posEnemy; }
	bool isAlive() const { return alive; }
	bool isDying() const { return bDying; }
	bool isInvincible() const { return hitTimer > 0; }

protected:
	void resetBaseState(const glm::ivec2 &tileMapPos);
	void setPositionCommon(const glm::vec2 &pos, int renderWidth, int renderHeight, int hitboxWidth, int hitboxHeight, Sprite *sprite);
	void computePathCommon(const glm::vec2 &playerPos, const EnemyNavParams &params);
	void updatePathMovementCommon(int deltaTime, float dt, const glm::vec2 &playerPos, Sprite *sprite, const BaseEnemyMoveConfig &config);
	virtual void computePath(const glm::vec2 &playerPos) = 0;

	bool facingLeft;
	bool bJumping;
	bool onGround;
	bool alive;
	bool bDying;
	int health;
	int jumpAngle;
	int startY;
	int pathRecalcTimer;
	int springCooldown;
	int dashCooldown;
	int hitTimer;
	int knockbackFrames;
	int knockbackDir;
	int dropThroughTimerMs;
	int dropCommitTimerMs;
	int dashTimeLeftMs;
	glm::ivec2 tileMapDispl;
	glm::ivec2 posEnemy;
	glm::vec2 posEnemyF;
	float verticalVelocity;
	float dashVelocity;
	float dashVelocityStart;
	bool ladderAnimActive;
	TileMap *map;
	std::vector<glm::ivec2> path;
	int pathIndex;
};

#endif