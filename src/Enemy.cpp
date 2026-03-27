#include <cmath>
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

#define ROW_RUN_TOP_PX   0
#define ROW_JUMP_TOP_PX 48

#define PATH_RECALC_FRAMES 30


enum EnemyAnims
{
	RUN, JUMP_UP, JUMP_FALL
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
	map = NULL;
}

Enemy::~Enemy()
{
	if (sprite != NULL)
		delete sprite;
}

void Enemy::init(const glm::ivec2 &tileMapPos, ShaderProgram &shaderProgram)
{
	facingLeft = false;
	bJumping = false;
	onGround = false;
	jumpAngle = 0;
	startY = 0;
	pathRecalcTimer = 0;
	pathIndex = 0;

	spritesheet.loadFromFile("images/Enemy1_Run_Jump.png", TEXTURE_PIXEL_FORMAT_RGBA);
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
	sprite->setNumberAnimations(3);

	sprite->setAnimationSpeed(RUN, 10);
	sprite->setAnimationLoop(RUN, true);
	sprite->setAnimationSpeed(JUMP_UP, 10);
	sprite->setAnimationLoop(JUMP_UP, false);
	sprite->setAnimationSpeed(JUMP_FALL, 10);
	sprite->setAnimationLoop(JUMP_FALL, false);

	// Row 0: Run (8 frames)
	for (int f = 0; f < ENEMY_RUN_FRAMES; ++f)
		sprite->addKeyframe(RUN, glm::vec2(f * frameSizeInTexture.x, float(ROW_RUN_TOP_PX) / texH));
	// Row 1: Jump up (frames 0-3)
	for (int f = 0; f < ENEMY_JUMP_UP_FRAMES; ++f)
		sprite->addKeyframe(JUMP_UP, glm::vec2(f * frameSizeInTexture.x, float(ROW_JUMP_TOP_PX) / texH));
	// Row 1: Jump fall (frames 4-7)
	for (int f = 0; f < ENEMY_JUMP_FALL_FRAMES; ++f)
		sprite->addKeyframe(JUMP_FALL, glm::vec2((f + ENEMY_JUMP_FALL_START) * frameSizeInTexture.x, float(ROW_JUMP_TOP_PX) / texH));

	sprite->changeAnimation(RUN);
	sprite->setFlipHorizontal(false);
	tileMapDispl = tileMapPos;
}


// =====================================================================
//  BFS pathfinding
//
//  State = tile (x, y).  Only "standing" tiles are valid nodes
//  (empty tile with solid/one-way ground directly below).
//
//  Edges (from each standing tile):
//    Walk left/right  → adjacent standing tile
//    Fall             → walk to empty tile, simulate gravity to find landing
//    Jump left/right/up → simulate parabolic arc, find landing tile
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
//  Update — follow the BFS path instead of blindly chasing player X
// =====================================================================

void Enemy::update(int deltaTime, const glm::vec2 &playerPos)
{
	sprite->update(deltaTime);

	// Recompute path periodically
	if (--pathRecalcTimer <= 0)
	{
		computePath(playerPos);
		pathRecalcTimer = PATH_RECALC_FRAMES;
	}

	// --- Determine desired movement from path ---
	int ts = map->getTileSize();
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
		posEnemy.x -= ENEMY_SPEED;
		if (map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::LEFT, &posEnemy.x))
			blocked = true;
	}
	else if (wantRight)
	{
		facingLeft = false;
		sprite->setFlipHorizontal(false);
		posEnemy.x += ENEMY_SPEED;
		if (map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::RIGHT, &posEnemy.x))
			blocked = true;
	}

	// --- Jump trigger ---
	if ((wantJump || blocked) && !bJumping && onGround)
	{
		bJumping = true;
		jumpAngle = 0;
		startY = posEnemy.y;
	}

	// --- Animation ---
	if (!bJumping)
	{
		if (sprite->animation() != RUN)
			sprite->changeAnimation(RUN);
	}
	else
	{
		if (jumpAngle < 90 && sprite->animation() != JUMP_UP)
			sprite->changeAnimation(JUMP_UP);
		else if (jumpAngle >= 90 && sprite->animation() != JUMP_FALL)
			sprite->changeAnimation(JUMP_FALL);
	}

	// --- Jump / gravity physics (unchanged) ---
	if (bJumping)
	{
		jumpAngle += ENEMY_JUMP_ANGLE_STEP;
		if (jumpAngle >= 180)
		{
			bJumping = false;
			posEnemy.y = startY;
		}
		else
		{
			posEnemy.y = int(startY - ENEMY_JUMP_HEIGHT * sin(3.14159f * jumpAngle / 180.f));
			if (jumpAngle < 90 && map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::UP, &posEnemy.y))
			{
				jumpAngle = 90;
				startY = posEnemy.y + ENEMY_JUMP_HEIGHT;
			}
			if (jumpAngle > 90)
				bJumping = !map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &posEnemy.y);
		}
	}
	else
	{
		posEnemy.y += ENEMY_FALL_STEP;
		onGround = map->checkCollision(posEnemy, glm::ivec2(ENEMY_HITBOX_WIDTH, ENEMY_HITBOX_HEIGHT), CollisionDir::DOWN, &posEnemy.y);
	}

	// --- Sprite position ---
	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}

void Enemy::render()
{
	sprite->render();
}

void Enemy::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void Enemy::setPosition(const glm::vec2 &pos)
{
	posEnemy = pos;
	float renderOffsetX = 0.5f * float(ENEMY_FRAME_WIDTH - ENEMY_HITBOX_WIDTH);
	float renderOffsetY = float(ENEMY_FRAME_HEIGHT - ENEMY_HITBOX_HEIGHT);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - renderOffsetX, float(tileMapDispl.y + posEnemy.y) - renderOffsetY));
}
