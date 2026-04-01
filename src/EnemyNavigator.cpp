#include "EnemyNavigator.h"

#include <cmath>
#include <queue>
#include <map>
#include <algorithm>

#include "TileMap.h"

namespace
{
bool isSolid(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return true;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts)) == TileType::SOLID;
}

bool isGround(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return false;
	if (isSolid(tx, ty, m)) return false;
	int below = ty + 1;
	if (below >= sz.y) return false;
	TileType t = m->getTileTypeAtPos(glm::ivec2(tx * ts, below * ts));
	return t == TileType::SOLID || t == TileType::ONE_WAY_PLATFORM;
}

bool isLadder(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return false;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts)) == TileType::LADDER;
}

TileType tileAt(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	glm::ivec2 sz = m->getMapSize();
	if (tx < 0 || tx >= sz.x || ty < 0 || ty >= sz.y) return TileType::SOLID;
	return m->getTileTypeAtPos(glm::ivec2(tx * ts, ty * ts));
}

bool isOnSpringTile(int tx, int ty, const TileMap *m)
{
	int ts = m->getTileSize();
	return m->isOnSpring(glm::ivec2(tx * ts, ty * ts), glm::ivec2(ts, ts));
}

bool isOnDashTile(int tx, int ty, const TileMap *m, int *dashDir)
{
	int ts = m->getTileSize();
	bool facingLeft = false;
	if (!m->isOnDash(glm::ivec2(tx * ts, ty * ts), glm::ivec2(ts, ts), &facingLeft))
		return false;
	if (dashDir)
		*dashDir = facingLeft ? -1 : 1;
	return true;
}

int landingY(int tx, int startTY, const TileMap *m)
{
	glm::ivec2 sz = m->getMapSize();
	for (int y = startTY; y < sz.y; y++)
	{
		if (isSolid(tx, y, m)) return -1;
		if (isGround(tx, y, m)) return y;
	}
	return -1;
}
}

std::vector<glm::ivec2> computeEnemyGroundPath(
	const TileMap *map,
	const glm::ivec2 &enemyPos,
	const glm::vec2 &playerPos,
	const EnemyNavParams &params)
{
	std::vector<glm::ivec2> path;
	if (!map) return path;

	int ts = map->getTileSize();
	glm::ivec2 ms = map->getMapSize();
	int W = ms.x;

	int ex = (enemyPos.x + params.hitboxWidth / 2) / ts;
	int ey = enemyPos.y / ts;
	int px = (int(playerPos.x) + params.playerCenterOffsetX) / ts;
	int py = (int(playerPos.y) + params.playerFeetOffsetY) / ts;

	ex = std::max(0, std::min(ex, W - 1));
	ey = std::max(0, std::min(ey, ms.y - 1));
	px = std::max(0, std::min(px, W - 1));
	py = std::max(0, std::min(py, ms.y - 1));

	if (!isGround(ex, ey, map) && !isLadder(ex, ey, map))
	{
		int ly = landingY(ex, ey, map);
		if (ly >= 0) ey = ly;
	}
	if (!isGround(px, py, map) && !isLadder(px, py, map))
	{
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
	if (start == goal) return path;

	auto key = [W](const glm::ivec2 &v) { return v.y * W + v.x; };

	std::queue<glm::ivec2> q;
	std::map<int, glm::ivec2> parent;
	q.push(start);
	parent[key(start)] = glm::ivec2(-1, -1);

	bool found = false;
	int iterations = 0;
	int jumpStep = params.jumpAngleStep > 0 ? params.jumpAngleStep : 4;
	int maxAngle = params.maxJumpAngle > 0 ? params.maxJumpAngle : 180;
	int maxIterations = params.maxIterations > 0 ? params.maxIterations : 2000;
	int dashReachTiles = 3;
	if (params.dashDistanceBasePx > 0)
		dashReachTiles = std::max(3, int(std::round(float(params.dashDistanceBasePx * 3) / float(ts))));

	auto allowLanding = [](int fromY, int toY) {
		if (toY < 0) return false;
		if (toY <= fromY) return true;
		return true;
	};

	while (!q.empty() && !found && iterations < maxIterations)
	{
		++iterations;
		glm::ivec2 c = q.front();
		q.pop();
		std::vector<glm::ivec2> nbrs;
		bool currentOnSpring = isOnSpringTile(c.x, c.y, map);
		int currentDashDir = 0;
		bool currentOnDash = isOnDashTile(c.x, c.y, map, &currentDashDir);

		// Ladder movement: climb up/down when current or destination tile is ladder.
		for (int dy = -1; dy <= 1; dy += 2)
		{
			int ny = c.y + dy;
			if (ny < 0 || ny >= ms.y) continue;
			if (isSolid(c.x, ny, map)) continue;
			if (isLadder(c.x, c.y, map) || isLadder(c.x, ny, map))
				nbrs.push_back(glm::ivec2(c.x, ny));
		}

		// One-way drop edge: allow intentional drop through platform under feet.
		if (c.y + 1 < ms.y && tileAt(c.x, c.y + 1, map) == TileType::ONE_WAY_PLATFORM)
		{
			int ly = landingY(c.x, c.y + 1, map);
			if (ly > c.y && allowLanding(c.y, ly)) nbrs.push_back(glm::ivec2(c.x, ly));
		}

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
				if (allowLanding(c.y, ly)) nbrs.push_back(glm::ivec2(nx, ly));
			}
		}

		for (int dir = -1; dir <= 1; dir++)
		{
			int effectiveJumpHeight = params.jumpHeight;
			if (currentOnSpring)
				effectiveJumpHeight = params.jumpHeight * 3;

			float jpx = c.x * ts + ts / 2.f;
			float jpy = float(c.y * ts);
			float jStartY = jpy;
			bool landed = false;

			for (int ang = jumpStep; ang <= maxAngle; ang += jumpStep)
			{
				jpy = jStartY - effectiveJumpHeight * std::sin(3.14159f * ang / 180.f);
				jpx += dir * params.horizontalSpeed;
				int ttx = int(jpx) / ts;
				int tty = int(jpy) / ts;
				if (ttx < 0 || ttx >= W || tty < 0 || tty >= ms.y) break;
				if (isSolid(ttx, tty, map)) break;
				if (ang >= 90 && isGround(ttx, tty, map))
				{
					nbrs.push_back(glm::ivec2(ttx, tty));
					landed = true;
					break;
				}
			}

			if (!landed && dir != 0)
			{
				int endTX = int(jpx) / ts;
				int endTY = int(jpy) / ts;
				if (endTX >= 0 && endTX < W && endTY >= 0 && endTY < ms.y && !isSolid(endTX, endTY, map))
				{
					int ly = landingY(endTX, endTY, map);
					if (allowLanding(c.y, ly)) nbrs.push_back(glm::ivec2(endTX, ly));
				}
			}
		}

		// Dash tiles add a horizontal launch edge in the tile direction.
		if (currentOnDash)
		{
			int nx = c.x + currentDashDir * dashReachTiles;
			nx = std::max(0, std::min(nx, W - 1));
			if (!isSolid(nx, c.y, map))
			{
				if (isGround(nx, c.y, map))
					nbrs.push_back(glm::ivec2(nx, c.y));
				else
				{
					int ly = landingY(nx, c.y, map);
					if (allowLanding(c.y, ly)) nbrs.push_back(glm::ivec2(nx, ly));
				}
			}
		}

		for (const auto &n : nbrs)
		{
			int k = key(n);
			if (parent.find(k) == parent.end())
			{
				parent[k] = c;
				if (n == goal)
				{
					found = true;
					break;
				}
				q.push(n);
			}
		}
	}

	if (!found) return path;

	glm::ivec2 c = goal;
	while (c != start && c.x != -1)
	{
		path.push_back(c);
		c = parent[key(c)];
	}
	std::reverse(path.begin(), path.end());
	return path;
}
