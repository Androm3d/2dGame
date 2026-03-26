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
#define PLAYER_ATTACK_ANIM_FRAMES 5
#define PLAYER_ATTACK_FRAME_WIDTH 96
#define PLAYER_ATTACK_FRAME_HEIGHT 104
#define PLAYER_CLIMB_ANIM_FRAMES 2
#define PLAYER_HITBOX_WIDTH Player::HITBOX_WIDTH
#define PLAYER_HITBOX_HEIGHT Player::HITBOX_HEIGHT


enum PlayerAnims
{
	STAND_LEFT, STAND_RIGHT, MOVE_LEFT, MOVE_RIGHT, JUMP_UP, JUMP_FALL, ATTACK, CLIMB_STAIRS
};


Player::Player()
{
	sprite = NULL;
	attackSprite = NULL;
	map = NULL;
}

Player::~Player()
{
	if (sprite != NULL)
		delete sprite;
	if (attackSprite != NULL)
		delete attackSprite;
}

void Player::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	bJumping = false;
	bClimbing = false;
	bAttacking = false;
	facingLeft = false;
	spritesheet.loadFromFile("images/Samurai_Animation.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);
	glm::vec2 frameSizeInTexture(
		float(PLAYER_FRAME_WIDTH) / float(spritesheet.width()),
		float(PLAYER_FRAME_HEIGHT) / float(spritesheet.height())
	);
	sprite = Sprite::createSprite(glm::ivec2(PLAYER_FRAME_WIDTH, PLAYER_FRAME_HEIGHT), frameSizeInTexture, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(8);

	sprite->setAnimationSpeed(STAND_LEFT, 8);
	sprite->setAnimationSpeed(STAND_RIGHT, 8);
	sprite->setAnimationSpeed(MOVE_LEFT, 12);
	sprite->setAnimationSpeed(MOVE_RIGHT, 12);
	sprite->setAnimationSpeed(JUMP_UP, 12);
	sprite->setAnimationSpeed(JUMP_FALL, 10);
	sprite->setAnimationSpeed(CLIMB_STAIRS, 8);
	sprite->setAnimationLoop(STAND_LEFT, true);
	sprite->setAnimationLoop(STAND_RIGHT, true);
	sprite->setAnimationLoop(MOVE_LEFT, true);
	sprite->setAnimationLoop(MOVE_RIGHT, true);
	sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationLoop(JUMP_FALL, false);
	sprite->setAnimationLoop(CLIMB_STAIRS, true);

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

	// Climb stairs animation (2 frames)
	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	float halfW = float(PLAYER_FRAME_WIDTH) / 2.f;
	float halfH = float(PLAYER_FRAME_HEIGHT) / 2.f;
	sprite->addKeyframe(CLIMB_STAIRS, glm::vec2((37.f - halfW) / texW, (369.f - halfH) / texH));
	sprite->addKeyframe(CLIMB_STAIRS, glm::vec2((97.f - halfW) / texW, (369.f - halfH) / texH));

	// Attack sprite (separate, larger frame: 96x104 to fit sword + slash)
	glm::vec2 attackFrameSize(
		float(PLAYER_ATTACK_FRAME_WIDTH) / texW,
		float(PLAYER_ATTACK_FRAME_HEIGHT) / texH
	);
	attackSprite = Sprite::createSprite(glm::ivec2(PLAYER_ATTACK_FRAME_WIDTH, PLAYER_ATTACK_FRAME_HEIGHT), attackFrameSize, &spritesheet, &shaderProgram);
	attackSprite->setNumberAnimations(1);
	attackSprite->setAnimationSpeed(0, 18);
	attackSprite->setAnimationLoop(0, false);
	// Align each frame by its BOTTOM edge (feet on ground) to the bottom of the 104px quad.
	// X: body center at x=48 in the 96-wide quad (centered, so flip works symmetrically).
	// UV top-left: x = (center_x - 48) / texW,  y = (center_y + height/2 - 104) / texH
	attackSprite->addKeyframe(0, glm::vec2( 7.f   / texW, 219.5f / texH));  // (55-48,  286+37.5-104)
	attackSprite->addKeyframe(0, glm::vec2(97.f   / texW, 218.f  / texH));  // (145-48, 273+49-104)
	attackSprite->addKeyframe(0, glm::vec2(184.f  / texW, 221.f  / texH));  // (232-48, 273+52-104)
	attackSprite->addKeyframe(0, glm::vec2(278.f  / texW, 222.f  / texH));  // (326-48, 277+49-104)
	attackSprite->addKeyframe(0, glm::vec2(390.f  / texW, 216.5f / texH));  // (438-48, 282+38.5-104)
	attackSprite->changeAnimation(0);
	attackSprite->setFlipHorizontal(false);
		
	sprite->changeAnimation(0);
	sprite->setFlipHorizontal(false);
	tileMapDispl = tileMapPos;
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

}

void Player::update(int deltaTime)
{
	sprite->update(deltaTime);
	bool onLadder = map->isOnLadder(posPlayer, glm::ivec2(PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT));
	bool skipMovement = false;

	// Attack with SPACE
	if(Game::instance().getKey(GLFW_KEY_SPACE) && !bAttacking && !bClimbing)
	{
		bAttacking = true;
		attackSprite->changeAnimation(0);
		attackSprite->setFlipHorizontal(facingLeft);
	}
	if(bAttacking)
	{
		attackSprite->update(deltaTime);
		if(attackSprite->animationFinished())
			bAttacking = false;
		// Only skip horizontal movement, but keep gravity/jump physics running
	}

	// Climbing logic
	if(!skipMovement && onLadder && (Game::instance().getKey(GLFW_KEY_UP) || Game::instance().getKey(GLFW_KEY_DOWN)))
	{
		if(!bClimbing)
		{
			bClimbing = true;
			bJumping = false;
			sprite->changeAnimation(CLIMB_STAIRS);
		}
		if(Game::instance().getKey(GLFW_KEY_UP))
		{
			posPlayer.y -= 2;
			if(map->checkCollision(posPlayer, glm::ivec2(PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT), CollisionDir::UP, &posPlayer.y))
				posPlayer.y += 2;
		}
		if(Game::instance().getKey(GLFW_KEY_DOWN))
		{
			posPlayer.y += 2;
			if(map->checkCollision(posPlayer, glm::ivec2(PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT), CollisionDir::DOWN, &posPlayer.y))
				posPlayer.y -= 2;
		}
		skipMovement = true;
	}
	else if(bClimbing && !skipMovement)
	{
		bClimbing = false;
		sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
	}

	// Horizontal movement (blocked during attack and climbing)
	if(!skipMovement && !bAttacking)
	{
		if(Game::instance().getKey(GLFW_KEY_LEFT))
		{
			facingLeft = true;
			sprite->setFlipHorizontal(true);
			if(!bJumping && sprite->animation() != MOVE_LEFT)
				sprite->changeAnimation(MOVE_LEFT);
			posPlayer.x -= 3;
			if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::LEFT, &posPlayer.x))
			{
				posPlayer.x += 3;
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
			posPlayer.x += 3;
			if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::RIGHT, &posPlayer.x))
			{
				posPlayer.x -= 3;
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
			else if(sprite->animation() == JUMP_UP || sprite->animation() == JUMP_FALL)
			{
				sprite->setFlipHorizontal(facingLeft);
				sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
			}
		}
	}

	// Jumping / gravity (always runs, even during attack)
	if(!skipMovement)
	{
		if(bJumping)
		{
			if(!bAttacking)
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
					if(!bAttacking)
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
				if(Game::instance().getKey(GLFW_KEY_UP) && !onLadder && !bAttacking)
				{
					bJumping = true;
					jumpAngle = 0;
					startY = posPlayer.y;
					sprite->setFlipHorizontal(facingLeft);
					sprite->changeAnimation(JUMP_UP);
				}
			}
		}
	}

	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - PLAYER_HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - PLAYER_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

	// Position attack sprite: feet at quad bottom (y=104), aligned with hitbox bottom
	// quadBottom = spritePos.y + 104 = posPlayer.y + 32  →  spritePos.y = posPlayer.y - 72
	// Body center x at 48 in quad, hitbox center at posPlayer.x + 16  →  spritePos.x = posPlayer.x - 32
	float atkRenderOffsetX = 32.f;
	float atkRenderOffsetY = 72.f;
	attackSprite->setFlipHorizontal(facingLeft);
	attackSprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - atkRenderOffsetX, float(tileMapDispl.y + posPlayer.y) - atkRenderOffsetY));
}

void Player::render()
{
	if(bAttacking)
		attackSprite->render();
	else
		sprite->render();
}

void Player::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Player::setPosition(const glm::vec2 &pos)
{
	posPlayer = pos;
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}



