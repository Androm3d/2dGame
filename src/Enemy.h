#ifndef _ENEMY_INCLUDE
#define _ENEMY_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"


class Enemy
{
public:
	Enemy();
	~Enemy();

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime, const glm::vec2 &playerPos);
	void render();

	void setTileMap(TileMap *tileMap);
	void setPosition(const glm::vec2 &pos);
	glm::ivec2 getPosition() const { return posEnemy; }

private:
	void computePath(const glm::vec2 &playerPos);

	bool facingLeft;
	bool bJumping;
	bool onGround;
	int jumpAngle, startY;
	int pathRecalcTimer;
	glm::ivec2 tileMapDispl, posEnemy;
	Texture spritesheet;
	Sprite *sprite;
	TileMap *map;

	std::vector<glm::ivec2> path;  // BFS result: sequence of tile coords
	int pathIndex;                  // current waypoint in path
};


#endif // _ENEMY_INCLUDE
