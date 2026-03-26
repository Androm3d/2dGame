#ifndef _ENEMY_INCLUDE
#define _ENEMY_INCLUDE


#include "Sprite.h"
#include "TileMap.h"


class Enemy
{
public:
	Enemy();
	~Enemy();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::ivec2 &playerPos);
	void render();

	void setTileMap(TileMap *tileMap);
	void setPosition(const glm::vec2 &pos);
	glm::ivec2 getPosition() const { return posEnemy; }

private:
	bool facingLeft;
	glm::ivec2 tileMapDispl, posEnemy;
	Texture spritesheet;
	Sprite *sprite;
	TileMap *map;
};


#endif // _ENEMY_INCLUDE
