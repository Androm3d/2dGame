#ifndef _ENEMY_INCLUDE
#define _ENEMY_INCLUDE


#include <vector>
#include "Sprite.h"
#include "TileMap.h"


struct Arrow
{
	glm::vec2 pos;
	bool goingLeft;
};


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

	void takeDamage();
	bool isAlive() const { return alive; }
	glm::vec4 getHitbox() const;

private:
	void computePath(const glm::vec2 &playerPos);

	bool facingLeft;
	bool bJumping;
	bool onGround;
	bool bShooting;
	bool alive;
	int health;
	int jumpAngle, startY;
	int pathRecalcTimer;
	int shotCooldown;
	glm::ivec2 tileMapDispl, posEnemy;
	Texture spritesheet;
	Texture shotSpritesheet;
	Texture arrowTexture;
	Sprite *sprite;
	Sprite *shotSprite;
	Sprite *arrowSprite;
	TileMap *map;

	std::vector<glm::ivec2> path;  // BFS result: sequence of tile coords
	int pathIndex;                  // current waypoint in path

	std::vector<Arrow> arrows;
};


#endif // _ENEMY_INCLUDE
