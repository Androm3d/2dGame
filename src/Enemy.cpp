#include <cmath>
#include <iostream>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy.h"


#define ENEMY_FRAME_WIDTH        64
#define ENEMY_FRAME_HEIGHT       48
#define ENEMY_HITBOX_WIDTH       32
#define ENEMY_HITBOX_HEIGHT      32
#define ENEMY_RUN_FRAMES          8
#define ENEMY_JUMP_UP_FRAMES      4
#define ENEMY_JUMP_FALL_FRAMES    4
#define ENEMY_JUMP_FALL_START     4
#define ENEMY_SPEED               1
#define ENEMY_FALL_STEP           4
#define ENEMY_JUMP_ANGLE_STEP     4
#define ENEMY_JUMP_HEIGHT        112

#define ROW_RUN_TOP_PX    0
#define ROW_JUMP_TOP_PX  48
#define ROW_HURT_TOP_PX  96
#define ROW_DEATH_TOP_PX 144

#define ENEMY_HURT_FRAMES  3
#define ENEMY_DEATH_FRAMES 5

#define PATH_RECALC_FRAMES 30
#define HIT_INVINCIBILITY_FRAMES 150
#define HIT_BLINK_FRAMES         50
#define KNOCKBACK_FRAMES 8
#define KNOCKBACK_SPEED  5

static const float ENEMY_GRAVITY = 1400.0f;
static const float ENEMY_JUMP_VELOCITY = std::sqrt(2.0f * ENEMY_GRAVITY * float(ENEMY_JUMP_HEIGHT));
static const float ENEMY_SPRING_JUMP_VELOCITY = ENEMY_JUMP_VELOCITY * std::sqrt(3.0f);

// Shot animation constants (Enemy1_Shot.png: 768x232, 96x116 per frame, 13 total: 8 row0 + 5 row1)
#define SHOT_FRAME_WIDTH    96
#define SHOT_FRAME_HEIGHT   116
#define SHOT_RENDER_HEIGHT  64
#define SHOT_FRAMES_ROW0     8
#define SHOT_FRAMES_ROW1     5
#define SHOT_DETECT_RANGE  200
#define SHOT_DETECT_VERTICAL 48
#define SHOT_COOLDOWN_FRAMES 90
#define ARROW_SPEED        4
#define ARROW_SIZE         64
#define ARROW_HITBOX_W     32
#define ARROW_HITBOX_H     12


enum EnemyAnims
{
	RUN, JUMP_UP, JUMP_FALL, HURT, DEATH
};


// =====================================================================
//  BFS helpers — tile-level queries used by the pathfinder
// =====================================================================

// Is tile (tx,ty) a solid wall?
static bool isSolid(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return true;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts)) == TileType::SOLID;
}

// Can the enemy stand at (tx,ty)?  i.e. tile itself is free AND
// tile below is solid ground or one-way platform.
static bool isGround(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return false;
	if (isSolid(tx, ty, m)) return false;          // can't stand inside a wall
	int below = ty + 1;
	if (below >= sz.y) return true;                 // map bottom = ground
	TileType t = m->getTileTypeAtPos(glm::ivec2(tx * ts, below * ts));
	return t == TileType::SOLID || t == TileType::ONE_WAY_PLATFORM;
}

// Simulate falling from (tx, startTY) downward.
// Returns the Y tile where the enemy would land, or -1 if it falls off the map.
static int landingY(int tx, int startTY, const TileMap *m)
{
	glm::ivec2 sz = m->getMapSize();
	for (int y = startTY; y < sz.y; y++)
	{
		if (isSolid(tx, y, m)) return -1;   // blocked by a wall mid-fall
		if (isGround(tx, y, m)) return y;
	}
	return -1;
}


// =====================================================================
//  Enemy implementation
// =====================================================================

Enemy::Enemy()
{
	sprite = NULL;
	shotSprite = NULL;
	arrowSprite = NULL;
	map = NULL;
}

Enemy::~Enemy()
{
	if (sprite != NULL)
		delete sprite;
	if (shotSprite != NULL)
		delete shotSprite;
	if (arrowSprite != NULL)
		delete arrowSprite;
}

void Enemy::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	facingLeft = false;
	bJumping = false;
	onGround = false;
	bShooting = false;
	alive = true;
	bDying = false;
	health = 2;
	jumpAngle = 0;
	startY = 0;
	pathRecalcTimer = 0;
	pathIndex = 0;
	shotCooldown = 0;
	springCooldown = 0;
	dashCooldown = 0;
	hitTimer = 0;
	knockbackFrames = 0;
	knockbackDir = 0;
	posEnemyF = glm::vec2(0.0f, 0.0f);
	verticalVelocity = 0.0f;

	// --- Run/Jump sprite (existing) ---
	spritesheet.loadFromFile("../images/Enemy1_Run_Jump_Hurt_Death.png", TEXTURE_PIXEL_FORMAT_RGBA);
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
	sprite->setNumberAnimations(5);

	sprite->setAnimationSpeed(RUN, 10);       sprite->setAnimationLoop(RUN, true);
	sprite->setAnimationSpeed(JUMP_UP, 10);   sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationSpeed(JUMP_FALL, 10); sprite->setAnimationLoop(JUMP_FALL, false);
	sprite->setAnimationSpeed(HURT, 10);      sprite->setAnimationLoop(HURT, false);
	sprite->setAnimationSpeed(DEATH, 8);      sprite->setAnimationLoop(DEATH, false);

	// Row 0: Run (8 frames)
	for (int f = 0; f < ENEMY_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN, glm::vec2(f * frameSizeInTexture.x, float(ROW_RUN_TOP_PX) / texH));
	// Row 1: Jump up (frames 0-3)
	for (int f = 0; f < ENEMY_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP, glm::vec2(f * frameSizeInTexture.x, float(ROW_JUMP_TOP_PX) / texH));
	// Row 1: Jump fall (frames 4-7)
	for (int f = 0; f < ENEMY_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL, glm::vec2((f + ENEMY_JUMP_FALL_START) * frameSizeInTexture.x, float(ROW_JUMP_TOP_PX) / texH));
	// Row 2: Hurt (3 frames)
	for (int f = 0; f < ENEMY_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT, glm::vec2(f * frameSizeInTexture.x, float(ROW_HURT_TOP_PX) / texH));
	// Row 3: Death (5 frames)
	for (int f = 0; f < ENEMY_DEATH_FRAMES; ++f)
		sprite->addKeyframe(DEATH, glm::vec2(f * frameSizeInTexture.x, float(ROW_DEATH_TOP_PX) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);

	// --- Shot sprite (Enemy1_Shot.png: 768x232, 12 frames at 64x116) ---
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
	// Row 0: 8 frames
	for (int f = 0; f < SHOT_FRAMES_ROW0; ++f)
		shotSprite->addKeyframe(0, glm::vec2(f * shotFrameSize.x, 0.0f));
	// Row 1: 5 remaining frames
	for (int f = 0; f < SHOT_FRAMES_ROW1; ++f)
		shotSprite->addKeyframe(0, glm::vec2(f * shotFrameSize.x, shotFrameSize.y));
	shotSprite->changeAnimation(0);

	// --- Arrow sprite (Arrow.png: 64x64, single frame) ---
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

	tileMapDispl = tileMapPos;
}


// =====================================================================
//  BFS pathfinding
// =====================================================================

void Enemy::computePath(const glm::vec2 &playerPos)
{
	path.clear();
	pathIndex = 0;
	if (!map) return;

	int ts = map->getTileSize();
	glm::ivec2 ms = map->getMapSize();
	int W = ms.x;

	// Convert pixel positions to tile coords
	int ex = (posEnemy.x + ENEMY_HITBOX_WIDTH / 2) / ts;
	int ey = posEnemy.y / ts;
	int px = ((int)playerPos.x + 16) / ts;      // player center (assuming ~32 hitbox)
	int py = ((int)playerPos.y + 63) / ts;       // player feet tile (64px hitbox)

	// Clamp
	ex = std::max(0, std::min(ex, W - 1));
	ey = std::max(0, std::min(ey, ms.y - 1));
	px = std::max(0, std::min(px, W - 1));
	py = std::max(0, std::min(py, ms.y - 1));

	// Snap to nearest standing tile if mid-air
	if (!isGround(ex, ey, map))
	{
		int ly = landingY(ex, ey, map);
		if (ly >= 0) ey = ly;
	}
	if (!isGround(px, py, map))
	{
		// Try adjacent rows
		if (py + 1 < ms.y && isGround(px, py + 1, map)) py++;
		else if (py - 1 >= 0 && isGround(px, py - 1, map)) py--;
		else
		{
			int ly = landingY(px, py, map);
			if (ly >= 0) py = ly;
		}
	}

	glm::ivec2 start(ex, ey);
	glm::ivec2 goal(px, py);
	if (start == goal) return;

	// BFS
	auto key = [W](const glm::ivec2 &v) { return v.y * W + v.x; };

	std::queue<glm::ivec2> q;
	std::map<int, glm::ivec2> parent;

	q.push(start);
	parent[key(start)] = glm::ivec2(-1, -1);

	bool found = false;
	int iterations = 0;

	while (!q.empty() && !found && iterations < 2000)
	{
		++iterations;
		glm::ivec2 c = q.front();
		q.pop();

		std::vector<glm::ivec2> nbrs;

		// ---- Walk / Fall left and right ----
		for (int dx = -1; dx <= 1; dx += 2)
		{
			int nx = c.x + dx;
			if (nx < 0 || nx >= W) continue;
			if (isSolid(nx, c.y, map)) continue;

			if (isGround(nx, c.y, map))
				nbrs.push_back(glm::ivec2(nx, c.y));
			else
			{
				int ly = landingY(nx, c.y, map);
				if (ly >= 0) nbrs.push_back(glm::ivec2(nx, ly));
			}
		}

		// ---- Jump (left, straight up, right) ----
		for (int dir = -1; dir <= 1; dir++)
		{
			float jpx = c.x * ts + ts / 2.f;
			float jpy = (float)(c.y * ts);
			float jStartY = jpy;
			bool landed = false;

			for (int ang = ENEMY_JUMP_ANGLE_STEP; ang <= 180; ang += ENEMY_JUMP_ANGLE_STEP)
			{
				jpy = jStartY - ENEMY_JUMP_HEIGHT * sin(3.14159f * ang / 180.f);
				jpx += dir * ENEMY_SPEED;

				int ttx = (int)jpx / ts;
				int tty = (int)jpy / ts;

				// Out of map or hit wall → abort this jump direction
				if (ttx < 0 || ttx >= W || tty < 0 || tty >= ms.y) break;
				if (isSolid(ttx, tty, map)) break;

				// During descent (past the peak), check for landing
				if (ang >= 90 && isGround(ttx, tty, map))
				{
					nbrs.push_back(glm::ivec2(ttx, tty));
					landed = true;
					break;
				}
			}

			// If the arc ended in mid-air, simulate the remaining fall
			if (!landed && dir != 0)
			{
				int endTX = (int)jpx / ts;
				int endTY = (int)jpy / ts;
				if (endTX >= 0 && endTX < W && endTY >= 0 && endTY < ms.y
					&& !isSolid(endTX, endTY, map))
				{
					int ly = landingY(endTX, endTY, map);
					if (ly >= 0) nbrs.push_back(glm::ivec2(endTX, ly));
				}
			}
		}

		// Enqueue unvisited neighbours
		for (const auto &n : nbrs)
		{
			int k = key(n);
			if (parent.find(k) == parent.end())
			{
				parent[k] = c;
				if (n == goal) { found = true; break; }
				q.push(n);
			}
		}
	}

	// Reconstruct path (goal → start, then reverse)
	if (found)
	{
		glm::ivec2 c = goal;
		while (c != start && c.x != -1)
		{
			path.push_back(c);
			c = parent[key(c)];
		}
		std::reverse(path.begin(), path.end());
	}
}


// =====================================================================
//  Update — follow the BFS path, shoot arrows when player is in range
// =====================================================================

void Enemy::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
	float dt = float(deltaTime) / 1000.0f;

	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);

	// Death animation — play out then fully remove
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += ENEMY_GRAVITY * dt;
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

	{
		glm::ivec2 groundedProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		onGround = map->checkCollision(groundedProbe, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY);
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
			posEnemyF.x += float(dir * 90);
			glm::ivec2 dashPos(int(posEnemyF.x), int(posEnemyF.y));
			map->checkCollision(dashPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), dir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x);
			posEnemyF.x = float(dashPos.x);
			posEnemy = dashPos;
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
		verticalVelocity += ENEMY_GRAVITY * dt;
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
		verticalVelocity += ENEMY_GRAVITY * dt;
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
		int tileX = checkX / ts;
		int tileY = checkY / ts;
		if (isSolid(tileX, tileY, map))
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
			float arrowY = posEnemy.y + ENEMY_HITBOX_HEIGHT / 2.f - ARROW_SIZE / 2.f;
			if (facingLeft)
				newArrow.pos = glm::vec2(posEnemy.x - ARROW_SIZE, arrowY);
			else
				newArrow.pos = glm::vec2(posEnemy.x + ENEMY_HITBOX_WIDTH, arrowY);
			newArrow.goingLeft = facingLeft;
			arrows.push_back(newArrow);

			bShooting = false;
			shotCooldown = SHOT_COOLDOWN_FRAMES;
			sprite->changeAnimation(RUN);
		}

		// While shooting: still apply gravity but no horizontal movement
		verticalVelocity += ENEMY_GRAVITY * dt;
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

	// --- Check if we should start shooting ---
	if (shotCooldown <= 0 && onGround && !bJumping)
	{
		float dx = playerPos.x - posEnemy.x;
		float dy = playerPos.y - (posEnemy.y - 16.f); // approximate vertical alignment
		bool playerInFront = (facingLeft && dx < 0) || (!facingLeft && dx > 0);

		if (playerInFront && abs(dx) < SHOT_DETECT_RANGE && abs(dy) < SHOT_DETECT_VERTICAL)
		{
			bShooting = true;
			shotSprite->changeAnimation(0);
			shotSprite->setFlipHorizontal(facingLeft);
			return;
		}
	}

	// --- Normal movement (BFS pathfinding) ---
	sprite->update(deltaTime);

	// Recompute path periodically
	if (--pathRecalcTimer <= 0)
	{
		computePath(playerPos);
		pathRecalcTimer = PATH_RECALC_FRAMES;
	}

	// --- Determine desired movement from path ---
	int myTX = (posEnemy.x + ENEMY_HITBOX_WIDTH / 2) / ts;
	int myTY = posEnemy.y / ts;

	bool wantLeft = false, wantRight = false, wantJump = false;

	// Skip all waypoints we've already reached
	while (pathIndex < (int)path.size())
	{
		glm::ivec2 wp = path[pathIndex];
		int wpPX = wp.x * ts;
		if (abs(posEnemy.x - wpPX) < ts / 2 && abs(myTY - wp.y) <= 1)
			pathIndex++;
		else
			break;
	}

	if (pathIndex < (int)path.size())
	{
		glm::ivec2 target = path[pathIndex];

		// Use pixel-level targeting to avoid freezing at tile boundaries
		int targetCenterPX = target.x * ts + ts / 2;
		int myCenterPX = posEnemy.x + ENEMY_HITBOX_WIDTH / 2;

		if (myCenterPX < targetCenterPX - 4)      wantRight = true;
		else if (myCenterPX > targetCenterPX + 4)  wantLeft = true;

		// Jump if next waypoint is above us
		if (target.y < myTY) wantJump = true;
	}

	// --- Horizontal movement ---
	bool blocked = false;
	if (wantLeft)
	{
		facingLeft = true;
		sprite->setFlipHorizontal(true);
		posEnemyF.x -= ENEMY_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::LEFT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}
	else if (wantRight)
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		posEnemyF.x += ENEMY_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::RIGHT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}

	// --- Jump trigger ---
	if ((wantJump || blocked) && !bJumping && onGround)
	{
		bJumping = true;
		onGround = false;
		verticalVelocity = -ENEMY_JUMP_VELOCITY;
	}

	{
		glm::ivec2 groundProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		if (map->checkCollision(groundProbe, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY) && verticalVelocity >= 0.0f)
		{
			onGround = true;
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	// --- Animation ---
	if (onGround)
	{
		if (sprite->animation() != RUN)
			sprite->changeAnimation(RUN);
	}
	else
	{
		if (verticalVelocity < 0.0f && sprite->animation() != JUMP_UP)
			sprite->changeAnimation(JUMP_UP);
		else if (verticalVelocity >= 0.0f && sprite->animation() != JUMP_FALL)
			sprite->changeAnimation(JUMP_FALL);
	}

	// --- Jump / gravity physics ---
	verticalVelocity += ENEMY_GRAVITY * dt;
	posEnemyF.y += verticalVelocity * dt;
	glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
	if (verticalVelocity > 0.0f)
	{
		onGround = map->checkCollision(fallPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
	}
	else if (verticalVelocity < 0.0f)
	{
		if (map->checkCollision(fallPos, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::UP, &fallPos.y))
		{
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
		}
		bJumping = true;
	}
	posEnemy = fallPos;

	// --- Sprite position ---
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}

void Enemy::render()
{
	if (!alive && !bDying) return;

	// Death animation — no arrows, no blink
	if (bDying)
	{
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
		else
			sprite->render();
	}
}

void Enemy::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Enemy::setPosition(const glm::vec2 &pos)
{
	posEnemyF = pos;
	posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}

void Enemy::setActive(bool value)
{
	alive = value;
	if (!value)
		bDying = false;
}

void Enemy::takeDamage(int knockDir)
{
	if (!alive) return;
	health--;
	bShooting = false;
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
