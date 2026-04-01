#include <cmath>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy3.h"
#include "EnemyNavigator.h"
#include "AudioManager.h"


#include "GameConstants.h"

static const float E3_JUMP_VELOCITY = std::sqrt(2.0f * GRAVITY * float(E3_JUMP_HEIGHT));
static const float E3_SPRING_JUMP_VELOCITY = E3_JUMP_VELOCITY * std::sqrt(3.0f);


enum Enemy3Anims
{
	RUN, CLIMB, JUMP_UP, JUMP_FALL, HURT, DEAD, ATTACK, ATTACK2
};


// =====================================================================
//  Enemy3 implementation
// =====================================================================

Enemy3::Enemy3()
{
	sprite    = NULL;
	stairsSprite = NULL;
	fireSprite = NULL;
}

Enemy3::~Enemy3()
{
	if (sprite)     delete sprite;
	if (stairsSprite) delete stairsSprite;
	if (fireSprite) delete fireSprite;
}

void Enemy3::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	resetBaseState(tileMapPos);
	bCasting        = false;
	bMelee          = false;
	attackCooldown  = 0;

	// --- Main spritesheet ---
	spritesheet.loadFromFile("images/Enemy3.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 fs(float(E3_FRAME_WIDTH) / texW, float(E3_FRAME_HEIGHT) / texH);

	sprite = Sprite::createSprite(glm::ivec2(E3_RENDER_WIDTH, E3_RENDER_HEIGHT), fs, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(8);

	sprite->setAnimationSpeed(RUN,     10); sprite->setAnimationLoop(RUN,     true);
	sprite->setAnimationSpeed(CLIMB,    8); sprite->setAnimationLoop(CLIMB,   true);
	sprite->setAnimationSpeed(JUMP_UP, 10); sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationSpeed(JUMP_FALL,10);sprite->setAnimationLoop(JUMP_FALL,false);
	sprite->setAnimationSpeed(HURT,    10); sprite->setAnimationLoop(HURT,    false);
	sprite->setAnimationSpeed(DEAD,     8); sprite->setAnimationLoop(DEAD,    false);
	sprite->setAnimationSpeed(ATTACK,  10); sprite->setAnimationLoop(ATTACK,  false);
	sprite->setAnimationSpeed(ATTACK2, 12); sprite->setAnimationLoop(ATTACK2, false);

	for (int f = 0; f < E3_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN,      glm::vec2(f * fs.x, float(E3_ROW_RUN)    / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(0.0f, float(E3_ROW_RUN) / texH));
	sprite->addKeyframe(CLIMB, glm::vec2(fs.x, float(E3_ROW_RUN) / texH));
	for (int f = 0; f < E3_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP,  glm::vec2(f * fs.x, float(E3_ROW_JUMP)   / texH));
	for (int f = 0; f < E3_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL,glm::vec2((f + E3_JUMP_UP_FRAMES) * fs.x, float(E3_ROW_JUMP) / texH));
	for (int f = 0; f < E3_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT,     glm::vec2(f * fs.x, float(E3_ROW_HURT)   / texH));
	for (int f = 0; f < E3_DEAD_FRAMES; ++f)
		sprite->addKeyframe(DEAD,     glm::vec2(f * fs.x, float(E3_ROW_DEAD)   / texH));
	for (int f = 0; f < E3_ATTACK_FRAMES; ++f)
		sprite->addKeyframe(ATTACK,   glm::vec2(f * fs.x, float(E3_ROW_ATTACK) / texH));
	for (int f = 0; f < E3_ATTACK2_FRAMES; ++f)
		sprite->addKeyframe(ATTACK2,  glm::vec2(f * fs.x, float(E3_ROW_ATK2)   / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);

	stairsSpritesheet.loadFromFile("images/Enemy3_Stairs.png", TEXTURE_PIXEL_FORMAT_RGBA);
	stairsSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	stairsSpritesheet.setMinFilter(GL_NEAREST);
	stairsSpritesheet.setMagFilter(GL_NEAREST);
	stairsSprite = Sprite::createSprite(
		glm::ivec2(E3_STAIRS_RENDER_WIDTH, E3_RENDER_HEIGHT),
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

	// --- Fire projectile spritesheet (Enemy3_Fire.png: 352x48, 32x48, 11 frames) ---
	fireSpritesheet.loadFromFile("images/Enemy3_Fire.png", TEXTURE_PIXEL_FORMAT_RGBA);
	fireSpritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	fireSpritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	fireSpritesheet.setMinFilter(GL_NEAREST);
	fireSpritesheet.setMagFilter(GL_NEAREST);

	float fTexW = float(fireSpritesheet.width());
	float fTexH = float(fireSpritesheet.height());
	glm::vec2 ffs(float(FIRE_FRAME_WIDTH) / fTexW, float(FIRE_FRAME_HEIGHT) / fTexH);

	fireSprite = Sprite::createSprite(glm::ivec2(FIRE_RENDER_WIDTH, FIRE_RENDER_HEIGHT), ffs, &fireSpritesheet, &shaderProgram);
	fireSprite->setNumberAnimations(1);
	fireSprite->setAnimationSpeed(0, FIRE_ANIM_FPS);
	fireSprite->setAnimationLoop(0, false);
	for (int f = 0; f < FIRE_FRAMES; ++f)
		fireSprite->addKeyframe(0, glm::vec2(f * ffs.x, 0.0f));
	fireSprite->changeAnimation(0);

}


// =====================================================================
//  BFS pathfinding
// =====================================================================

void Enemy3::computePath(const glm::vec2 &playerPos)
{
	EnemyNavParams navParams;
	navParams.hitboxWidth = E3_HITBOX_WIDTH;
	navParams.jumpHeight = E3_JUMP_HEIGHT;
	navParams.horizontalSpeed = E3_SPEED;
	navParams.dashDistanceBasePx = int(E3_DASH_DISTANCE_BASE);
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

void Enemy3::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
	ladderAnimActive = false;
	float dt = float(deltaTime) / 1000.0f;

	float renderOffsetX = 0.5f * float(E3_RENDER_WIDTH  - E3_HITBOX_WIDTH);
	float renderOffsetY =        float(E3_RENDER_HEIGHT - E3_HITBOX_HEIGHT);

	// --- Update fireballs ---
	fireSprite->update(deltaTime);   // advance shared animation once per frame

	int ts = map->getTileSize();
	glm::ivec2 mapPx = glm::ivec2(map->getMapSize().x * ts, map->getMapSize().y * ts);
	for (int i = (int)fireballs.size() - 1; i >= 0; --i)
	{
		FireBall &fb = fireballs[i];
		fb.pos.x += fb.goingLeft ? -FIRE_SPEED : FIRE_SPEED;

		// Remove if off-screen
		if (fb.pos.x < -FIRE_RENDER_WIDTH || fb.pos.x > mapPx.x) { fireballs.erase(fireballs.begin() + i); continue; }

		// Remove if hits solid tile
		int checkX = (int)fb.pos.x + (fb.goingLeft ? 0 : FIRE_RENDER_WIDTH);
		int checkY = (int)fb.pos.y + FIRE_RENDER_HEIGHT / 2;
		if (map->getTileTypeAtPos(glm::ivec2(checkX, checkY)) == TileType::SOLID) { fireballs.erase(fireballs.begin() + i); continue; }

	}

	// --- Death animation ---
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished()) bDying = false;
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
		onGround = map->checkCollision(groundedProbe, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY, dropThrough);
		if (onGround && verticalVelocity >= 0.0f) {
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	{
		glm::ivec2 triggerPos(int(posEnemyF.x), int(posEnemyF.y));
		if (springCooldown == 0 && map->isOnSpring(triggerPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT))) {
			verticalVelocity = -E3_SPRING_JUMP_VELOCITY;
			onGround = false;
			bJumping = true;
			springCooldown = 120;
		}
		bool dashLeft = false;
		if (dashCooldown == 0 && map->isOnDash(triggerPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), &dashLeft)) {
			int dir = dashLeft ? -1 : 1;
			dashTimeLeftMs = E3_DASH_DURATION_MS;
			dashVelocityStart = dir * (E3_DASH_DISTANCE_BASE * 3.0f) / (0.5f * float(E3_DASH_DURATION_MS));
			dashVelocity = dashVelocityStart;
			dashCooldown = 120;
		}
	}

	// --- Knockback + hurt animation ---
	if (knockbackFrames > 0)
	{
		knockbackFrames--;
		sprite->update(deltaTime);
		posEnemyF.x += knockbackDir * KNOCKBACK_SPEED;
		glm::ivec2 kbPos(int(posEnemyF.x), int(posEnemyF.y));
		map->checkCollision(kbPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT),
			knockbackDir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &kbPos.x);
		posEnemyF.x = float(kbPos.x);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		kbPos = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(kbPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &kbPos.y);
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
		onGround = map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished()) sprite->changeAnimation(RUN);
		return;
	}

	if (attackCooldown > 0) attackCooldown--;

	// Keep facing the player for combat readability (shooting/melee), but still allow path chase.
	float faceDx = playerPos.x - posEnemy.x;
	if (std::abs(faceDx) > 2.0f)
	{
		facingLeft = (faceDx < 0.0f);
		sprite->setFlipHorizontal(facingLeft);
	}

	// --- Fire cast: freeze until animation finishes, then spawn fireball ---
	if (bCasting)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished())
		{
			// Spawn fireball
			fireSprite->changeAnimation(0);  // reset to frame 0 for each new fireball
			FireBall fb;
			fb.goingLeft = facingLeft;
			fb.reflected = false;
			float fbY     = posEnemy.y + E3_HITBOX_HEIGHT / 2.f - FIRE_RENDER_HEIGHT / 2.f - 12.f;
			fb.pos        = facingLeft
				? glm::vec2(posEnemy.x - FIRE_RENDER_WIDTH, fbY)
				: glm::vec2(posEnemy.x + E3_HITBOX_WIDTH,   fbY);
			fireballs.push_back(fb);

			bCasting       = false;
			attackCooldown = E3_ATTACK_COOLDOWN;
			sprite->changeAnimation(RUN);
		}
		return;
	}

	// --- Melee (Attack2): freeze until animation finishes ---
	if (bMelee)
	{
		sprite->update(deltaTime);
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		onGround = map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
		posEnemy = fallPos;
		sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
		if (sprite->animationFinished())
		{
			bMelee         = false;
			attackCooldown = E3_ATTACK_COOLDOWN;
			sprite->changeAnimation(RUN);
		}
		return;
	}

	// --- Attack decision ---
	if (attackCooldown <= 0 && onGround && !bJumping)
	{
		float dx = playerPos.x - posEnemy.x;
		float dy = playerPos.y - posEnemy.y;
		bool inFront = (facingLeft && dx < 0) || (!facingLeft && dx > 0);

		// Melee (Attack2) takes priority if player is very close
		if (inFront && std::abs(dx) < E3_MELEE_DETECT_RANGE && std::abs(dy) < E3_MELEE_DETECT_VERT)
		{
			bMelee = true;
			sprite->changeAnimation(ATTACK2);
			return;
		}
		// Fire cast at medium range
		if (inFront && std::abs(dx) < FIRE_DETECT_RANGE && std::abs(dy) < FIRE_DETECT_VERT)
		{
			bCasting = true;
			sprite->changeAnimation(ATTACK);
			return;
		}
	}

	BaseEnemyMoveConfig moveConfig;
	moveConfig.hitboxWidth = E3_HITBOX_WIDTH;
	moveConfig.hitboxHeight = E3_HITBOX_HEIGHT;
	moveConfig.moveSpeed = E3_SPEED;
	moveConfig.jumpVelocity = E3_JUMP_VELOCITY;
	moveConfig.dashDurationMs = E3_DASH_DURATION_MS;
	moveConfig.runAnim = RUN;
	moveConfig.climbAnim = CLIMB;
	moveConfig.jumpUpAnim = JUMP_UP;
	moveConfig.jumpFallAnim = JUMP_FALL;
	moveConfig.pathRecalcFrames = PATH_RECALC_FRAMES;
	moveConfig.renderOffsetX = renderOffsetX;
	moveConfig.renderOffsetY = renderOffsetY;
	updatePathMovementCommon(deltaTime, dt, playerPos, sprite, moveConfig);

	float stairsRenderOffsetX = 0.5f * float(E3_STAIRS_RENDER_WIDTH - E3_HITBOX_WIDTH);
	float stairsRenderOffsetY = float(E3_RENDER_HEIGHT - E3_HITBOX_HEIGHT);
	stairsSprite->setFlipHorizontal(facingLeft);
	stairsSprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - stairsRenderOffsetX, float(tileMapDispl.y + posEnemy.y) - stairsRenderOffsetY));
	if (ladderAnimActive)
		stairsSprite->update(deltaTime);
}


void Enemy3::render()
{
	if (!alive && !bDying) return;

	// Render fireballs — all share the same animated frame (fireSprite updated in update())
	for (const FireBall &fb : fireballs)
	{
		fireSprite->setFlipHorizontal(fb.goingLeft);  // sprite faces right by default; flip when going left
		fireSprite->setPosition(glm::vec2(float(tileMapDispl.x) + fb.pos.x, float(tileMapDispl.y) + fb.pos.y));
		fireSprite->render();
	}

	// Blink on hit
	if (!bDying && hitTimer > (HIT_INVINCIBILITY_FRAMES - HIT_BLINK_FRAMES) && (hitTimer / 4) % 2 != 0) return;

	if (ladderAnimActive)
		stairsSprite->render();
	else
		sprite->render();
}


void Enemy3::setPosition(const glm::vec2 &pos)
{
	setPositionCommon(pos, E3_RENDER_WIDTH, E3_RENDER_HEIGHT, E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT, sprite);
}

void Enemy3::takeDamage(int knockDir)
{
	if (!alive) return;
	health--;
	bCasting = false;
	bMelee   = false;
	if (health <= 0)
	{
		AudioManager::instance().playHurt(AudioManager::HurtProfile::EnemyHigh);
		alive           = false;
		bDying          = true;
		knockbackFrames = 0;
		sprite->changeAnimation(DEAD);
	}
	else
	{
		hitTimer        = HIT_INVINCIBILITY_FRAMES;
		knockbackFrames = KNOCKBACK_FRAMES;
		knockbackDir    = knockDir;
		sprite->changeAnimation(HURT);
		AudioManager::instance().playHurt(AudioManager::HurtProfile::EnemyHigh);
	}
}

glm::vec4 Enemy3::getHitbox() const
{
	return glm::vec4(posEnemy.x, posEnemy.y, E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT);
}

bool Enemy3::checkFireballHit(const glm::vec2 &pPos, const glm::ivec2 &pSize)
{
	for (int i = (int)fireballs.size() - 1; i >= 0; --i)
	{
		const FireBall &fb = fireballs[i];
		if (fb.pos.x < pPos.x + pSize.x &&
			fb.pos.x + FIRE_RENDER_WIDTH  > pPos.x &&
			fb.pos.y < pPos.y + pSize.y &&
			fb.pos.y + FIRE_RENDER_HEIGHT > pPos.y)
		{
			fireballs.erase(fireballs.begin() + i);
			return true;
		}
	}
	return false;
}

bool Enemy3::reflectFireballHit(const glm::vec2 &pPos, const glm::ivec2 &pSize, bool playerFacingLeft)
{
	bool reflectedAny = false;
	for (int i = (int)fireballs.size() - 1; i >= 0; --i)
	{
		if (fireballs[i].reflected) continue;
		FireBall &fb = fireballs[i];
		// Only reflect if player is facing the fireball
		if (fb.goingLeft == playerFacingLeft) continue;
		if (fb.pos.x < pPos.x + pSize.x && fb.pos.x + FIRE_RENDER_WIDTH  > pPos.x &&
			fb.pos.y < pPos.y + pSize.y && fb.pos.y + FIRE_RENDER_HEIGHT > pPos.y)
		{
			fb.goingLeft = !fb.goingLeft;
			fb.reflected = true;
			reflectedAny = true;
		}
	}
	return reflectedAny;
}

bool Enemy3::checkReflectedFireballHit(const glm::vec4 &hitbox, int &outKnockDir)
{
	for (int i = (int)fireballs.size() - 1; i >= 0; --i)
	{
		if (!fireballs[i].reflected) continue;
		const FireBall &fb = fireballs[i];
		if (fb.pos.x < hitbox.x + hitbox.z && fb.pos.x + FIRE_RENDER_WIDTH  > hitbox.x &&
			fb.pos.y < hitbox.y + hitbox.w && fb.pos.y + FIRE_RENDER_HEIGHT > hitbox.y)
		{
			outKnockDir = fb.goingLeft ? -1 : 1;
			fireballs.erase(fireballs.begin() + i);
			return true;
		}
	}
	return false;
}

glm::vec4 Enemy3::getMeleeHitbox() const
{
	if (facingLeft)
		return glm::vec4(posEnemy.x - E3_MELEE_DETECT_RANGE, posEnemy.y, E3_MELEE_DETECT_RANGE, E3_HITBOX_HEIGHT);
	else
		return glm::vec4(posEnemy.x + E3_HITBOX_WIDTH, posEnemy.y, E3_MELEE_DETECT_RANGE, E3_HITBOX_HEIGHT);
}
