#include <cmath>
#include <iostream>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy.h"
#include "EnemyNavigator.h"
#include "AudioManager.h"


#include "GameConstants.h"

static const float ENEMY_JUMP_VELOCITY = std::sqrt(2.0f * GRAVITY * float(ENEMY_JUMP_HEIGHT));
static const float ENEMY_SPRING_JUMP_VELOCITY = ENEMY_JUMP_VELOCITY * std::sqrt(3.0f);

namespace {
bool hasHorizontalLineOfSight(TileMap *map, const glm::ivec2 &fromPos, const glm::ivec2 &toPos)
{
	if (map == nullptr) return false;
	int startX = std::min(fromPos.x, toPos.x);
	int endX = std::max(fromPos.x, toPos.x);
	int y = fromPos.y;
	for (int x = startX; x <= endX; x += 8)
	{
		if (map->getTileTypeAtPos(glm::ivec2(x, y)) == TileType::SOLID)
			return false;
	}
	return true;
}
}


enum EnemyAnims
{
	RUN, CLIMB, JUMP_UP, JUMP_FALL, HURT, DEATH
};


// =====================================================================
//  Enemy implementation
// =====================================================================

Enemy::Enemy()
{
	sprite = NULL;
	stairsSprite = NULL;
	shotSprite = NULL;
	arrowSprite = NULL;
}

Enemy::~Enemy()
{
	if (sprite != NULL)
		delete sprite;
	if (stairsSprite != NULL)
		delete stairsSprite;
	if (shotSprite != NULL)
		delete shotSprite;
	if (arrowSprite != NULL)
		delete arrowSprite;
}

void Enemy::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	resetBaseState(tileMapPos);
	bShooting = false;
	shotCooldown = 0;

	// --- Run/Jump sprite (existing) ---
	spritesheet.loadFromFile("../images/Enemy1_Run_Jump_Hurt_Death.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 frameSizeInTexture(
		float(ENEMY_TEX_FRAME_WIDTH) / texW,
		float(ENEMY_TEX_FRAME_HEIGHT) / texH
	);

	sprite = Sprite::createSprite(glm::ivec2(ENEMY_FRAME_WIDTH, ENEMY_FRAME_HEIGHT), frameSizeInTexture, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(6);

	sprite->setAnimationSpeed(RUN, 10);       sprite->setAnimationLoop(RUN, true);
	sprite->setAnimationSpeed(CLIMB, 8);      sprite->setAnimationLoop(CLIMB, true);
	sprite->setAnimationSpeed(JUMP_UP, 10);   sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationSpeed(JUMP_FALL, 10); sprite->setAnimationLoop(JUMP_FALL, false);
	sprite->setAnimationSpeed(HURT, 10);      sprite->setAnimationLoop(HURT, false);
	sprite->setAnimationSpeed(DEATH, 8);      sprite->setAnimationLoop(DEATH, false);

	for (int f = 0; f < ENEMY_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN, glm::vec2(f * frameSizeInTexture.x, float(E1_ROW_RUN) / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(0.0f, float(E1_ROW_RUN) / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(frameSizeInTexture.x, float(E1_ROW_RUN) / texH));
	for (int f = 0; f < ENEMY_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP, glm::vec2(f * frameSizeInTexture.x, float(E1_ROW_JUMP) / texH));
	for (int f = 0; f < ENEMY_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL, glm::vec2((f + ENEMY_JUMP_FALL_START) * frameSizeInTexture.x, float(E1_ROW_JUMP) / texH));
	for (int f = 0; f < ENEMY_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT, glm::vec2(f * frameSizeInTexture.x, float(E1_ROW_HURT) / texH));
	for (int f = 0; f < ENEMY_DEATH_FRAMES; ++f)
		sprite->addKeyframe(DEATH, glm::vec2(f * frameSizeInTexture.x, float(E1_ROW_DEATH) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);

	stairsSpritesheet.loadFromFile("../images/Enemy1_Stairs.png", TEXTURE_PIXEL_FORMAT_RGBA);
	stairsSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setMinFilter(GL_NEAREST);
	stairsSpritesheet.setMagFilter(GL_NEAREST);
	stairsSprite = Sprite::createSprite(
		glm::ivec2(ENEMY_STAIRS_RENDER_WIDTH, ENEMY_FRAME_HEIGHT),
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

	shotSpritesheet.loadFromFile("images/Enemy1_Shot.png", TEXTURE_PIXEL_FORMAT_RGBA);
	shotSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	shotSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	shotSpritesheet.setMinFilter(GL_NEAREST);
	shotSpritesheet.setMagFilter(GL_NEAREST);

	float shotTexW = float(shotSpritesheet.width());
	float shotTexH = float(shotSpritesheet.height());
	glm::vec2 shotFrameSize(
		float(SHOT_FRAME_WIDTH) / shotTexW,
		float(SHOT_FRAME_HEIGHT) / shotTexH
	);

	shotSprite = Sprite::createSprite(glm::ivec2(ENEMY_FRAME_WIDTH, SHOT_RENDER_HEIGHT), shotFrameSize, &shotSpritesheet, &shaderProgram);
	shotSprite->setNumberAnimations(1);
	shotSprite->setAnimationSpeed(0, 12);
	shotSprite->setAnimationLoop(0, false);
	for (int f = 0; f < SHOT_FRAMES_ROW0; ++f)
		shotSprite->addKeyframe(0, glm::vec2(f * shotFrameSize.x, 0.0f));
	for (int f = 0; f < SHOT_FRAMES_ROW1; ++f)
		shotSprite->addKeyframe(0, glm::vec2(f * shotFrameSize.x, shotFrameSize.y));
	shotSprite->changeAnimation(0);

	arrowTexture.loadFromFile("images/Arrow.png", TEXTURE_PIXEL_FORMAT_RGBA);
	arrowTexture.setWrapS(GL_CLAMP_TO_EDGE);
	arrowTexture.setWrapT(GL_CLAMP_TO_EDGE);
	arrowTexture.setMinFilter(GL_NEAREST);
	arrowTexture.setMagFilter(GL_NEAREST);

	arrowSprite = Sprite::createSprite(glm::ivec2(ARROW_SIZE, ARROW_SIZE), glm::vec2(1.0f, 1.0f), &arrowTexture, &shaderProgram);
	arrowSprite->setNumberAnimations(1);
	arrowSprite->setAnimationSpeed(0, 1);
	arrowSprite->setAnimationLoop(0, true);
	arrowSprite->addKeyframe(0, glm::vec2(0.0f, 0.0f));
	arrowSprite->changeAnimation(0);

}


// =====================================================================
//  BFS pathfinding
// =====================================================================

void Enemy::computePath(const glm::vec2 &playerPos)
{
	EnemyNavParams navParams;
	navParams.hitboxWidth = ENEMY_HITBOX_WIDTH;
	navParams.jumpHeight = ENEMY_JUMP_HEIGHT;
	navParams.horizontalSpeed = ENEMY_SPEED;
	navParams.dashDistanceBasePx = int(ENEMY_DASH_DISTANCE_BASE);
	navParams.jumpAngleStep = JUMP_ANGLE_STEP;
	navParams.maxIterations = 2000;
	navParams.maxJumpAngle = 180;
	navParams.playerCenterOffsetX = 16;
	navParams.playerFeetOffsetY = 63;
	computePathCommon(playerPos, navParams);
}


// =====================================================================
//  Update — follow the BFS path, shoot arrows when player is in range
// =====================================================================

void Enemy::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
	ladderAnimActive = false;
	float dt = float(deltaTime) / 1000.0f;

	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);

	// Death animation — play out then fully remove
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
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
		onGround = map->checkCollision(groundedProbe, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY, dropThrough);
		if (onGround && verticalVelocity >= 0.0f) {
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	{
		glm::ivec2 triggerPos(int(posEnemyF.x), int(posEnemyF.y));
		if (springCooldown == 0 && map->isOnSpring(triggerPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT))) {
			verticalVelocity = -ENEMY_SPRING_JUMP_VELOCITY;
			onGround = false;
			bJumping = true;
			springCooldown = 120;
		}
		bool dashLeft = false;
		if (dashCooldown == 0 && map->isOnDash(triggerPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), &dashLeft)) {
			int dir = dashLeft ? -1 : 1;
			dashTimeLeftMs = ENEMY_DASH_DURATION_MS;
			dashVelocityStart = dir * (ENEMY_DASH_DISTANCE_BASE * 3.0f) / (0.5f * float(ENEMY_DASH_DURATION_MS));
			dashVelocity = dashVelocityStart;
			dashCooldown = 120;
		}
	}

	// Knockback — move enemy back while hurt animation plays
	if (knockbackFrames > 0)
	{
		knockbackFrames--;
		sprite->update(deltaTime);
		posEnemyF.x += knockbackDir * KNOCKBACK_SPEED;
		glm::ivec2 kbPos(int(posEnemyF.x), int(posEnemyF.y));
		if (knockbackDir < 0)
			map->checkCollision(kbPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::LEFT, &kbPos.x);
		else
			map->checkCollision(kbPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::RIGHT, &kbPos.x);
		posEnemyF.x = float(kbPos.x);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		kbPos = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(kbPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &kbPos.y);
		if (onGround) {
			posEnemyF.y = float(kbPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = kbPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		return;
	}

	// Hurt animation finishing — freeze until it completes
	if (sprite->animation() == HURT)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
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

	// Decrease shot cooldown
	if (shotCooldown > 0) shotCooldown--;

	// --- Update arrows (move horizontally, remove on wall/off-screen) ---
	int ts = map->getTileSize();
	glm::ivec2 mapPixels = glm::ivec2(map->getMapSize().x * ts, map->getMapSize().y * ts);
	for (int i = (int)arrows.size() - 1; i >= 0; --i)
	{
		arrows[i].pos.x += arrows[i].goingLeft ? -ARROW_SPEED : ARROW_SPEED;

		// Remove if off-screen
		if (arrows[i].pos.x < -ARROW_SIZE || arrows[i].pos.x > mapPixels.x)
		{
			arrows.erase(arrows.begin() + i);
			continue;
		}

		// Remove if hits a solid tile (check the leading edge of the arrow)
		int checkX = (int)arrows[i].pos.x + (arrows[i].goingLeft ? 0 : ARROW_SIZE);
		int checkY = (int)arrows[i].pos.y + ARROW_SIZE / 2;
		if (map->getTileTypeAtPos(glm::ivec2(checkX, checkY)) == TileType::SOLID)
		{
			arrows.erase(arrows.begin() + i);
			continue;
		}

	}

	// --- Shooting logic ---
	if (bShooting)
	{
		shotSprite->update(deltaTime);
		if (shotSprite->animationFinished())
		{
			// Spawn an arrow at the enemy's position
			Arrow newArrow;
			float arrowY = posEnemy.y + ENEMY_HITBOX_HEIGHT / 4.f - ARROW_SIZE / 2.f;
			if (facingLeft)
				newArrow.pos = glm::vec2(posEnemy.x - ARROW_SIZE, arrowY);
			else
				newArrow.pos = glm::vec2(posEnemy.x + ENEMY_HITBOX_WIDTH, arrowY);
			newArrow.goingLeft = facingLeft;
			newArrow.reflected = false;
			arrows.push_back(newArrow);

			bShooting = false;
			shotCooldown = SHOT_COOLDOWN_FRAMES;
			sprite->changeAnimation(RUN);
		}

		// While shooting: still apply gravity but no horizontal movement
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		} else {
			bJumping = true;
		}
		posEnemy = fallPos;

		// Position shot sprite (feet aligned with hitbox bottom, centered on hitbox)
		float shotRenderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
		float shotRenderOffsetY = float(SHOT_RENDER_HEIGHT - ENEMY_HITBOX_HEIGHT);
		shotSprite->setFlipHorizontal(facingLeft);
		shotSprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - shotRenderOffsetX, float(tileMapDispl.y + posEnemy.y) - shotRenderOffsetY));

		// Also update run/jump sprite position for when we switch back
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		return;
	}

	float dx = playerPos.x - posEnemy.x;
	float dy = playerPos.y - (posEnemy.y - 16.f);
	bool playerInShootBand = (std::abs(dx) < SHOT_DETECT_RANGE && std::abs(dy) < SHOT_DETECT_VERTICAL);
	glm::ivec2 enemyEye(posEnemy.x + ENEMY_HITBOX_WIDTH / 2, posEnemy.y + ENEMY_HITBOX_HEIGHT / 4);
	glm::ivec2 playerAim(int(playerPos.x) + ENEMY_HITBOX_WIDTH / 2, int(playerPos.y) + ENEMY_HITBOX_HEIGHT / 3);
	bool playerVisible = hasHorizontalLineOfSight(map, enemyEye, playerAim);

	// Archers should immediately face the player when in shooting band even if pathing fails.
	if (playerInShootBand)
	{
		facingLeft = dx < 0.0f;
		sprite->setFlipHorizontal(facingLeft);
		shotSprite->setFlipHorizontal(facingLeft);
	}

	// --- Check if we should start shooting ---
	if (shotCooldown <= 0 && onGround && !bJumping && playerInShootBand && playerVisible)
	{
		bShooting = true;
		shotSprite->changeAnimation(0);
		shotSprite->setFlipHorizontal(facingLeft);
		return;
	}

	// If player is in a valid shooting lane, hold position and keep aiming instead of chasing.
	if (!bShooting && playerInShootBand && playerVisible)
	{
		if (sprite->animation() != RUN)
			sprite->changeAnimation(RUN);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 holdPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(holdPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &holdPos.y);
		if (onGround) {
			posEnemyF.y = float(holdPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		} else {
			bJumping = true;
		}
		posEnemy = holdPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		float stairsRenderOffsetX = 0.5f * float(ENEMY_STAIRS_RENDER_WIDTH - ENEMY_HITBOX_WIDTH);
		float stairsRenderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
		stairsSprite->setFlipHorizontal(facingLeft);
		stairsSprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - stairsRenderOffsetX, float(tileMapDispl.y + posEnemy.y) - stairsRenderOffsetY));
		return;
	}

	BaseEnemyMoveConfig moveConfig;
	moveConfig.hitboxWidth = ENEMY_HITBOX_WIDTH;
	moveConfig.hitboxHeight = ENEMY_HITBOX_HEIGHT;
	moveConfig.moveSpeed = ENEMY_SPEED;
	moveConfig.jumpVelocity = ENEMY_JUMP_VELOCITY;
	moveConfig.dashDurationMs = ENEMY_DASH_DURATION_MS;
	moveConfig.runAnim = RUN;
	moveConfig.climbAnim = CLIMB;
	moveConfig.jumpUpAnim = JUMP_UP;
	moveConfig.jumpFallAnim = JUMP_FALL;
	moveConfig.pathRecalcFrames = PATH_RECALC_FRAMES;
	moveConfig.renderOffsetX = renderOffsetX;
	moveConfig.renderOffsetY = renderOffsetY;
	updatePathMovementCommon(deltaTime, dt, playerPos, sprite, moveConfig);

	float stairsRenderOffsetX = 0.5f * float(ENEMY_STAIRS_RENDER_WIDTH - ENEMY_HITBOX_WIDTH);
	float stairsRenderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	stairsSprite->setFlipHorizontal(facingLeft);
	stairsSprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - stairsRenderOffsetX, float(tileMapDispl.y + posEnemy.y) - stairsRenderOffsetY));
	if (ladderAnimActive)
		stairsSprite->update(deltaTime);
}

void Enemy::render()
{
	if (!alive && !bDying) return;

	// Death animation — no arrows, no blink
	if (bDying)
	{
		if (ladderAnimActive)
			stairsSprite->render();
		else
			sprite->render();
		return;
	}

	// Render arrows
	for (const Arrow &a : arrows)
	{
		arrowSprite->setFlipHorizontal(a.goingLeft);
		arrowSprite->setPosition(glm::vec2(float(tileMapDispl.x) + a.pos.x, float(tileMapDispl.y) + a.pos.y));
		arrowSprite->render();
	}

	// Render enemy (shot sprite or run/jump sprite) — blink when hit
	if (hitTimer == 0 || hitTimer <= (HIT_INVINCIBILITY_FRAMES - HIT_BLINK_FRAMES) || (hitTimer / 4) % 2 == 0)
	{
		if (bShooting)
			shotSprite->render();
		else if (ladderAnimActive)
			stairsSprite->render();
		else
			sprite->render();
	}
}

void Enemy::setPosition(const glm::vec2 &pos)
{
	setPositionCommon(pos, ENEMY_FRAME_WIDTH, ENEMY_FRAME_HEIGHT, ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT, sprite);
}

void Enemy::takeDamage(int knockDir)
{
	if (!alive) return;
	health--;
	bShooting = false;
	if (health <= 0)
	{
		AudioManager::instance().playHurt(AudioManager::HurtProfile::EnemyLow);
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
		AudioManager::instance().playHurt(AudioManager::HurtProfile::EnemyLow);
	}
}

bool Enemy::checkArrowHit(const glm::vec2 &pPos, const glm::ivec2 &pSize)
{
	for (int i = (int)arrows.size() - 1; i >= 0; --i)
	{
		const Arrow &a = arrows[i];
		// Arrow hitbox centered on sprite
		float hbX = a.pos.x + (ARROW_SIZE - ARROW_HITBOX_W) / 2.f;
		float hbY = a.pos.y + (ARROW_SIZE - ARROW_HITBOX_H) / 2.f;
		if (hbX < pPos.x + pSize.x &&
			hbX + ARROW_HITBOX_W > pPos.x &&
			hbY < pPos.y + pSize.y &&
			hbY + ARROW_HITBOX_H > pPos.y)
		{
			arrows.erase(arrows.begin() + i);
			return true;
		}
	}
	return false;
}


bool Enemy::reflectArrowHit(const glm::vec2 &pPos, const glm::ivec2 &pSize, bool playerFacingLeft)
{
	bool reflectedAny = false;
	for (int i = (int)arrows.size() - 1; i >= 0; --i)
	{
		if (arrows[i].reflected) continue;
		// Only reflect if player is facing the arrow (arrow comes from the side player is facing)
		if (arrows[i].goingLeft == playerFacingLeft) continue;
		float hbX = arrows[i].pos.x + (ARROW_SIZE - ARROW_HITBOX_W) / 2.f;
		float hbY = arrows[i].pos.y + (ARROW_SIZE - ARROW_HITBOX_H) / 2.f;
		if (hbX < pPos.x + pSize.x && hbX + ARROW_HITBOX_W > pPos.x &&
			hbY < pPos.y + pSize.y && hbY + ARROW_HITBOX_H > pPos.y)
		{
			arrows[i].goingLeft = !arrows[i].goingLeft;
			arrows[i].reflected = true;
			reflectedAny = true;
		}
	}
	return reflectedAny;
}

bool Enemy::checkReflectedArrowHit(const glm::vec4 &hitbox, int &outKnockDir)
{
	for (int i = (int)arrows.size() - 1; i >= 0; --i)
	{
		if (!arrows[i].reflected) continue;
		float hbX = arrows[i].pos.x + (ARROW_SIZE - ARROW_HITBOX_W) / 2.f;
		float hbY = arrows[i].pos.y + (ARROW_SIZE - ARROW_HITBOX_H) / 2.f;
		if (hbX < hitbox.x + hitbox.z && hbX + ARROW_HITBOX_W > hitbox.x &&
			hbY < hitbox.y + hitbox.w && hbY + ARROW_HITBOX_H > hitbox.y)
		{
			outKnockDir = arrows[i].goingLeft ? -1 : 1;
			arrows.erase(arrows.begin() + i);
			return true;
		}
	}
	return false;
}

void Enemy::destroyArrowsInHitbox(const glm::vec4 &attackHitbox)
{
	for (int i = (int)arrows.size() - 1; i >= 0; --i)
	{
		const Arrow &a = arrows[i];
		float hbX = a.pos.x + (ARROW_SIZE - ARROW_HITBOX_W) / 2.f;
		float hbY = a.pos.y + (ARROW_SIZE - ARROW_HITBOX_H) / 2.f;
		
		if (hbX < attackHitbox.x + attackHitbox.z &&
			hbX + ARROW_HITBOX_W > attackHitbox.x &&
			hbY < attackHitbox.y + attackHitbox.w &&
			hbY + ARROW_HITBOX_H > attackHitbox.y)
		{
			arrows.erase(arrows.begin() + i);
		}
	}
}


glm::vec4 Enemy::getHitbox() const
{
	return glm::vec4(posEnemy.x, posEnemy.y, ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT);
}
