#include <cmath>
#include <GL/glew.h>
#include "Enemy.h"


#define ENEMY_FRAME_WIDTH 103
#define ENEMY_FRAME_HEIGHT 72
#define ENEMY_HITBOX_WIDTH 32
#define ENEMY_HITBOX_HEIGHT 32
#define ENEMY_RUN_FRAMES 8
#define ENEMY_SPEED 1
#define ENEMY_FALL_STEP 4


enum EnemyAnims
{
	RUN
};


Enemy::Enemy()
{
	sprite = NULL;
	map = NULL;
}

Enemy::~Enemy()
{
	if (sprite != NULL)
		delete sprite;
}

void Enemy::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	facingLeft = false;
	spritesheet.loadFromFile("images/Enemy1.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 frameSizeInTexture(
		float(ENEMY_FRAME_WIDTH) / texW,
		float(ENEMY_FRAME_HEIGHT) / texH
	);

	sprite = Sprite::createSprite(glm::ivec2(ENEMY_FRAME_WIDTH, ENEMY_FRAME_HEIGHT), frameSizeInTexture, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(1);
	sprite->setAnimationSpeed(RUN, 10);
	sprite->setAnimationLoop(RUN, true);

	// 8 frames with centers given, convert to top-left of 103x72 frame
	float halfW = float(ENEMY_FRAME_WIDTH) / 2.f;
	float halfH = float(ENEMY_FRAME_HEIGHT) / 2.f;
	sprite->addKeyframe(RUN, glm::vec2(( 57.f - halfW) / texW, ( 44.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((184.f - halfW) / texW, ( 44.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((310.f - halfW) / texW, ( 44.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((436.f - halfW) / texW, ( 44.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2(( 57.f - halfW) / texW, (135.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((184.f - halfW) / texW, (135.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((310.f - halfW) / texW, (135.f - halfH) / texH));
	sprite->addKeyframe(RUN, glm::vec2((436.f - halfW) / texW, (135.f - halfH) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);
	tileMapDispl = tileMapPos;
}

void Enemy::update(int deltaTime, const glm::vec2 &playerPos)
{
	sprite->update(deltaTime);

	// Move toward the player horizontally
	if (playerPos.x < posEnemy.x)
	{
		facingLeft = true;
		sprite->setFlipHorizontal(true);
		posEnemy.x -= ENEMY_SPEED;
		if (map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::LEFT, &posEnemy.x))
			posEnemy.x += ENEMY_SPEED;
	}
	else if (playerPos.x > posEnemy.x)
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		posEnemy.x += ENEMY_SPEED;
		if (map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::RIGHT, &posEnemy.x))
			posEnemy.x -= ENEMY_SPEED;
	}

	// Gravity
	posEnemy.y += ENEMY_FALL_STEP;
	map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &posEnemy.y);

	// Update sprite position
	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}

void Enemy::render()
{
	sprite->render();
}

void Enemy::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Enemy::setPosition(const glm::vec2 &pos)
{
	posEnemy = pos;
	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}
