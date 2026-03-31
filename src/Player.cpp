#include <cmath>
#include <iostream>
#include <GL/glew.h>
#include "Player.h"
#include "Game.h"


#define JUMP_ANGLE_STEP 4
#define JUMP_HEIGHT 96
#define SPRING_JUMP_MULTIPLIER 3
#define DASH_DURATION_MS 1000
#define DASH_DISTANCE_BASE 60.0f

static const float PLAYER_GRAVITY = 1400.0f;
static const float PLAYER_JUMP_VELOCITY = std::sqrt(2.0f * PLAYER_GRAVITY * float(JUMP_HEIGHT));
static const float SPRING_JUMP_VELOCITY = PLAYER_JUMP_VELOCITY * std::sqrt(float(SPRING_JUMP_MULTIPLIER));
static const float PLAYER_WALK_SPEED = 180.0f;
static const float PLAYER_CLIMB_SPEED = 120.0f;
static const int PLAYER_DROP_THROUGH_MS = 180;
static const float PLAYER_DROP_THROUGH_NUDGE = 4.0f;
#define FALL_STEP 4

// Main spritesheet (Samurai.png, 512x512, frame 56x80)
#define PLAYER_FRAME_WIDTH    56
#define PLAYER_FRAME_HEIGHT   80

// Attack spritesheet (Samurai_Attack.png, 512x128, frame 88x106)
#define PLAYER_ATTACK_FRAME_WIDTH  88
#define PLAYER_ATTACK_FRAME_HEIGHT 106

// Animation frame counts
#define PLAYER_IDLE_FRAMES       6
#define PLAYER_RUN_FRAMES        8
#define PLAYER_JUMP_UP_FRAMES    5
#define PLAYER_JUMP_FALL_FRAMES  4
#define PLAYER_HURT_FRAMES       3
#define PLAYER_DEAD_FRAMES       6
#define PLAYER_PROTECT_FRAMES    2
#define PLAYER_ATTACK_FRAMES     5
#define PARRY_FRAMES    22
#define PARRY_COOLDOWN  55

// Row Y positions in Samurai.png
#define ROW_IDLE_PX     0
#define ROW_RUN_PX      80
#define ROW_JUMP_PX     160
#define ROW_HURT_PX     240
#define ROW_DEAD_PX     320
#define ROW_PROTECT_PX  400


enum PlayerAnims
{
	STAND_LEFT, STAND_RIGHT, MOVE_LEFT, MOVE_RIGHT, JUMP_UP, JUMP_FALL, CLIMB_STAIRS, HURT, DEAD, PROTECT
};


Player::Player()
{
	sprite = NULL;
	attackSprite = NULL;
	stairSprite = NULL;
	map = NULL;
}

Player::~Player()
{
	if (sprite != NULL)
		delete sprite;
	if (attackSprite != NULL)
		delete attackSprite;
	if (stairSprite != NULL)
		delete stairSprite;
}

void Player::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	bJumping = false;
	bClimbing = false;
	bAttacking = false;
	bProtecting = false;
	bDying = false;
	bHurt = false;
	prevShift = false;
	parryTimer = 0;
	parryCooldown = 0;
	facingLeft = false;
	alive = true;
	hitTimer = 0;
	springCooldown = 0;
	dashCooldown = 0;
	dropThroughTimerMs = 0;
	springTriggered = false;
	jumpHeight = JUMP_HEIGHT;
	dashVelocity = 0.0f;
	dashVelocityStart = 0.0f;
	dashTimeLeftMs = 0;
	posPlayerF = glm::vec2(0.0f, 0.0f);
	verticalVelocity = 0.0f;

	// --- Main spritesheet ---
	spritesheet.loadFromFile("../images/Samurai.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 frameSize(float(PLAYER_FRAME_WIDTH) / texW, float(PLAYER_FRAME_HEIGHT) / texH);

	sprite = Sprite::createSprite(glm::ivec2(PLAYER_FRAME_WIDTH, PLAYER_FRAME_HEIGHT), frameSize, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(10);

	// Speeds
	sprite->setAnimationSpeed(STAND_LEFT,    8);
	sprite->setAnimationSpeed(STAND_RIGHT,   8);
	sprite->setAnimationSpeed(MOVE_LEFT,    12);
	sprite->setAnimationSpeed(MOVE_RIGHT,   12);
	sprite->setAnimationSpeed(JUMP_UP,      12);
	sprite->setAnimationSpeed(JUMP_FALL,    10);
	sprite->setAnimationSpeed(CLIMB_STAIRS,  8);
	sprite->setAnimationSpeed(HURT,         10);
	sprite->setAnimationSpeed(DEAD,          8);
	sprite->setAnimationSpeed(PROTECT,       8);

	// Loops
	sprite->setAnimationLoop(STAND_LEFT,   true);
	sprite->setAnimationLoop(STAND_RIGHT,  true);
	sprite->setAnimationLoop(MOVE_LEFT,    true);
	sprite->setAnimationLoop(MOVE_RIGHT,   true);
	sprite->setAnimationLoop(JUMP_UP,      false);
	sprite->setAnimationLoop(JUMP_FALL,    false);
	sprite->setAnimationLoop(CLIMB_STAIRS, true);
	sprite->setAnimationLoop(HURT,         false);
	sprite->setAnimationLoop(DEAD,         false);
	sprite->setAnimationLoop(PROTECT,      false);

	// Idle (row 0) — used for STAND_LEFT and STAND_RIGHT
	for (int f = 0; f < PLAYER_IDLE_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_IDLE_PX) / texH;
		sprite->addKeyframe(STAND_LEFT,  glm::vec2(u, v));
		sprite->addKeyframe(STAND_RIGHT, glm::vec2(u, v));
	}

	// Run (row 1) — used for MOVE_LEFT and MOVE_RIGHT
	for (int f = 0; f < PLAYER_RUN_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_RUN_PX) / texH;
		sprite->addKeyframe(MOVE_LEFT,  glm::vec2(u, v));
		sprite->addKeyframe(MOVE_RIGHT, glm::vec2(u, v));
	}

	// Jump up (row 2, frames 0–4)
	for (int f = 0; f < PLAYER_JUMP_UP_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_JUMP_PX) / texH;
		sprite->addKeyframe(JUMP_UP, glm::vec2(u, v));
	}

	// Jump fall (row 2, frames 5–8)
	for (int f = 0; f < PLAYER_JUMP_FALL_FRAMES; ++f)
	{
		float u = (f + PLAYER_JUMP_UP_FRAMES) * frameSize.x;
		float v = float(ROW_JUMP_PX) / texH;
		sprite->addKeyframe(JUMP_FALL, glm::vec2(u, v));
	}

	// Hurt (row 3)
	for (int f = 0; f < PLAYER_HURT_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_HURT_PX) / texH;
		sprite->addKeyframe(HURT, glm::vec2(u, v));
	}

	// Dead (row 4)
	for (int f = 0; f < PLAYER_DEAD_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_DEAD_PX) / texH;
		sprite->addKeyframe(DEAD, glm::vec2(u, v));
	}

	// Protection (row 5) — used for PROTECT and CLIMB_STAIRS placeholder
	for (int f = 0; f < PLAYER_PROTECT_FRAMES; ++f)
	{
		float u = f * frameSize.x;
		float v = float(ROW_PROTECT_PX) / texH;
		sprite->addKeyframe(PROTECT,      glm::vec2(u, v));
		sprite->addKeyframe(CLIMB_STAIRS, glm::vec2(u, v));
	}

	sprite->changeAnimation(STAND_RIGHT);
	sprite->setFlipHorizontal(false);

	// --- Attack spritesheet ---
	attackSpritesheet.loadFromFile("../images/Samurai_Attack.png", TEXTURE_PIXEL_FORMAT_RGBA);
	attackSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	attackSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	attackSpritesheet.setMinFilter(GL_NEAREST);
	attackSpritesheet.setMagFilter(GL_NEAREST);

	float atkTexW = float(attackSpritesheet.width());
	float atkTexH = float(attackSpritesheet.height());
	glm::vec2 atkFrameSize(float(PLAYER_ATTACK_FRAME_WIDTH) / atkTexW, float(PLAYER_ATTACK_FRAME_HEIGHT) / atkTexH);

	attackSprite = Sprite::createSprite(glm::ivec2(PLAYER_ATTACK_FRAME_WIDTH, PLAYER_ATTACK_FRAME_HEIGHT), atkFrameSize, &attackSpritesheet, &shaderProgram);
	attackSprite->setNumberAnimations(1);
	attackSprite->setAnimationSpeed(0, 18);
	attackSprite->setAnimationLoop(0, false);
	for (int f = 0; f < PLAYER_ATTACK_FRAMES; ++f)
	{
		float u = f * atkFrameSize.x;
		attackSprite->addKeyframe(0, glm::vec2(u, 0.f));
	}
	attackSprite->changeAnimation(0);
	attackSprite->setFlipHorizontal(false);

	// --- Stair spritesheet (Samurai_Stair_Animation.png, 64x64, 2 frames) ---
	stairSpritesheet.loadFromFile("../images/Samurai_Stair_Animation.png", TEXTURE_PIXEL_FORMAT_RGBA);
	stairSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	stairSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	stairSpritesheet.setMinFilter(GL_NEAREST);
	stairSpritesheet.setMagFilter(GL_NEAREST);
	{
		float sTexW = float(stairSpritesheet.width());
		float sTexH = float(stairSpritesheet.height());
		glm::vec2 sFrameSize(64.f / sTexW, 64.f / sTexH);
		stairSprite = Sprite::createSprite(glm::ivec2(96, 96), sFrameSize, &stairSpritesheet, &shaderProgram);
		stairSprite->setNumberAnimations(1);
		stairSprite->setAnimationSpeed(0, 8);
		stairSprite->setAnimationLoop(0, true);
		for (int f = 0; f < 2; ++f)
			stairSprite->addKeyframe(0, glm::vec2(f * sFrameSize.x, 0.f));
		stairSprite->changeAnimation(0);
	}

	tileMapDispl = tileMapPos;
	float renderOffsetX = 0.5f * (PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}

void Player::update(int deltaTime)
{
	if (!alive) return;
	if (bDying)
	{
		sprite->update(deltaTime);
		if (sprite->animationFinished())
			alive = false;
		// Position sprite so it stays in place
		float renderOffsetX = 0.5f * (PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
		float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
		return;
	}
	if (bHurt)
	{
		sprite->update(deltaTime);
		if (sprite->animationFinished())
		{
			bHurt = false;
			sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
		}
		// Still apply gravity while hurt
		posPlayer.y += FALL_STEP;
		bool dropThrough = false;
		map->checkCollision(posPlayer, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &posPlayer.y, dropThrough);
		float renderOffsetX = 0.5f * (PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
		float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
		if (hitTimer > 0) hitTimer--;
		return;
	}
	if (hitTimer > 0) hitTimer--;
	if (springCooldown > 0) springCooldown--;
	if (dashCooldown > 0) dashCooldown--;
	if (dropThroughTimerMs > 0) dropThroughTimerMs -= deltaTime;
	springTriggered = false;
	float dt = float(deltaTime) / 1000.0f;


	sprite->update(deltaTime);
	glm::ivec2 posI(int(posPlayerF.x), int(posPlayerF.y));
	bool onLadder = map->isOnLadder(posI, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT));
	bool onSpring = map->isOnSpring(posI, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT));
	bool dashFacingLeft = false;
	bool onDash = map->isOnDash(posI, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), &dashFacingLeft);
	bool upPressed = Game::instance().getKey(GLFW_KEY_UP);
	bool upAllowsJump = upPressed && !Game::instance().isJumpInputBlocked();
	bool downPressed = Game::instance().getKey(GLFW_KEY_DOWN);
	bool leftPressed = Game::instance().getKey(GLFW_KEY_LEFT);
	bool rightPressed = Game::instance().getKey(GLFW_KEY_RIGHT);
	bool skipMovement = false;
	int groundedY = 0;
	glm::ivec2 groundProbe = posI;
	groundProbe.y += 1;
	int groundedYSolid = 0;
	bool onGroundSolid = map->checkCollision(groundProbe, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &groundedYSolid, false);
	bool dropThrough = (downPressed || dropThroughTimerMs > 0) && !onLadder;
	bool onGround = map->checkCollision(groundProbe, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY, dropThrough);
	if (downPressed && onGroundSolid && !onLadder)
	{
		dropThroughTimerMs = PLAYER_DROP_THROUGH_MS;
		posPlayerF.y += PLAYER_DROP_THROUGH_NUDGE;
		verticalVelocity = std::max(verticalVelocity, 80.0f);
		onGround = false;
	}
	if (onGround && verticalVelocity >= 0.0f) {
		posPlayerF.y = float(groundedY);
		verticalVelocity = 0.0f;
		bJumping = false;
	}

	// Attack with SPACE
	if(Game::instance().getKey(GLFW_KEY_SPACE) && !bAttacking && !bClimbing && Game::instance().hasSword)
	{
		bAttacking = true;
		bProtecting = false;
		parryTimer = 0;
		attackSprite->changeAnimation(0);
		attackSprite->setFlipHorizontal(facingLeft);
	}
	if(bAttacking)
	{
		attackSprite->update(deltaTime);
		if(attackSprite->animationFinished())
			bAttacking = false;
	}

	// Parry with SHIFT (edge trigger — one press = one parry window)
	if(parryCooldown > 0) parryCooldown--;
	if(parryTimer > 0)
	{
		parryTimer--;
		if(parryTimer == 0)
		{
			bProtecting = false;
			sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
		}
	}
	bool shiftHeld = Game::instance().getKey(GLFW_KEY_LEFT_SHIFT) || Game::instance().getKey(GLFW_KEY_RIGHT_SHIFT);
	if(shiftHeld && !prevShift && !bAttacking && !bClimbing && parryCooldown == 0)
	{
		bProtecting = true;
		parryTimer = PARRY_FRAMES;
		parryCooldown = PARRY_COOLDOWN;
		sprite->changeAnimation(PROTECT);
	}
	prevShift = shiftHeld;

	// Spring boost
	if (onSpring && springCooldown == 0) {
		verticalVelocity = -SPRING_JUMP_VELOCITY;
		bJumping = true;
		onGround = false;
		springCooldown = 120;
		springTriggered = true;
	}

	// Dash boost
	if (onDash && dashCooldown == 0) {
		int dashDir = dashFacingLeft ? -1 : 1;
		dashTimeLeftMs = DASH_DURATION_MS;
		dashVelocityStart = dashDir * (DASH_DISTANCE_BASE * 3.0f) / (0.5f * float(DASH_DURATION_MS));
		dashVelocity = dashVelocityStart;
		dashCooldown = 120;
	}

	// Climbing logic
	bool tryGrabLadder = bClimbing || ((upPressed || downPressed) && !leftPressed && !rightPressed);
	if(!skipMovement && onLadder && !bAttacking && (!bJumping || tryGrabLadder))
	{
		bool ladderJumpLeft  = upAllowsJump && leftPressed  && !rightPressed;
		bool ladderJumpRight = upAllowsJump && rightPressed && !leftPressed;
		bool detachLeftOrRight = (leftPressed || rightPressed) && !upPressed;

		if(ladderJumpLeft || ladderJumpRight)
		{
			bClimbing = false;
			bJumping = true;
			verticalVelocity = -PLAYER_JUMP_VELOCITY;
			onGround = false;
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
			if(bJumping)
				bJumping = false;

		if(!bClimbing)
		{
			bClimbing = true;
			sprite->changeAnimation(CLIMB_STAIRS);
		}
		verticalVelocity = 0.0f;
		bJumping = false;

		int tileSize = map->getTileSize();
		int ladderCol = (int(posPlayerF.x) + Player::HITBOX_WIDTH / 2) / tileSize;
		posPlayerF.x = float(ladderCol * tileSize + (tileSize - Player::HITBOX_WIDTH) / 2);

		if(upPressed)
		{
			posPlayerF.y -= PLAYER_CLIMB_SPEED * dt;
			glm::ivec2 climbPos(int(posPlayerF.x), int(posPlayerF.y));
			map->checkCollision(climbPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::UP, &climbPos.y);
			posPlayerF.y = float(climbPos.y);
		}
		else if(downPressed)
		{
			posPlayerF.y += PLAYER_CLIMB_SPEED * dt;
			glm::ivec2 climbPos(int(posPlayerF.x), int(posPlayerF.y));
			map->checkCollision(climbPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &climbPos.y);
			posPlayerF.y = float(climbPos.y);
		}

			skipMovement = true;
		}
	}
	else if(bClimbing && !skipMovement)
	{
		bClimbing = false;
		sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
	}

	// Horizontal movement
	if(!skipMovement && !bAttacking && !bProtecting)
	{
		const float walkSpeed = PLAYER_WALK_SPEED * dt;
		if(leftPressed && !rightPressed)
		{
			facingLeft = true;
			sprite->setFlipHorizontal(true);
			if(!bJumping && sprite->animation() != MOVE_LEFT)
				sprite->changeAnimation(MOVE_LEFT);
			bool hitWall = false;
			int steps = int(std::ceil(std::abs(walkSpeed)));
			for(int step = 0; step < steps; ++step)
			{
				posPlayerF.x -= 1.0f;
				glm::ivec2 movePos(int(posPlayerF.x), int(posPlayerF.y));
				if(map->checkCollision(movePos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::LEFT, &movePos.x))
				{
					posPlayerF.x = float(movePos.x);
					hitWall = true;
					break;
				}
			}
			if(hitWall && !bJumping)
				sprite->changeAnimation(STAND_LEFT);
		}
		else if(rightPressed && !leftPressed)
		{
			facingLeft = false;
			sprite->setFlipHorizontal(false);
			if(!bJumping && sprite->animation() != MOVE_RIGHT)
				sprite->changeAnimation(MOVE_RIGHT);
			bool hitWall = false;
			int steps = int(std::ceil(std::abs(walkSpeed)));
			for(int step = 0; step < steps; ++step)
			{
				posPlayerF.x += 1.0f;
				glm::ivec2 movePos(int(posPlayerF.x), int(posPlayerF.y));
				if(map->checkCollision(movePos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::RIGHT, &movePos.x))
				{
					posPlayerF.x = float(movePos.x);
					hitWall = true;
					break;
				}
			}
			if(hitWall && !bJumping)
				sprite->changeAnimation(STAND_RIGHT);
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

	if (!skipMovement && dashTimeLeftMs > 0 && dashVelocity != 0.0f)
	{
		float ratio = float(dashTimeLeftMs) / float(DASH_DURATION_MS);
		dashVelocity = dashVelocityStart * ratio;
		float dashDelta = dashVelocity * float(deltaTime);
		int dashSteps = int(std::ceil(std::abs(dashDelta)));
		int dashStepDir = (dashDelta < 0.0f) ? -1 : 1;
		for (int step = 0; step < dashSteps; ++step)
		{
			posPlayerF.x += float(dashStepDir);
			glm::ivec2 dashPos(int(posPlayerF.x), int(posPlayerF.y));
			if (map->checkCollision(dashPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), dashStepDir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x))
			{
				posPlayerF.x = float(dashPos.x);
				dashTimeLeftMs = 0;
				dashVelocity = 0.0f;
				break;
			}
		}
	}

	// Jumping / gravity (time-based, always runs, even during attack)
	if(!skipMovement && !bClimbing)
	{
		if(upAllowsJump && onGround && !onLadder && !bAttacking)
		{
			verticalVelocity = -PLAYER_JUMP_VELOCITY;
			bJumping = true;
			onGround = false;
			sprite->setFlipHorizontal(facingLeft);
			sprite->changeAnimation(JUMP_UP);
		}

		verticalVelocity += PLAYER_GRAVITY * dt;
		posPlayerF.y += verticalVelocity * dt;

		glm::ivec2 fallPos(int(posPlayerF.x), int(posPlayerF.y));
		if (verticalVelocity > 0.0f)
		{
			bool dropThrough = downPressed && !onLadder;
			if (map->checkCollision(fallPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y, dropThrough))
			{
				posPlayerF.y = float(fallPos.y);
				verticalVelocity = 0.0f;
				onGround = true;
				bJumping = false;
			}
			else
			{
				glm::ivec2 nearGroundProbe = fallPos;
				nearGroundProbe.y += 1;
				int nearGroundY = 0;
				if (map->checkCollision(nearGroundProbe, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &nearGroundY, dropThrough))
				{
					posPlayerF.y = float(nearGroundY);
					verticalVelocity = 0.0f;
					onGround = true;
					bJumping = false;
				}
				else
					onGround = false;
			}
		}
		else if (verticalVelocity < 0.0f)
		{
			if (map->checkCollision(fallPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::UP, &fallPos.y))
			{
				posPlayerF.y = float(fallPos.y);
				verticalVelocity = 0.0f;
			}
		}

		if (!bAttacking)
		{
			if (!onGround)
			{
				bJumping = true;
				sprite->setFlipHorizontal(facingLeft);
				sprite->changeAnimation(verticalVelocity < 0.0f ? JUMP_UP : JUMP_FALL);
			}
			else if (sprite->animation() == JUMP_UP || sprite->animation() == JUMP_FALL)
			{
				sprite->changeAnimation(facingLeft ? STAND_LEFT : STAND_RIGHT);
			}
		}
	}

	if (dashTimeLeftMs > 0)
	{
		dashTimeLeftMs -= deltaTime;
		if (dashTimeLeftMs <= 0)
		{
			dashTimeLeftMs = 0;
			dashVelocity = 0.0f;
		}
	}

	posPlayer = glm::ivec2(int(posPlayerF.x), int(posPlayerF.y));

	float renderOffsetX = 0.5f * (PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

	// Position attack sprite (center horizontally, feet-aligned)
	float atkOffsetX = 0.5f * (PLAYER_ATTACK_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float atkOffsetY = float(PLAYER_ATTACK_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	attackSprite->setFlipHorizontal(facingLeft);
	attackSprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - atkOffsetX, float(tileMapDispl.y + posPlayer.y) - atkOffsetY));

	// Position stair sprite (64x64, centered horizontally, feet-aligned)
	stairSprite->setFlipHorizontal(facingLeft);
	stairSprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - 32.f, float(tileMapDispl.y + posPlayer.y) - 32.f));
	if(bClimbing)
		stairSprite->update(deltaTime);
}

void Player::render()
{
	if (!alive) return;
	if (!bDying && hitTimer > 0 && (hitTimer / 4) % 2 != 0) return;

	if(bAttacking)
		attackSprite->render();
	else if(bClimbing)
		stairSprite->render();
	else
		sprite->render();
}

void Player::takeDamage()
{
	if (!alive || hitTimer > 0 || Game::instance().godMode) return;
	if (Game::instance().hasShield) {
		Game::instance().hasShield = false;
		hitTimer = 90;
		return;
	}
	Game::instance().lives--;
	if (Game::instance().lives <= 0)
	{
		bDying = true;
		sprite->changeAnimation(DEAD);
		sprite->setAnimationLoop(DEAD, false);
	}
	else
	{
		hitTimer = 90;
		bHurt = true;
		bAttacking = false;
		bProtecting = false;
		sprite->changeAnimation(HURT);
	}
}

void Player::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

bool Player::consumeSpringTrigger()
{
	if (!springTriggered)
		return false;
	springTriggered = false;
	return true;
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
	posPlayerF = pos;
	posPlayer = glm::ivec2(int(posPlayerF.x), int(posPlayerF.y));
	float renderOffsetX = 0.5f * (PLAYER_FRAME_WIDTH - Player::HITBOX_WIDTH);
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}
