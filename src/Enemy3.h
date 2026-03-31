#ifndef _ENEMY3_INCLUDE
#define _ENEMY3_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"
#include "BaseEnemy.h"


struct FireBall
{
	glm::vec2 pos;
	bool goingLeft;
	bool reflected;
};


class Enemy3 : public BaseEnemy
{
public:
	Enemy3();
	~Enemy3();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setPosition(const glm::vec2 &pos);

	void takeDamage(int knockDir);
	glm::vec4 getHitbox() const;
	bool checkFireballHit(const glm::vec2 &pPos, const glm::ivec2 &pSize);
	bool reflectFireballHit(const glm::vec2 &pPos, const glm::ivec2 &pSize, bool playerFacingLeft);
	bool checkReflectedFireballHit(const glm::vec4 &hitbox, int &outKnockDir);
	glm::vec4 getMeleeHitbox() const;
	bool isMeleeAttacking() const { return bMelee; }

private:
	void computePath(const glm::vec2 &playerPos) override;

	bool bCasting;     // playing fire-cast animation (Attack)
	bool bMelee;       // playing close-melee animation (Attack2)
	int attackCooldown;
	Texture spritesheet;
	Texture stairsSpritesheet;
	Texture fireSpritesheet;
	Sprite *sprite;
	Sprite *stairsSprite;
	Sprite *fireSprite;   // re-used for each fireball render

	std::vector<FireBall> fireballs;
};


#endif // _ENEMY3_INCLUDE
