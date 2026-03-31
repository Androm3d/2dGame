#ifndef _ENEMY3_INCLUDE
#define _ENEMY3_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"


struct FireBall
{
	glm::vec2 pos;
	bool goingLeft;
};


class Enemy3
{
public:
	Enemy3();
	~Enemy3();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setTileMap(TileMap *tileMap);
	void setPosition(const glm::vec2 &pos);
	glm::ivec2 getPosition() const { return posEnemy; }

	void takeDamage(int knockDir);
	void setActive(bool value);
	bool isAlive() const { return alive; }
	bool isDying() const { return bDying; }
	bool isInvincible() const { return hitTimer > 0; }
	glm::vec4 getHitbox() const;
	bool checkFireballHit(const glm::vec2 &pPos, const glm::ivec2 &pSize);
	glm::vec4 getMeleeHitbox() const;
	bool isMeleeAttacking() const { return bMelee; }

private:
	void computePath(const glm::vec2 &playerPos);

	bool facingLeft;
	bool bJumping;
	bool onGround;
	bool bCasting;     // playing fire-cast animation (Attack)
	bool bMelee;       // playing close-melee animation (Attack2)
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
	glm::ivec2 tileMapDispl, posEnemy;
	glm::vec2 posEnemyF;
	float verticalVelocity;
	Texture spritesheet;
	Texture fireSpritesheet;
	Sprite *sprite;
	Sprite *fireSprite;   // re-used for each fireball render
	TileMap *map;

	std::vector<glm::ivec2> path;
	int pathIndex;

	std::vector<FireBall> fireballs;
};


#endif // _ENEMY3_INCLUDE
