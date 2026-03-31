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
#define PLAYER_FRAME_WIDTH 32
#define PLAYER_FRAME_HEIGHT 64
#define PLAYER_VISUAL_SCALE_X 1.2f
#define PLAYER_RUN_ANIM_FRAMES 8
#define PLAYER_JUMP_UP_ANIM_FRAMES 4
#define PLAYER_JUMP_FALL_ANIM_FRAMES 4
#define PLAYER_JUMP_FALL_START_FRAME 4
#define PLAYER_IDLE_ANIM_FRAMES 6
#define PLAYER_ATTACK_ANIM_FRAMES 5
#define PLAYER_ATTACK_FRAME_WIDTH 64
#define PLAYER_ATTACK_FRAME_HEIGHT 92
#define PLAYER_ATTACK_VISUAL_SCALE_X 1.2f
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
	spritesheet.loadFromFile("../images/Samurai_Animation.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);
	glm::vec2 frameSizeInTexture(
		float(PLAYER_FRAME_WIDTH) / float(spritesheet.width()),
		float(PLAYER_FRAME_HEIGHT) / float(spritesheet.height())
	);
	float visualFrameWidth = float(PLAYER_FRAME_WIDTH) * PLAYER_VISUAL_SCALE_X;
	sprite = Sprite::createSprite(glm::vec2(visualFrameWidth, float(PLAYER_FRAME_HEIGHT)), frameSizeInTexture, &spritesheet, &shaderProgram);
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
	float visualAttackFrameWidth = float(PLAYER_ATTACK_FRAME_WIDTH) * PLAYER_ATTACK_VISUAL_SCALE_X;
	attackSprite = Sprite::createSprite(glm::vec2(visualAttackFrameWidth, float(PLAYER_ATTACK_FRAME_HEIGHT)), attackFrameSize, &spritesheet, &shaderProgram);
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
	float renderOffsetX = 0.5f * (float(PLAYER_FRAME_WIDTH) * PLAYER_VISUAL_SCALE_X - float(Player::HITBOX_WIDTH));
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

}

void Player::update(int deltaTime)
{
	if (!alive) return;
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

	// Climbing logic:
	// - Stay attached to ladder while overlapping it
	// - Move only with UP/DOWN
	// - Hold position when no vertical input
	bool tryGrabLadder = bClimbing || ((upPressed || downPressed) && !leftPressed && !rightPressed);
	if(!skipMovement && onLadder && !bAttacking && (!bJumping || tryGrabLadder))
	{
		bool ladderJumpLeft = upAllowsJump && leftPressed && !rightPressed;
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
			bJumping = false; // Successfully grabbed ladder mid-air

		if(!bClimbing)
		{
			bClimbing = true;
			sprite->changeAnimation(CLIMB_STAIRS);
		}
		verticalVelocity = 0.0f;
		bJumping = false;

		// Keep the player centered on ladder column to avoid side-wall collisions.
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

	// Horizontal movement (blocked during attack and climbing)
	if(!skipMovement && !bAttacking)
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

	float renderOffsetX = 0.5f * (float(PLAYER_FRAME_WIDTH) * PLAYER_VISUAL_SCALE_X - float(Player::HITBOX_WIDTH));
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));

	// Position attack sprite with feet aligned to hitbox bottom.
	// hitbox bottom = posPlayer.y + 64, attack quad height = 92 -> sprite.y = posPlayer.y - 28
	// attack quad width = 64, hitbox width = 32 (centered) -> sprite.x = posPlayer.x - 16
	float atkRenderOffsetX = 0.5f * (float(PLAYER_ATTACK_FRAME_WIDTH) * PLAYER_ATTACK_VISUAL_SCALE_X - float(Player::HITBOX_WIDTH));
	float atkRenderOffsetY = float(PLAYER_ATTACK_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	attackSprite->setFlipHorizontal(facingLeft);
	attackSprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - atkRenderOffsetX, float(tileMapDispl.y + posPlayer.y) - atkRenderOffsetY));
}

void Player::render()
{
	if (!alive) return;
	// Blink during invincibility (skip every other 4-frame window)
	if (hitTimer > 0 && (hitTimer / 4) % 2 != 0) return;

	if(bAttacking)
		attackSprite->render();
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
		alive = false;
	}
	else
	{
		hitTimer = 90;   // ~1.5s invincibility
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
	float renderOffsetX = 0.5f * (float(PLAYER_FRAME_WIDTH) * PLAYER_VISUAL_SCALE_X - float(Player::HITBOX_WIDTH));
	float renderOffsetY = float(PLAYER_FRAME_HEIGHT - Player::HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posPlayer.x) - renderOffsetX, float(tileMapDispl.y + posPlayer.y) - renderOffsetY));
}



