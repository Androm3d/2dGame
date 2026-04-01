#ifndef _ENEMY_INCLUDE
#define _ENEMY_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"
#include "BaseEnemy.h"


struct Arrow
{
	glm::vec2 pos;
	bool goingLeft;
	bool reflected;
};


class Enemy : public BaseEnemy
{
public:
	Enemy();
	~Enemy();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setPosition(const glm::vec2 &pos);

	void takeDamage(int knockDir);
	bool checkArrowHit(const glm::vec2 &pPos, const glm::ivec2 &pSize);
	bool reflectArrowHit(const glm::vec2 &pPos, const glm::ivec2 &pSize, bool playerFacingLeft);
	bool checkReflectedArrowHit(const glm::vec4 &hitbox, int &outKnockDir);
	void destroyArrowsInHitbox(const glm::vec4 &attackHitbox);
	glm::vec4 getHitbox() const;

private:
	void computePath(const glm::vec2 &playerPos) override;

	bool bShooting;
	int shotCooldown;
	Texture spritesheet;
	Texture stairsSpritesheet;
	Texture shotSpritesheet;
	Texture arrowTexture;
	Sprite *sprite;
	Sprite *stairsSprite;
	Sprite *shotSprite;
	Sprite *arrowSprite;

	std::vector<Arrow> arrows;
};


#endif // _ENEMY_INCLUDE
