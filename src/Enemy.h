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

	void takeDamage(int knockDir);  // knockDir: -1 left, +1 right
	bool isAlive() const { return alive; }
	bool isDying() const { return bDying; }
	bool isInvincible() const { return hitTimer > 0; }
	glm::vec4 getHitbox() const;

private:
	void computePath(const glm::vec2 &playerPos);

	bool facingLeft;
	bool bJumping;
	bool onGround;
	bool bShooting;
	bool alive;
	bool bDying;
	int health;
	int jumpAngle, startY;
	int pathRecalcTimer;
	int shotCooldown;
	int hitTimer;       // invincibility + blink countdown
	int knockbackFrames;
	int knockbackDir;
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
