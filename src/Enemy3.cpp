#include <cmath>
#include <queue>
#include <map>
#include <algorithm>
#include <GL/glew.h>
#include "Enemy3.h"


// Enemy3.png: 960x576, 96x96 per frame, 10 cols x 6 rows
// Row 0 (y=  0): Run      —  8 frames
// Row 1 (y= 96): Jump     — 10 frames (0-4 up, 5-9 fall)
// Row 2 (y=192): Hurt     —  2 frames
// Row 3 (y=288): Dead     — 10 frames
// Row 4 (y=384): Attack   — 10 frames  (fire cast)
// Row 5 (y=480): Attack2  —  7 frames  (close melee)
//
// Enemy3_Fire.png: 352x48, 32x48 per frame, 11 frames (fire projectile)

// Sprite dimensions: texture frame is 96x96 (square), rendered at 64x64 (max height target)
#define E3_FRAME_WIDTH     96   // UV sampling width
#define E3_FRAME_HEIGHT    96   // UV sampling height
#define E3_RENDER_WIDTH    64   // render quad width  (96 * 64/96 = 64, already a clean multiple)
#define E3_RENDER_HEIGHT   64   // render quad height
#define E3_HITBOX_WIDTH    32   // collision box, centered inside the render quad
#define E3_HITBOX_HEIGHT   32
#define E3_SPEED            1   // px per tick horizontal movement
#define E3_FALL_STEP        4   // px per tick gravity fallback (integer physics)
#define E3_JUMP_ANGLE_STEP  4   // degrees per tick in the jump arc simulation
#define E3_JUMP_HEIGHT    112   // max jump height in px; used in v = sqrt(2gh)

#define E3_RUN_FRAMES       8
#define E3_JUMP_UP_FRAMES   5
#define E3_JUMP_FALL_FRAMES 5
#define E3_HURT_FRAMES      2
#define E3_DEAD_FRAMES     10
#define E3_ATTACK_FRAMES    7   // fire cast — row 5
#define E3_ATTACK2_FRAMES  10   // melee — row 4

#define ROW_RUN_PX      0
#define ROW_JUMP_PX    96
#define ROW_HURT_PX   192
#define ROW_DEAD_PX   288
#define ROW_ATTACK_PX 480   // cast (fire) — row 5
#define ROW_ATK2_PX   384   // melee — row 4

// Fire projectile (Enemy3_Fire.png: 352x48, 32x48, 11 frames)
#define FIRE_FRAME_WIDTH   32
#define FIRE_FRAME_HEIGHT  48
#define FIRE_RENDER_WIDTH  32
#define FIRE_RENDER_HEIGHT 48
#define FIRE_FRAMES        11
#define FIRE_ANIM_FPS       5       // fps — 11 frames @ 5fps = ~2.2s total
#define FIRE_SPEED          3       // px per game frame
#define FIRE_DETECT_RANGE 180       // px — medium range (mage keeps distance)
#define FIRE_DETECT_VERT   48

// Melee (Attack2) — very close range
#define MELEE_DETECT_RANGE  40
#define MELEE_DETECT_VERT   48

#define PATH_RECALC_FRAMES       30   // ticks between BFS pathfinder recalculations
#define HIT_INVINCIBILITY_FRAMES 150  // ticks of invincibility after taking damage
#define HIT_BLINK_FRAMES          50  // ticks during which the sprite blinks
#define KNOCKBACK_FRAMES           8  // ticks the enemy is pushed back after a hit
#define KNOCKBACK_SPEED            5  // px per tick during knockback
#define ATTACK_COOLDOWN_FRAMES    90  // ticks between attacks (fire or melee)

static const float E3_GRAVITY = 1400.0f;
static const float E3_JUMP_VELOCITY = std::sqrt(2.0f * E3_GRAVITY * float(E3_JUMP_HEIGHT));
static const float E3_SPRING_JUMP_VELOCITY = E3_JUMP_VELOCITY * std::sqrt(3.0f);
static const int E3_DASH_DURATION_MS = 1000;
static const float E3_DASH_DISTANCE_BASE = 60.0f;


enum Enemy3Anims
{
	RUN, JUMP_UP, JUMP_FALL, HURT, DEAD, ATTACK, ATTACK2
};


// =====================================================================
//  BFS helpers
// =====================================================================

static bool e3IsSolid(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return true;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts)) == TileType::SOLID;
}

static bool e3IsGround(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return false;
	if (e3IsSolid(tx, ty, m)) return false;
	int below = ty + 1;
	if (below >= sz.y) return true;
	TileType t = m->getTileTypeAtPos(glm::ivec2(tx * ts, below * ts));
	return t == TileType::SOLID || t == TileType::ONE_WAY_PLATFORM;
}

static int e3LandingY(int tx, int startTY, const TileMap *m)
{
	glm::ivec2 sz = m->getMapSize();
	for (int y = startTY; y < sz.y; y++)
	{
		if (e3IsSolid(tx, y, m)) return -1;
		if (e3IsGround(tx, y, m)) return y;
	}
	return -1;
}


// =====================================================================
//  Enemy3 implementation
// =====================================================================

Enemy3::Enemy3()
{
	sprite    = NULL;
	fireSprite = NULL;
	map       = NULL;
	posEnemyF = glm::vec2(0.0f, 0.0f);
	verticalVelocity = 0.0f;
}

Enemy3::~Enemy3()
{
	if (sprite)     delete sprite;
	if (fireSprite) delete fireSprite;
}

void Enemy3::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	facingLeft      = false;
	bJumping        = false;
	onGround        = false;
	bCasting        = false;
	bMelee          = false;
	alive           = true;
	bDying          = false;
	health          = 2;
	jumpAngle       = 0;
	startY          = 0;
	pathRecalcTimer = 0;
	pathIndex       = 0;
	attackCooldown  = 0;
	springCooldown  = 0;
	dashCooldown    = 0;
	hitTimer        = 0;
	knockbackFrames = 0;
	knockbackDir    = 0;
	dashTimeLeftMs = 0;
	dashVelocity = 0.0f;
	dashVelocityStart = 0.0f;

	// --- Main spritesheet ---
	spritesheet.loadFromFile("../images/Enemy3.png", TEXTURE_PIXEL_FORMAT_RGBA);
	spritesheet.setWrapS(GL_CLAMP_TO_EDGE);
	spritesheet.setWrapT(GL_CLAMP_TO_EDGE);
	spritesheet.setMinFilter(GL_NEAREST);
	spritesheet.setMagFilter(GL_NEAREST);

	float texW = float(spritesheet.width());
	float texH = float(spritesheet.height());
	glm::vec2 fs(float(E3_FRAME_WIDTH) / texW, float(E3_FRAME_HEIGHT) / texH);

	sprite = Sprite::createSprite(glm::ivec2(E3_RENDER_WIDTH, E3_RENDER_HEIGHT), fs, &spritesheet, &shaderProgram);
	sprite->setNumberAnimations(7);

	sprite->setAnimationSpeed(RUN,     10); sprite->setAnimationLoop(RUN,     true);
	sprite->setAnimationSpeed(JUMP_UP, 10); sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationSpeed(JUMP_FALL,10);sprite->setAnimationLoop(JUMP_FALL,false);
	sprite->setAnimationSpeed(HURT,    10); sprite->setAnimationLoop(HURT,    false);
	sprite->setAnimationSpeed(DEAD,     8); sprite->setAnimationLoop(DEAD,    false);
	sprite->setAnimationSpeed(ATTACK,  10); sprite->setAnimationLoop(ATTACK,  false);
	sprite->setAnimationSpeed(ATTACK2, 12); sprite->setAnimationLoop(ATTACK2, false);

	for (int f = 0; f < E3_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN,      glm::vec2(f * fs.x, float(ROW_RUN_PX)    / texH));
	for (int f = 0; f < E3_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP,  glm::vec2(f * fs.x, float(ROW_JUMP_PX)   / texH));
	for (int f = 0; f < E3_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL,glm::vec2((f + E3_JUMP_UP_FRAMES) * fs.x, float(ROW_JUMP_PX) / texH));
	for (int f = 0; f < E3_HURT_FRAMES; ++f)
		sprite->addKeyframe(HURT,     glm::vec2(f * fs.x, float(ROW_HURT_PX)   / texH));
	for (int f = 0; f < E3_DEAD_FRAMES; ++f)
		sprite->addKeyframe(DEAD,     glm::vec2(f * fs.x, float(ROW_DEAD_PX)   / texH));
	for (int f = 0; f < E3_ATTACK_FRAMES; ++f)
		sprite->addKeyframe(ATTACK,   glm::vec2(f * fs.x, float(ROW_ATTACK_PX) / texH));
	for (int f = 0; f < E3_ATTACK2_FRAMES; ++f)
		sprite->addKeyframe(ATTACK2,  glm::vec2(f * fs.x, float(ROW_ATK2_PX)   / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);

	// --- Fire projectile spritesheet (Enemy3_Fire.png: 352x48, 32x48, 11 frames) ---
	fireSpritesheet.loadFromFile("../images/Enemy3_Fire.png", TEXTURE_PIXEL_FORMAT_RGBA);
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

	tileMapDispl = tileMapPos;
}


// =====================================================================
//  BFS pathfinding
// =====================================================================

void Enemy3::computePath(const glm::vec2 &playerPos)
{
	path.clear();
	pathIndex = 0;
	if (!map) return;

	int ts = map->getTileSize();
	glm::ivec2 ms = map->getMapSize();
	int W = ms.x;

	int ex = (posEnemy.x + E3_HITBOX_WIDTH / 2) / ts;
	int ey = posEnemy.y / ts;
	int px = ((int)playerPos.x + 16) / ts;
	int py = ((int)playerPos.y + 63) / ts;

	ex = std::max(0, std::min(ex, W - 1));
	ey = std::max(0, std::min(ey, ms.y - 1));
	px = std::max(0, std::min(px, W - 1));
	py = std::max(0, std::min(py, ms.y - 1));

	if (!e3IsGround(ex, ey, map)) { int ly = e3LandingY(ex, ey, map); if (ly >= 0) ey = ly; }
	if (!e3IsGround(px, py, map))
	{
		if      (py + 1 < ms.y && e3IsGround(px, py + 1, map)) py++;
		else if (py - 1 >= 0   && e3IsGround(px, py - 1, map)) py--;
		else { int ly = e3LandingY(px, py, map); if (ly >= 0) py = ly; }
	}

	glm::ivec2 start(ex, ey), goal(px, py);
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

		for (int dx = -1; dx <= 1; dx += 2)
		{
			int nx = c.x + dx;
			if (nx < 0 || nx >= W || e3IsSolid(nx, c.y, map)) continue;
			if (e3IsGround(nx, c.y, map)) nbrs.push_back(glm::ivec2(nx, c.y));
			else { int ly = e3LandingY(nx, c.y, map); if (ly >= 0) nbrs.push_back(glm::ivec2(nx, ly)); }
		}

		for (int dir = -1; dir <= 1; dir++)
		{
			float jpx = c.x * ts + ts / 2.f, jpy = (float)(c.y * ts), jStartY = jpy;
			bool landed = false;
			for (int ang = E3_JUMP_ANGLE_STEP; ang <= 180; ang += E3_JUMP_ANGLE_STEP)
			{
				jpy = jStartY - E3_JUMP_HEIGHT * sin(3.14159f * ang / 180.f);
				jpx += dir * E3_SPEED;
				int ttx = (int)jpx / ts, tty = (int)jpy / ts;
				if (ttx < 0 || ttx >= W || tty < 0 || tty >= ms.y || e3IsSolid(ttx, tty, map)) break;
				if (ang >= 90 && e3IsGround(ttx, tty, map)) { nbrs.push_back(glm::ivec2(ttx, tty)); landed = true; break; }
			}
			if (!landed && dir != 0)
			{
				int endTX = (int)jpx / ts, endTY = (int)jpy / ts;
				if (endTX >= 0 && endTX < W && endTY >= 0 && endTY < ms.y && !e3IsSolid(endTX, endTY, map))
				{ int ly = e3LandingY(endTX, endTY, map); if (ly >= 0) nbrs.push_back(glm::ivec2(endTX, ly)); }
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
		while (c != start && c.x != -1) { path.push_back(c); c = parent[key(c)]; }
		std::reverse(path.begin(), path.end());
	}
}


// =====================================================================
//  Update
// =====================================================================

void Enemy3::update(int deltaTime, const glm::vec2 &playerPos)
{
	if (!alive && !bDying) return;
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
		if (e3IsSolid(checkX / ts, checkY / ts, map)) { fireballs.erase(fireballs.begin() + i); continue; }

	}

	// --- Death animation ---
	if (bDying)
	{
		sprite->update(deltaTime);
		verticalVelocity += E3_GRAVITY * dt;
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

	{
		glm::ivec2 groundedProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		onGround = map->checkCollision(groundedProbe, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY);
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
		verticalVelocity += E3_GRAVITY * dt;
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
		verticalVelocity += E3_GRAVITY * dt;
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

	// --- Fire cast: freeze until animation finishes, then spawn fireball ---
	if (bCasting)
	{
		sprite->update(deltaTime);
		verticalVelocity += E3_GRAVITY * dt;
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
			attackCooldown = ATTACK_COOLDOWN_FRAMES;
			sprite->changeAnimation(RUN);
		}
		return;
	}

	// --- Melee (Attack2): freeze until animation finishes ---
	if (bMelee)
	{
		sprite->update(deltaTime);
		verticalVelocity += E3_GRAVITY * dt;
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
			attackCooldown = ATTACK_COOLDOWN_FRAMES;
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
		if (inFront && std::abs(dx) < MELEE_DETECT_RANGE && std::abs(dy) < MELEE_DETECT_VERT)
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

	// --- Normal movement (BFS pathfinding) ---
	sprite->update(deltaTime);

	if (--pathRecalcTimer <= 0)
	{
		computePath(playerPos);
		pathRecalcTimer = PATH_RECALC_FRAMES;
	}

	int myTX = (posEnemy.x + E3_HITBOX_WIDTH / 2) / ts;
	int myTY = posEnemy.y / ts;

	bool wantLeft = false, wantRight = false, wantJump = false;

	while (pathIndex < (int)path.size())
	{
		glm::ivec2 wp = path[pathIndex];
		if (std::abs(posEnemy.x - wp.x * ts) < ts / 2 && std::abs(myTY - wp.y) <= 1)
			pathIndex++;
		else break;
	}

	if (pathIndex < (int)path.size())
	{
		glm::ivec2 target = path[pathIndex];
		int targetCX = target.x * ts + ts / 2;
		int myCX     = posEnemy.x + E3_HITBOX_WIDTH / 2;
		if      (myCX < targetCX - 4) wantRight = true;
		else if (myCX > targetCX + 4) wantLeft  = true;
		if (target.y < myTY)          wantJump  = true;
	}

	bool blocked = false;
	if (wantLeft)
	{
		facingLeft = true;
		sprite->setFlipHorizontal(true);
		posEnemyF.x -= E3_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::LEFT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}
	else if (wantRight)
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		posEnemyF.x += E3_SPEED;
		glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
		if (map->checkCollision(movePos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::RIGHT, &movePos.x))
			blocked = true;
		posEnemyF.x = float(movePos.x);
		posEnemy = movePos;
	}

	if (dashTimeLeftMs > 0 && dashVelocity != 0.0f)
	{
		float ratio = float(dashTimeLeftMs) / float(E3_DASH_DURATION_MS);
		dashVelocity = dashVelocityStart * ratio;
		float dashDelta = dashVelocity * float(deltaTime);
		int dashSteps = int(std::ceil(std::abs(dashDelta)));
		int dashStepDir = (dashDelta < 0.0f) ? -1 : 1;
		for (int step = 0; step < dashSteps; ++step)
		{
			posEnemyF.x += float(dashStepDir);
			glm::ivec2 dashPos(int(posEnemyF.x), int(posEnemyF.y));
			if (map->checkCollision(dashPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), dashStepDir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x))
			{
				posEnemyF.x = float(dashPos.x);
				dashTimeLeftMs = 0;
				dashVelocity = 0.0f;
				break;
			}
		}
	}

	if ((wantJump || blocked) && !bJumping && onGround)
	{
		bJumping  = true;
		onGround  = false;
		verticalVelocity = -E3_JUMP_VELOCITY;
	}

	{
		glm::ivec2 groundProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		if (map->checkCollision(groundProbe, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &groundedY) && verticalVelocity >= 0.0f)
		{
			onGround = true;
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	if (onGround)
	{
		if (sprite->animation() != RUN) sprite->changeAnimation(RUN);
	}
	else
	{
		if      (verticalVelocity <  0.0f && sprite->animation() != JUMP_UP)   sprite->changeAnimation(JUMP_UP);
		else if (verticalVelocity >= 0.0f && sprite->animation() != JUMP_FALL) sprite->changeAnimation(JUMP_FALL);
	}

	verticalVelocity += E3_GRAVITY * dt;
	posEnemyF.y += verticalVelocity * dt;
	glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
	if (verticalVelocity > 0.0f)
	{
		onGround = map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::DOWN, &fallPos.y);
		if (onGround) {
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
			bJumping = false;
		}
	}
	else if (verticalVelocity < 0.0f)
	{
		if (map->checkCollision(fallPos, glm::ivec2(E3_HITBOX_WIDTH, E3_HITBOX_HEIGHT), CollisionDir::UP, &fallPos.y))
		{
			posEnemyF.y = float(fallPos.y);
			verticalVelocity = 0.0f;
		}
		bJumping = true;
	}
	posEnemy = fallPos;

	if (dashTimeLeftMs > 0)
	{
		dashTimeLeftMs -= deltaTime;
		if (dashTimeLeftMs <= 0)
		{
			dashTimeLeftMs = 0;
			dashVelocity = 0.0f;
		}
	}

	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
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

	sprite->render();
}


void Enemy3::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Enemy3::setPosition(const glm::vec2 &pos)
{
	posEnemyF = pos;
	posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
	float offsetX = 0.5f * float(E3_RENDER_WIDTH  - E3_HITBOX_WIDTH);
	float offsetY =        float(E3_RENDER_HEIGHT - E3_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - offsetX, float(tileMapDispl.y + posEnemy.y) - offsetY));
}

void Enemy3::setActive(bool value)
{
	alive = value;
	if (!value)
		bDying = false;
}

void Enemy3::takeDamage(int knockDir)
{
	if (!alive) return;
	health--;
	bCasting = false;
	bMelee   = false;
	if (health <= 0)
	{
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
		return glm::vec4(posEnemy.x - MELEE_DETECT_RANGE, posEnemy.y, MELEE_DETECT_RANGE, E3_HITBOX_HEIGHT);
	else
		return glm::vec4(posEnemy.x + E3_HITBOX_WIDTH, posEnemy.y, MELEE_DETECT_RANGE, E3_HITBOX_HEIGHT);
}
