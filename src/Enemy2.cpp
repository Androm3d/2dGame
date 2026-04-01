#include <cmath>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy2.h"
#include "EnemyNavigator.h"


#include "GameConstants.h"

static const float E2_JUMP_VELOCITY = std::sqrt(2.0f * GRAVITY * float(E2_JUMP_HEIGHT));
static const float E2_SPRING_JUMP_VELOCITY = E2_JUMP_VELOCITY * std::sqrt(3.0f);
static const int E2_DROP_THROUGH_MS = PLAYER_DROP_THROUGH_MS;


enum Enemy2Anims
{
	RUN, CLIMB, JUMP_UP, JUMP_FALL, ATTACK, HURT, DEATH
};


// =====================================================================
//  Enemy2 implementation
// =====================================================================

Enemy2::Enemy2()
{
	sprite = NULL;
	stairsSprite = NULL;
}

Enemy2::~Enemy2()
{
	if (sprite != NULL)
		delete sprite;
	if (stairsSprite != NULL)
		delete stairsSprite;
}

void Enemy2::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	resetBaseState(tileMapPos);
	bAttacking    = false;
	attackCooldown = 0;

	spritesheet.loadFromFile("../images/Enemy2.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 fs(float(E2_FRAME_WIDTH) / texW, float(E2_FRAME_HEIGHT) / texH);

	sprite = Sprite::createSprite(glm::ivec2(E2_RENDER_WIDTH, E2_RENDER_HEIGHT), fs, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(7);

	sprite->setAnimationSpeed(RUN,       10); sprite->setAnimationLoop(RUN,       true);
	sprite->setAnimationSpeed(CLIMB,      8); sprite->setAnimationLoop(CLIMB,     true);
	sprite->setAnimationSpeed(JUMP_UP,   10); sprite->setAnimationLoop(JUMP_UP,   false);
	sprite->setAnimationSpeed(JUMP_FALL, 10); sprite->setAnimationLoop(JUMP_FALL, false);
	sprite->setAnimationSpeed(ATTACK,    12); sprite->setAnimationLoop(ATTACK,    false);
	sprite->setAnimationSpeed(HURT,      10); sprite->setAnimationLoop(HURT,      false);
	sprite->setAnimationSpeed(DEATH,      8); sprite->setAnimationLoop(DEATH,     false);

	// Row 0: Run (8 frames)
	for (int f = 0; f < E2_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN, glm::vec2(f * fs.x, float(E2_ROW_RUN) / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(0.0f, float(E2_ROW_RUN) / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(fs.x, float(E2_ROW_RUN) / texH));
	// Row 1: Jump up (frames 0-3)
	for (int f = 0; f < E2_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP, glm::vec2(f * fs.x, float(E2_ROW_JUMP) / texH));
	// Row 1: Jump fall (frames 4-6)
	for (int f = 0; f < E2_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL, glm::vec2((f + E2_JUMP_UP_FRAMES) * fs.x, float(E2_ROW_JUMP) / texH));
	// Row 2: Attack (4 frames)
	for (int f = 0; f < E2_ATTACK_FRAMES; ++f)
		sprite->addKeyframe(ATTACK, glm::vec2(f * fs.x, float(E2_ROW_ATTACK) / texH));
	// Row 3: Hurt (2 frames)
	for (int f = 0; f < E2_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT, glm::vec2(f * fs.x, float(E2_ROW_HURT) / texH));
	// Row 4: Death (6 frames)
	for (int f = 0; f < E2_DEATH_FRAMES; ++f)
		sprite->addKeyframe(DEATH, glm::vec2(f * fs.x, float(E2_ROW_DEATH) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);

	stairsSpritesheet.loadFromFile("../images/Enemy2_Stairs.png", TEXTURE_PIXEL_FORMAT_RGBA);
	stairsSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setMinFilter(GL_NEAREST);
	stairsSpritesheet.setMagFilter(GL_NEAREST);
	stairsSprite = Sprite::createSprite(
		glm::ivec2(E2_STAIRS_RENDER_WIDTH, E2_RENDER_HEIGHT),
		glm::vec2(64.0f / float(stairsSpritesheet.width()), 64.0f / float(stairsSpritesheet.height())),
		&stairsSpritesheet,
		&shaderProgram
	);
	stairsSprite->setNumberAnimations(1);
	stairsSprite->setAnimationSpeed(0, 8);
	stairsSprite->setAnimationLoop(0, true);
	stairsSprite->addKeyframe(0, glm::vec2(0.0f, 0.0f));
	stairsSprite->addKeyframe(0, glm::vec2(64.0f / float(stairsSpritesheet.width()), 0.0f));
	stairsSprite->changeAnimation(0);
}


// =====================================================================
//  BFS pathfinding (same algorithm as Enemy1)
// =====================================================================

void Enemy2::computePath(const glm::vec2 &playerPos)
{
	EnemyNavParams navParams;
	navParams.hitboxWidth = E2_HITBOX_WIDTH;
	navParams.jumpHeight = E2_JUMP_HEIGHT;
	navParams.horizontalSpeed = E2_SPEED;
	navParams.dashDistanceBasePx = int(E2_DASH_DISTANCE_BASE);
	navParams.jumpAngleStep = JUMP_ANGLE_STEP;
	navParams.maxIterations = 2000;
	navParams.maxJumpAngle = 180;
	navParams.playerCenterOffsetX = 16;
	navParams.playerFeetOffsetY = 63;
	computePathCommon(playerPos, navParams);
}


// =====================================================================
//  Update
// =====================================================================

void Enemy2::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
	ladderAnimActive = false;
	float dt = float(deltaTime) / 1000.0f;

	float renderOffsetX = 0.5f * float(E2_RENDER_WIDTH - E2_HITBOX_WIDTH);
	float renderOffsetY = float(E2_RENDER_HEIGHT - E2_HITBOX_HEIGHT);

	// --- Death animation, then vanish ---
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished())
			bDying = false;
		return;
	}

	if (hitTimer > 0) hitTimer--;
	if (springCooldown > 0) springCooldown--;
	if (dashCooldown > 0) dashCooldown--;
	if (dropThroughTimerMs > 0) dropThroughTimerMs -= deltaTime;
	if (dropCommitTimerMs > 0) {
		dropCommitTimerMs -= deltaTime;
		if (dropCommitTimerMs < 0) dropCommitTimerMs = 0;
		pathRecalcTimer = PATH_RECALC_FRAMES;
	}

	{
		glm::ivec2 groundedProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		bool dropThrough = dropThroughTimerMs > 0;
		onGround = map->checkCollision(groundedProbe, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY, dropThrough);
		if (onGround && verticalVelocity >= 0.0f) {
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	{
		glm::ivec2 triggerPos(int(posEnemyF.x), int(posEnemyF.y));
		if (springCooldown == 0 && map->isOnSpring(triggerPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT))) {
			verticalVelocity = -E2_SPRING_JUMP_VELOCITY;
			onGround = false;
			bJumping = true;
			springCooldown = 120;
		}
		bool dashLeft = false;
		if (dashCooldown == 0 && map->isOnDash(triggerPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), &dashLeft)) {
			int dir = dashLeft ? -1 : 1;
			dashTimeLeftMs = E2_DASH_DURATION_MS;
			dashVelocityStart = dir * (E2_DASH_DISTANCE_BASE * 3.0f) / (0.5f * float(E2_DASH_DURATION_MS));
			dashVelocity = dashVelocityStart;
			dashCooldown = 120;
		}
	}

	// --- Knockback while hurt animation plays ---
	if (knockbackFrames > 0)
	{
		knockbackFrames--;
		sprite->update(deltaTime);
		posEnemyF.x += knockbackDir * KNOCKBACK_SPEED;
		glm::ivec2 kbPos(int(posEnemyF.x), int(posEnemyF.y));
		if (knockbackDir < 0)
			map->checkCollision(kbPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::LEFT,  &kbPos.x);
		else
			map->checkCollision(kbPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::RIGHT, &kbPos.x);
		posEnemyF.x = float(kbPos.x);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		kbPos = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(kbPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &kbPos.y);
		if (onGround) {
			posEnemyF.y = float(kbPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = kbPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		return;
	}

	// --- Wait for hurt animation to finish ---
	if (sprite->animation() == HURT)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished())
			sprite->changeAnimation(RUN);
		return;
	}

	if (attackCooldown > 0) attackCooldown--;

	// --- Melee attack: freeze until animation finishes ---
	if (bAttacking)
	{
		sprite->update(deltaTime);
		// gravity still applies while attacking
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished())
		{
			bAttacking = false;
			attackCooldown = E2_ATTACK_COOLDOWN;
			sprite->changeAnimation(RUN);
		}
		return;
	}

	// --- Check if we should start a melee attack ---
	if (attackCooldown <= 0 && onGround && !bJumping)
	{
		float dx = playerPos.x - posEnemy.x;
		float dy = playerPos.y - posEnemy.y;
		bool playerInFront = (facingLeft && dx < 0) || (!facingLeft && dx > 0);

		if (playerInFront && std::abs(dx) < E2_MELEE_DETECT_RANGE && std::abs(dy) < E2_MELEE_DETECT_VERT)
		{
			bAttacking = true;
			sprite->changeAnimation(ATTACK);
			return;
		}
	}

	BaseEnemyMoveConfig moveConfig;
	moveConfig.hitboxWidth = E2_HITBOX_WIDTH;
	moveConfig.hitboxHeight = E2_HITBOX_HEIGHT;
	moveConfig.moveSpeed = E2_SPEED;
	moveConfig.jumpVelocity = E2_JUMP_VELOCITY;
	moveConfig.dashDurationMs = E2_DASH_DURATION_MS;
	moveConfig.runAnim = RUN;
	moveConfig.climbAnim = CLIMB;
	moveConfig.jumpUpAnim = JUMP_UP;
	moveConfig.jumpFallAnim = JUMP_FALL;
	moveConfig.pathRecalcFrames = PATH_RECALC_FRAMES;
	moveConfig.renderOffsetX = renderOffsetX;
	moveConfig.renderOffsetY = renderOffsetY;
	updatePathMovementCommon(deltaTime, dt, playerPos, sprite, moveConfig);

	float stairsRenderOffsetX = 0.5f * float(E2_STAIRS_RENDER_WIDTH - E2_HITBOX_WIDTH);
	float stairsRenderOffsetY = float(E2_RENDER_HEIGHT - E2_HITBOX_HEIGHT);
	stairsSprite->setFlipHorizontal(facingLeft);
	stairsSprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - stairsRenderOffsetX, float(tileMapDispl.y + posEnemy.y) - stairsRenderOffsetY));
	if (ladderAnimActive)
		stairsSprite->update(deltaTime);
}

void Enemy2::render()
{
	if (!alive && !bDying) return;

	// Blink on hit (unless dying)
	if (!bDying && hitTimer > (HIT_INVINCIBILITY_FRAMES - HIT_BLINK_FRAMES) && (hitTimer / 4) % 2 != 0) return;

	if (ladderAnimActive)
		stairsSprite->render();
	else
		sprite->render();
}

void Enemy2::setPosition(const glm::vec2 &pos)
{
	setPositionCommon(pos, E2_RENDER_WIDTH, E2_RENDER_HEIGHT, E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT, sprite);
}

void Enemy2::takeDamage(int knockDir)
{
	if (!alive) return;
	health--;
	bAttacking = false;
	if (health <= 0)
	{
		alive = false;
		bDying = true;
		knockbackFrames = 0;
		sprite->changeAnimation(DEATH);
	}
	else
	{
		hitTimer = HIT_INVINCIBILITY_FRAMES;
		knockbackFrames = KNOCKBACK_FRAMES;
		knockbackDir = knockDir;
		sprite->changeAnimation(HURT);
	}
}

glm::vec4 Enemy2::getHitbox() const
{
	return glm::vec4(posEnemy.x, posEnemy.y, E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT);
}

glm::vec4 Enemy2::getMeleeHitbox() const
{
	// Sword reach extends in front of the enemy
	if (facingLeft)
		return glm::vec4(posEnemy.x - E2_MELEE_HITBOX_REACH, posEnemy.y, E2_MELEE_HITBOX_REACH, E2_HITBOX_HEIGHT);
	else
		return glm::vec4(posEnemy.x + E2_HITBOX_WIDTH, posEnemy.y, E2_MELEE_HITBOX_REACH, E2_HITBOX_HEIGHT);
}
