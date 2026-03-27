#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include "Player.h"
#include "Game.h"


#define JUMP_ANGLE_STEP 4
#define JUMP_HEIGHT 96
#define FALL_STEP 4
#define PLAYER_FRAME_WIDTH 32
#define PLAYER_FRAME_HEIGHT 64
#define PLAYER_RUN_ANIM_FRAMES 8
#define PLAYER_JUMP_UP_ANIM_FRAMES 4
#define PLAYER_JUMP_FALL_ANIM_FRAMES 4
#define PLAYER_JUMP_FALL_START_FRAME 4
#define PLAYER_IDLE_ANIM_FRAMES 6
#define PLAYER_ATTACK_ANIM_FRAMES 5
#define PLAYER_ATTACK_FRAME_WIDTH 64
#define PLAYER_ATTACK_FRAME_HEIGHT 92
#define PLAYER_CLIMB_ANIM_FRAMES 2

#define ROW_RUN_TOP_PX 0
#define ROW_JUMP_TOP_PX 64
#define ROW_IDLE_TOP_PX 128
#define ROW_ATTACK_TOP_PX 192
#define ROW_CLIMB_TOP_PX 284


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
		float v = float(ROW_RUN_TOP_PX) / float(spritesheet.height());
		sprite->addKeyframe(MOVE_LEFT, glm::vec2(u, v));
		sprite->addKeyframe(MOVE_RIGHT, glm::vec2(u, v));
	}
	for(int frame = 0; frame < PLAYER_JUMP_UP_ANIM_FRAMES; ++frame)
	{
		float u = frame * frameSizeInTexture.x;
		float v = float(ROW_JUMP_TOP_PX) / float(spritesheet.height());
		sprite->addKeyframe(JUMP_UP, glm::vec2(u, v));
	}
	for(int frame = 0; frame < PLAYER_JUMP_FALL_ANIM_FRAMES; ++frame)
	{
		float u = (frame + PLAYER_JUMP_FALL_START_FRAME) * frameSizeInTexture.x;
		float v = float(ROW_JUMP_TOP_PX) / float(spritesheet.height());
		sprite->addKeyframe(JUMP_FALL, glm::vec2(u, v));
	}
	for(int frame = 0; frame < PLAYER_IDLE_ANIM_FRAMES; ++frame)
	{
		float u = frame * frameSizeInTexture.x;
		float v = float(ROW_IDLE_TOP_PX) / float(spritesheet.height());
		glm::vec2 idleFrame(u, v);
		sprite->addKeyframe(STAND_LEFT, idleFrame);
		sprite->addKeyframe(STAND_RIGHT, idleFrame);
	}

	// Climb stairs animation (2 frames)
	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	float climbV = float(ROW_CLIMB_TOP_PX) / texH;
	for(int frame = 0; frame < PLAYER_CLIMB_ANIM_FRAMES; ++frame)
	{
		float climbU = frame * frameSizeInTexture.x;
		sprite->addKeyframe(CLIMB_STAIRS, glm::vec2(climbU, climbV));
	}

	// Attack sprite (separate, larger frame: 96x104 to fit sword + slash)
	glm::vec2 attackFrameSize(
		float(PLAYER_ATTACK_FRAME_WIDTH) / texW,
		float(PLAYER_ATTACK_FRAME_HEIGHT) / texH
	);
	attackSprite = Sprite::createSprite(glm::ivec2(PLAYER_ATTACK_FRAME_WIDTH, PLAYER_ATTACK_FRAME_HEIGHT), attackFrameSize, &spritesheet, &shaderProgram);
	attackSprite->setNumberAnimations(1);
	attackSprite->setAnimationSpeed(0, 18);
	attackSprite->setAnimationLoop(0, false);
	float attackV = float(ROW_ATTACK_TOP_PX) / texH;
	for(int frame = 0; frame < PLAYER_ATTACK_ANIM_FRAMES; ++frame)
	{
		float attackU = frame * attackFrameSize.x;
		attackSprite->addKeyframe(0, glm::vec2(attackU, attackV));
	}
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
	bool onLadder = map->isOnLadder(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT));
	bool upPressed = Game::instance().getKey(GLFW_KEY_UP);
	bool downPressed = Game::instance().getKey(GLFW_KEY_DOWN);
	bool leftPressed = Game::instance().getKey(GLFW_KEY_LEFT);
	bool rightPressed = Game::instance().getKey(GLFW_KEY_RIGHT);
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

	// Climbing logic:
	// - Stay attached to ladder while overlapping it
	// - Move only with UP/DOWN
	// - Hold position when no vertical input
	if(!skipMovement && onLadder && !bAttacking && !bJumping)
	{
		bool ladderJumpLeft = upPressed && leftPressed && !rightPressed;
		bool ladderJumpRight = upPressed && rightPressed && !leftPressed;
		bool detachLeftOrRight = (leftPressed || rightPressed) && !upPressed;

		if(ladderJumpLeft || ladderJumpRight)
		{
			bClimbing = false;
			bJumping = true;
			jumpAngle = 0;
			startY = posPlayer.y;
			facingLeft = ladderJumpLeft;
			sprite->setFlipHorizontal(facingLeft);
			sprite->changeAnimation(JUMP_UP);
		}
		else if(detachLeftOrRight)
		{
			bClimbing = false;
		}
		else
		{
		if(!bClimbing)
		{
			bClimbing = true;
			sprite->changeAnimation(CLIMB_STAIRS);
		}

		// Keep the player centered on ladder column to avoid side-wall collisions.
		int tileSize = map->getTileSize();
		int ladderCol = (posPlayer.x + Player::HITBOX_WIDTH / 2) / tileSize;
		posPlayer.x = ladderCol * tileSize + (tileSize - Player::HITBOX_WIDTH) / 2;

		if(upPressed)
		{
			posPlayer.y -= 2;
			map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::UP, &posPlayer.y);
		}
		else if(downPressed)
		{
			posPlayer.y += 2;
			map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &posPlayer.y);
		}

		skipMovement = true;
		}
	}
	else if(bClimbing && !skipMovement)
	{
		bClimbing = false;
		sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
	}

	// Horizontal movement (blocked during attack and climbing)
	if(!skipMovement && !bAttacking)
	{
		if(leftPressed && !rightPressed)
		{
			facingLeft = true;
			sprite->setFlipHorizontal(true);
			if(!bJumping && sprite->animation() != MOVE_LEFT)
				sprite->changeAnimation(MOVE_LEFT);
			posPlayer.x -= 3;
			if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::LEFT, &posPlayer.x))
			{
				if(!bJumping)
					sprite->changeAnimation(STAND_LEFT);
			}
		}
		else if(rightPressed && !leftPressed)
		{
			facingLeft = false;
			sprite->setFlipHorizontal(false);
			if(!bJumping && sprite->animation() != MOVE_RIGHT)
				sprite->changeAnimation(MOVE_RIGHT);
			posPlayer.x += 3;
			if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::RIGHT, &posPlayer.x))
			{
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
			if(jumpAngle < 90)
			{
				posPlayer.y = int(startY - JUMP_HEIGHT * sin(3.14159f * jumpAngle / 180.f));
				if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::UP, &posPlayer.y))
				{
					jumpAngle = 90;
					if(!bAttacking)
						sprite->changeAnimation(JUMP_FALL);
				}
			}
			else
			{
				posPlayer.y += FALL_STEP;
				bJumping = !map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &posPlayer.y);
			}
		}
		else
		{
			posPlayer.y += FALL_STEP;
			bool dropThrough = downPressed && !onLadder;
			if(map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &posPlayer.y, dropThrough))
			{
				if(upPressed && !onLadder && !bAttacking)
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

	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

	// Position attack sprite with feet aligned to hitbox bottom.
	// hitbox bottom = posPlayer.y + 64, attack quad height = 92 -> sprite.y = posPlayer.y - 28
	// attack quad width = 64, hitbox width = 32 (centered) -> sprite.x = posPlayer.x - 16
	float atkRenderOffsetX = 16.f;
	float atkRenderOffsetY = 28.f;
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

glm::vec4 Player::getAttackHitbox() const
{
	float attackRange = 24.f;
	if (facingLeft)
		return glm::vec4(posPlayer.x - attackRange, posPlayer.y, attackRange, HITBOX_HEIGHT);
	else
		return glm::vec4(posPlayer.x + HITBOX_WIDTH, posPlayer.y, attackRange, HITBOX_HEIGHT);
}

void Player::setPosition(const glm::vec2 &pos)
{
	posPlayer = pos;
	float renderOffsetX = 0.5f * float(PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}



