#ifndef _ENEMY2_INCLUDE
#define _ENEMY2_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"


class Enemy2
{
public:
	Enemy2();
	~Enemy2();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setTileMap(TileMap *tileMap);
	void setPosition(const glm::vec2 &pos);
	glm::ivec2 getPosition() const { return posEnemy; }

	void takeDamage(int knockDir);  // knockDir: -1 left, +1 right
	void setActive(bool value);
	bool isAlive() const { return alive; }
	bool isDying() const { return bDying; }
	bool isInvincible() const { return hitTimer > 0; }
	glm::vec4 getHitbox() const;
	glm::vec4 getMeleeHitbox() const;  // active attack box, valid only while bAttacking
	bool isMeleeAttacking() const { return bAttacking; }

private:
	void computePath(const glm::vec2 &playerPos);

	bool facingLeft;
	bool bJumping;
	bool onGround;
	bool bAttacking;
	bool alive;
	bool bDying;
	int health;
	int jumpAngle, startY;
	int pathRecalcTimer;
	int attackCooldown;
	int springCooldown;
	int dashCooldown;
	int hitTimer;
	int knockbackFrames;
	int knockbackDir;
	int dashTimeLeftMs;
	glm::ivec2 tileMapDispl, posEnemy;
	glm::vec2 posEnemyF;
	float verticalVelocity;
	float dashVelocity;
	float dashVelocityStart;
	Texture spritesheet;
	Sprite *sprite;
	TileMap *map;

	std::vector<glm::ivec2> path;
	int pathIndex;
};


#endif // _ENEMY2_INCLUDE
