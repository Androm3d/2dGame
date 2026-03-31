#ifndef _ENEMY2_INCLUDE
#define _ENEMY2_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"
#include "BaseEnemy.h"


class Enemy2 : public BaseEnemy
{
public:
	Enemy2();
	~Enemy2();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setPosition(const glm::vec2 &pos);

	void takeDamage(int knockDir);  // knockDir: -1 left, +1 right
	glm::vec4 getHitbox() const;
	glm::vec4 getMeleeHitbox() const;  // active attack box, valid only while bAttacking
	bool isMeleeAttacking() const { return bAttacking; }

private:
	void computePath(const glm::vec2 &playerPos) override;

	bool bAttacking;
	int attackCooldown;
	Texture spritesheet;
	Texture stairsSpritesheet;
	Sprite *sprite;
	Sprite *stairsSprite;
};


#endif // _ENEMY2_INCLUDE
