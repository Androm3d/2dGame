#include <cmath>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy2.h"


// Enemy2.png: 768x560, frame 96x112, one animation per row
// Row 0 (y=  0): Run    — 8 frames
// Row 1 (y=112): Jump   — 7 frames (0-3 up, 4-6 fall)
// Row 2 (y=224): Attack — 4 frames
// Row 3 (y=336): Hurt   — 2 frames
// Row 4 (y=448): Death  — 6 frames

#define E2_FRAME_WIDTH     96   // source frame in texture
#define E2_FRAME_HEIGHT   112
#define E2_RENDER_WIDTH    55   // (96*64/112)
#define E2_RENDER_HEIGHT   64
#define E2_HITBOX_WIDTH    32
#define E2_HITBOX_HEIGHT   32
#define E2_SPEED            1
#define E2_FALL_STEP        4
#define E2_JUMP_ANGLE_STEP  4
#define E2_JUMP_HEIGHT    112

#define E2_RUN_FRAMES      8
#define E2_JUMP_UP_FRAMES  4
#define E2_JUMP_FALL_FRAMES 3
#define E2_ATTACK_FRAMES   4
#define E2_HURT_FRAMES     2
#define E2_DEATH_FRAMES    6

#define ROW_RUN_PX      0
#define ROW_JUMP_PX   112
#define ROW_ATTACK_PX 224
#define ROW_HURT_PX   336
#define ROW_DEATH_PX  448

#define PATH_RECALC_FRAMES       30
#define HIT_INVINCIBILITY_FRAMES 150
#define HIT_BLINK_FRAMES         50
#define KNOCKBACK_FRAMES          8
#define KNOCKBACK_SPEED           5

static const float E2_GRAVITY = 1400.0f;
static const float E2_JUMP_VELOCITY = std::sqrt(2.0f * E2_GRAVITY * float(E2_JUMP_HEIGHT));
static const float E2_SPRING_JUMP_VELOCITY = E2_JUMP_VELOCITY * std::sqrt(3.0f);

// Melee attack: close range, sword enemy
#define MELEE_DETECT_RANGE    36   // px horizontal (≈1 tile, sword requires contact)
#define MELEE_DETECT_VERTICAL 48   // px vertical tolerance
#define MELEE_HITBOX_REACH    48   // how far the sword extends in front
#define ATTACK_COOLDOWN_FRAMES 90


enum Enemy2Anims
{
	RUN, JUMP_UP, JUMP_FALL, ATTACK, HURT, DEATH
};


// =====================================================================
//  BFS helpers (identical logic to Enemy1)
// =====================================================================

static bool e2IsSolid(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return true;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts)) == TileType::SOLID;
}

static bool e2IsGround(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return false;
	if (e2IsSolid(tx, ty, m)) return false;
	int below = ty + 1;
	if (below >= sz.y) return true;
	TileType t = m->getTileTypeAtPos(glm::ivec2(tx * ts, below * ts));
	return t == TileType::SOLID || t == TileType::ONE_WAY_PLATFORM;
}

static int e2LandingY(int tx, int startTY, const TileMap *m)
{
	glm::ivec2 sz = m->getMapSize();
	for (int y = startTY; y < sz.y; y++)
	{
		if (e2IsSolid(tx, y, m)) return -1;
		if (e2IsGround(tx, y, m)) return y;
	}
	return -1;
}


// =====================================================================
//  Enemy2 implementation
// =====================================================================

Enemy2::Enemy2()
{
	sprite = NULL;
	map = NULL;
	posEnemyF = glm::vec2(0.0f, 0.0f);
	verticalVelocity = 0.0f;
}

Enemy2::~Enemy2()
{
	if (sprite != NULL)
		delete sprite;
}

void Enemy2::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	facingLeft    = false;
	bJumping      = false;
	onGround      = false;
	bAttacking    = false;
	alive         = true;
	bDying        = false;
	health        = 2;
	jumpAngle     = 0;
	startY        = 0;
	pathRecalcTimer = 0;
	pathIndex     = 0;
	attackCooldown = 0;
	springCooldown  = 0;
	dashCooldown    = 0;
	hitTimer      = 0;
	knockbackFrames = 0;
	knockbackDir  = 0;

	spritesheet.loadFromFile("../images/Enemy2.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 fs(float(E2_FRAME_WIDTH) / texW, float(E2_FRAME_HEIGHT) / texH);

	sprite = Sprite::createSprite(glm::ivec2(E2_RENDER_WIDTH, E2_RENDER_HEIGHT), fs, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(6);

	sprite->setAnimationSpeed(RUN,       10); sprite->setAnimationLoop(RUN,       true);
	sprite->setAnimationSpeed(JUMP_UP,   10); sprite->setAnimationLoop(JUMP_UP,   false);
	sprite->setAnimationSpeed(JUMP_FALL, 10); sprite->setAnimationLoop(JUMP_FALL, false);
	sprite->setAnimationSpeed(ATTACK,    12); sprite->setAnimationLoop(ATTACK,    false);
	sprite->setAnimationSpeed(HURT,      10); sprite->setAnimationLoop(HURT,      false);
	sprite->setAnimationSpeed(DEATH,      8); sprite->setAnimationLoop(DEATH,     false);

	// Row 0: Run (8 frames)
	for (int f = 0; f < E2_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN, glm::vec2(f * fs.x, float(ROW_RUN_PX) / texH));
	// Row 1: Jump up (frames 0-3)
	for (int f = 0; f < E2_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP, glm::vec2(f * fs.x, float(ROW_JUMP_PX) / texH));
	// Row 1: Jump fall (frames 4-6)
	for (int f = 0; f < E2_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL, glm::vec2((f + E2_JUMP_UP_FRAMES) * fs.x, float(ROW_JUMP_PX) / texH));
	// Row 2: Attack (4 frames)
	for (int f = 0; f < E2_ATTACK_FRAMES; ++f)
		sprite->addKeyframe(ATTACK, glm::vec2(f * fs.x, float(ROW_ATTACK_PX) / texH));
	// Row 3: Hurt (2 frames)
	for (int f = 0; f < E2_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT, glm::vec2(f * fs.x, float(ROW_HURT_PX) / texH));
	// Row 4: Death (6 frames)
	for (int f = 0; f < E2_DEATH_FRAMES; ++f)
		sprite->addKeyframe(DEATH, glm::vec2(f * fs.x, float(ROW_DEATH_PX) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);
	tileMapDispl = tileMapPos;
}


// =====================================================================
//  BFS pathfinding (same algorithm as Enemy1)
// =====================================================================

void Enemy2::computePath(const glm::vec2 &playerPos)
{
	path.clear();
	pathIndex = 0;
	if (!map) return;

	int ts = map->getTileSize();
	glm::ivec2 ms = map->getMapSize();
	int W = ms.x;

	int ex = (posEnemy.x + E2_HITBOX_WIDTH / 2) / ts;
	int ey = posEnemy.y / ts;
	int px = ((int)playerPos.x + 16) / ts;
	int py = ((int)playerPos.y + 63) / ts;

	ex = std::max(0, std::min(ex, W - 1));
	ey = std::max(0, std::min(ey, ms.y - 1));
	px = std::max(0, std::min(px, W - 1));
	py = std::max(0, std::min(py, ms.y - 1));

	if (!e2IsGround(ex, ey, map))
	{
		int ly = e2LandingY(ex, ey, map);
		if (ly >= 0) ey = ly;
	}
	if (!e2IsGround(px, py, map))
	{
		if (py + 1 < ms.y && e2IsGround(px, py + 1, map)) py++;
		else if (py - 1 >= 0 && e2IsGround(px, py - 1, map)) py--;
		else { int ly = e2LandingY(px, py, map); if (ly >= 0) py = ly; }
	}

	glm::ivec2 start(ex, ey);
	glm::ivec2 goal(px, py);
	if (start == goal) return;

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
		glm::ivec2 c = q.front(); q.pop();
		std::vector<glm::ivec2> nbrs;

		// Walk / Fall
		for (int dx = -1; dx <= 1; dx += 2)
		{
			int nx = c.x + dx;
			if (nx < 0 || nx >= W) continue;
			if (e2IsSolid(nx, c.y, map)) continue;
			if (e2IsGround(nx, c.y, map))
				nbrs.push_back(glm::ivec2(nx, c.y));
			else
			{
				int ly = e2LandingY(nx, c.y, map);
				if (ly >= 0) nbrs.push_back(glm::ivec2(nx, ly));
			}
		}

		// Jump
		for (int dir = -1; dir <= 1; dir++)
		{
			float jpx = c.x * ts + ts / 2.f;
			float jpy = (float)(c.y * ts);
			float jStartY = jpy;
			bool landed = false;

			for (int ang = E2_JUMP_ANGLE_STEP; ang <= 180; ang += E2_JUMP_ANGLE_STEP)
			{
				jpy = jStartY - E2_JUMP_HEIGHT * sin(3.14159f * ang / 180.f);
				jpx += dir * E2_SPEED;
				int ttx = (int)jpx / ts;
				int tty = (int)jpy / ts;
				if (ttx < 0 || ttx >= W || tty < 0 || tty >= ms.y) break;
				if (e2IsSolid(ttx, tty, map)) break;
				if (ang >= 90 && e2IsGround(ttx, tty, map))
				{
					nbrs.push_back(glm::ivec2(ttx, tty));
					landed = true;
					break;
				}
			}

			if (!landed && dir != 0)
			{
				int endTX = (int)jpx / ts;
				int endTY = (int)jpy / ts;
				if (endTX >= 0 && endTX < W && endTY >= 0 && endTY < ms.y && !e2IsSolid(endTX, endTY, map))
				{
					int ly = e2LandingY(endTX, endTY, map);
					if (ly >= 0) nbrs.push_back(glm::ivec2(endTX, ly));
				}
			}
		}

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
//  Update
// =====================================================================

void Enemy2::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
	float dt = float(deltaTime) / 1000.0f;

	float renderOffsetX = 0.5f * float(E2_RENDER_WIDTH - E2_HITBOX_WIDTH);
	float renderOffsetY = float(E2_RENDER_HEIGHT - E2_HITBOX_HEIGHT);

	// --- Death animation, then vanish ---
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += E2_GRAVITY * dt;
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

	{
		glm::ivec2 groundedProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		onGround = map->checkCollision(groundedProbe, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY);
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
			posEnemyF.x += float(dir * 90);
			glm::ivec2 dashPos(int(posEnemyF.x), int(posEnemyF.y));
			map->checkCollision(dashPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), dir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x);
			posEnemyF.x = float(dashPos.x);
			posEnemy = dashPos;
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
		verticalVelocity += E2_GRAVITY * dt;
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
		verticalVelocity += E2_GRAVITY * dt;
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
		verticalVelocity += E2_GRAVITY * dt;
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
			attackCooldown = ATTACK_COOLDOWN_FRAMES;
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

		if (playerInFront && std::abs(dx) < MELEE_DETECT_RANGE && std::abs(dy) < MELEE_DETECT_VERTICAL)
		{
			bAttacking = true;
			sprite->changeAnimation(ATTACK);
			return;
		}
	}

	// --- Normal movement (BFS pathfinding) ---
	sprite->update(deltaTime);

	int ts = map->getTileSize();

	if (--pathRecalcTimer <= 0)
	{
		computePath(playerPos);
		pathRecalcTimer = PATH_RECALC_FRAMES;
	}

	int myTX = (posEnemy.x + E2_HITBOX_WIDTH / 2) / ts;
	int myTY = posEnemy.y / ts;

	bool wantLeft = false, wantRight = false, wantJump = false;

	while (pathIndex < (int)path.size())
	{
		glm::ivec2 wp = path[pathIndex];
		if (std::abs(posEnemy.x - wp.x * ts) < ts / 2 && std::abs(myTY - wp.y) <= 1)
			pathIndex++;
		else
			break;
	}

	if (pathIndex < (int)path.size())
	{
		glm::ivec2 target = path[pathIndex];
		int targetCenterPX = target.x * ts + ts / 2;
		int myCenterPX = posEnemy.x + E2_HITBOX_WIDTH / 2;
		if (myCenterPX < targetCenterPX - 4)       wantRight = true;
		else if (myCenterPX > targetCenterPX + 4)  wantLeft  = true;
		if (target.y < myTY) wantJump = true;
	}

	bool blocked = false;
	if (wantLeft)
	{
		facingLeft = true;
		sprite->setFlipHorizontal(true);
		posEnemyF.x -= E2_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::LEFT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}
	else if (wantRight)
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		posEnemyF.x += E2_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::RIGHT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}

	if ((wantJump || blocked) && !bJumping && onGround)
	{
		bJumping = true;
		onGround = false;
		verticalVelocity = -E2_JUMP_VELOCITY;
	}

	{
		glm::ivec2 groundProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		if (map->checkCollision(groundProbe, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY) && verticalVelocity >= 0.0f)
		{
			onGround = true;
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	// Animation
	if (onGround)
	{
		if (sprite->animation() != RUN) sprite->changeAnimation(RUN);
	}
	else
	{
		if (verticalVelocity < 0.0f && sprite->animation() != JUMP_UP)        sprite->changeAnimation(JUMP_UP);
		else if (verticalVelocity >= 0.0f && sprite->animation() != JUMP_FALL) sprite->changeAnimation(JUMP_FALL);
	}

	// Physics
	verticalVelocity += E2_GRAVITY * dt;
	posEnemyF.y += verticalVelocity * dt;
	glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
	if (verticalVelocity > 0.0f)
	{
		onGround = map->checkCollision(fallPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
	}
	else if (verticalVelocity < 0.0f)
	{
		if (map->checkCollision(fallPos, glm::ivec2(E2_HITBOX_WIDTH, E2_HITBOX_HEIGHT), CollisionDir::UP, &fallPos.y))
		{
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
		}
		bJumping = true;
	}
	posEnemy = fallPos;

	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}

void Enemy2::render()
{
	if (!alive && !bDying) return;

	// Blink on hit (unless dying)
	if (!bDying && hitTimer > (HIT_INVINCIBILITY_FRAMES - HIT_BLINK_FRAMES) && (hitTimer / 4) % 2 != 0) return;

	sprite->render();
}

void Enemy2::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Enemy2::setPosition(const glm::vec2 &pos)
{
	posEnemyF = pos;
	posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
	constexpr float offsetX = 0.5f * float(E2_RENDER_WIDTH - E2_HITBOX_WIDTH);
	constexpr float offsetY = float(E2_RENDER_HEIGHT - E2_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - offsetX, float(tileMapDispl.y + posEnemy.y) - offsetY));
}

void Enemy2::setActive(bool value)
{
	alive = value;
	if (!value)
		bDying = false;
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
		return glm::vec4(posEnemy.x - MELEE_HITBOX_REACH, posEnemy.y, MELEE_HITBOX_REACH, E2_HITBOX_HEIGHT);
	else
		return glm::vec4(posEnemy.x + E2_HITBOX_WIDTH, posEnemy.y, MELEE_HITBOX_REACH, E2_HITBOX_HEIGHT);
}
