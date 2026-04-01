#ifndef _ENEMY_NAVIGATOR_INCLUDE
#define _ENEMY_NAVIGATOR_INCLUDE

#include <vector>
#include <glm/glm.hpp>

class TileMap;

struct EnemyNavParams
{
	int hitboxWidth;
	int jumpHeight;
	int horizontalSpeed;
	int dashDistanceBasePx;
	int jumpAngleStep;
	int maxIterations;
	int maxJumpAngle;
	int playerCenterOffsetX;
	int playerFeetOffsetY;
};

std::vector<glm::ivec2> computeEnemyGroundPath(
	const TileMap *map,
	const glm::ivec2 &enemyPos,
	const glm::vec2 &playerPos,
	const EnemyNavParams &params
);

#endif
