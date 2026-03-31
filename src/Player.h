#ifndef _PLAYER_INCLUDE
#define _PLAYER_INCLUDE


#include "Sprite.h"
#include "TileMap.h"


// Player is basically a Sprite that represents the player. As such it has
// all properties it needs to track its movement, jumping, and collisions.


class Player
{
public:
	Player();
	~Player();
	static constexpr int HITBOX_WIDTH = 32;
	static constexpr int HITBOX_HEIGHT = 64;

public:
	void init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram);
	void update(int deltaTime);
	void render();
	glm::vec2 getPosition() const { return posPlayerF; }

	void setTileMap(TileMap *tileMap);
	void setPosition(const glm::vec2 &pos);
	bool isAttacking() const { return bAttacking; }
	glm::vec4 getAttackHitbox() const;
	void takeDamage();
	bool isInvincible() const { return hitTimer > 0; }
	bool isAlive() const { return alive; }
	bool consumeSpringTrigger();

private:
	bool bJumping;
	bool bClimbing;
	bool bAttacking;
	bool facingLeft;
	bool alive;
	int hitTimer;
	int springCooldown;
	int dashCooldown;
	int dropThroughTimerMs;
	bool springTriggered;
	int jumpHeight;
	float dashVelocity;
	float dashVelocityStart;
	int dashTimeLeftMs;
	glm::ivec2 tileMapDispl, posPlayer;
	glm::vec2 posPlayerF;
	float verticalVelocity;
	int jumpAngle, startY;
	Texture spritesheet;
	Sprite *sprite;
	Sprite *attackSprite;
	TileMap *map;

};


#endif // _PLAYER_INCLUDE

