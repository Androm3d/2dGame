#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include "Player.h"
#include "Game.h"


#define JUMP_ANGLE_STEP 4
#define JUMP_HEIGHT 96
#define FALL_STEP 4
#define PLAYER_FRAME_WIDTH 64
#define PLAYER_FRAME_HEIGHT 64
#define PLAYER_RUN_ANIM_FRAMES 8
#define PLAYER_JUMP_UP_ANIM_FRAMES 2
#define PLAYER_JUMP_FALL_ANIM_FRAMES 2
#define PLAYER_JUMP_FALL_START_FRAME 4
#define PLAYER_IDLE_ANIM_FRAMES 4
#define PLAYER_IDLE_Y_OFFSET_PIXELS 12
#define PLAYER_HITBOX_WIDTH 32
#define PLAYER_HITBOX_HEIGHT 32


enum PlayerAnims
{
	STAND_LEFT, STAND_RIGHT, MOVE_LEFT, MOVE_RIGHT, JUMP_UP, JUMP_FALL
};


Player::Player()
{
	sprite = NULL;
	map = NULL;
}

Player::~Player()
{
	if (sprite != NULL)
		delete sprite;
}

void Player::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	bJumping = false;
	facingLeft = false;
	spritesheet.loadFromFile("images/Run_Jump_Idle_Animation.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);
	glm::vec2 frameSizeInTexture(
		float(PLAYER_FRAME_WIDTH) / float(spritesheet.width()),
		float(PLAYER_FRAME_HEIGHT) / float(spritesheet.height())
	);
	sprite = Sprite::createSprite(glm::ivec2(PLAYER_FRAME_WIDTH, PLAYER_FRAME_HEIGHT), frameSizeInTexture, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(6);
	
	sprite->setAnimationSpeed(STAND_LEFT, 8);
	sprite->setAnimationSpeed(STAND_RIGHT, 8);
	sprite->setAnimationSpeed(MOVE_LEFT, 12);
	sprite->setAnimationSpeed(MOVE_RIGHT, 12);
	sprite->setAnimationSpeed(JUMP_UP, 12);
	sprite->setAnimationSpeed(JUMP_FALL, 10);
	sprite->setAnimationLoop(STAND_LEFT, true);
	sprite->setAnimationLoop(STAND_RIGHT, true);
	sprite->setAnimationLoop(MOVE_LEFT, true);
	sprite->setAnimationLoop(MOVE_RIGHT, true);
	sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationLoop(JUMP_FALL, false);

	for(int frame = 0; frame < PLAYER_RUN_ANIM_FRAMES; ++frame)
	{
		float u = frame * frameSizeInTexture.x;
		sprite->addKeyframe(MOVE_LEFT, glm::vec2(u, 0.f));
		sprite->addKeyframe(MOVE_RIGHT, glm::vec2(u, 0.f));
	}
	for(int frame = 0; frame < PLAYER_JUMP_UP_ANIM_FRAMES; ++frame)
	{
		float u = frame * frameSizeInTexture.x;
		sprite->addKeyframe(JUMP_UP, glm::vec2(u, frameSizeInTexture.y));
	}
	for(int frame = 0; frame < PLAYER_JUMP_FALL_ANIM_FRAMES; ++frame)
	{
		float u = (frame + PLAYER_JUMP_FALL_START_FRAME) * frameSizeInTexture.x;
		sprite->addKeyframe(JUMP_FALL, glm::vec2(u, frameSizeInTexture.y));
	}
	for(int frame = 0; frame < PLAYER_IDLE_ANIM_FRAMES; ++frame)
	{
		float u = frame * frameSizeInTexture.x;
		float v = 2.f * frameSizeInTexture.y + float(PLAYER_IDLE_Y_OFFSET_PIXELS) / float(spritesheet.height());
		glm::vec2 idleFrame(u, v);
		sprite->addKeyframe(STAND_LEFT, idleFrame);
		sprite->addKeyframe(STAND_RIGHT, idleFrame);
	}
		
	sprite->changeAnimation(0);
	sprite->setFlipHorizontal(false);
	tileMapDispl = tileMapPos;
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - PLAYER_HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - PLAYER_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

}

void Player::update(int deltaTime)
{
	sprite->update(deltaTime);
	if(Game::instance().getKey(GLFW_KEY_LEFT))
	{
		facingLeft = true;
		sprite->setFlipHorizontal(true);
		if(!bJumping && sprite->animation() != MOVE_LEFT)
			sprite->changeAnimation(MOVE_LEFT);
		posPlayer.x -= 2;
		if(map->checkCollision(posPlayer, glm::ivec2(32, 32), CollisionDir::LEFT, &posPlayer.x))
		{
			posPlayer.x += 2;
			if(!bJumping)
				sprite->changeAnimation(STAND_LEFT);
		}
	}
	else if(Game::instance().getKey(GLFW_KEY_RIGHT))
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		if(!bJumping && sprite->animation() != MOVE_RIGHT)
			sprite->changeAnimation(MOVE_RIGHT);
		posPlayer.x += 2;
			if(map->checkCollision(posPlayer, glm::ivec2(32, 32), CollisionDir::RIGHT, &posPlayer.x))		{
			posPlayer.x -= 2;
			if(!bJumping)
				sprite->changeAnimation(STAND_RIGHT);
		}
	}
	else if(!bJumping)
	{
		if(sprite->animation() == MOVE_LEFT)
		{
			facingLeft = true;
			sprite->setFlipHorizontal(facingLeft);
			sprite->changeAnimation(STAND_LEFT);
		}
		else if(sprite->animation() == MOVE_RIGHT)
		{
			facingLeft = false;
			sprite->setFlipHorizontal(facingLeft);
			sprite->changeAnimation(STAND_RIGHT);
		}
		else if(!bJumping && (sprite->animation() == JUMP_UP || sprite->animation() == JUMP_FALL))
		{
			sprite->setFlipHorizontal(facingLeft);
			sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
		}
	}
	
	if(bJumping)
	{
		sprite->setFlipHorizontal(facingLeft);
		if(jumpAngle < 90)
		{
			if(sprite->animation() != JUMP_UP)
				sprite->changeAnimation(JUMP_UP);
		}
		else
		{
			if(sprite->animation() != JUMP_FALL)
				sprite->changeAnimation(JUMP_FALL);
		}
		jumpAngle += JUMP_ANGLE_STEP;
		if(jumpAngle == 180)
		{
			bJumping = false;
			posPlayer.y = startY;
		}
		else
		{
			posPlayer.y = int(startY - JUMP_HEIGHT * sin(3.14159f * jumpAngle / 180.f));
			if(jumpAngle < 90 && map->checkCollision(posPlayer, glm::ivec2(PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT), CollisionDir::UP, &posPlayer.y))
			{
				jumpAngle = 90;
				startY = posPlayer.y + JUMP_HEIGHT;
				sprite->changeAnimation(JUMP_FALL);
			}
			if(jumpAngle > 90)
				bJumping = !map->checkCollision(posPlayer, glm::ivec2(32, 32), CollisionDir::DOWN, &posPlayer.y);
		}
	}
	else
	{
		posPlayer.y += FALL_STEP;
		if(map->checkCollision(posPlayer, glm::ivec2(32, 32), CollisionDir::DOWN, &posPlayer.y))
		{
			if(Game::instance().getKey(GLFW_KEY_UP))
			{
				bJumping = true;
				jumpAngle = 0;
				startY = posPlayer.y;
				sprite->setFlipHorizontal(facingLeft);
				sprite->changeAnimation(JUMP_UP);
			}
		}
	}
	
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - PLAYER_HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - PLAYER_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}

void Player::render()
{
	sprite->render();
}

void Player::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Player::setPosition(const glm::vec2 &pos)
{
	posPlayer = pos;
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - PLAYER_HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - PLAYER_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}




