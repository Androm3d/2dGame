#include "BaseEnemy.h"
#include "Sprite.h"

#include <algorithm>
#include <cmath>

#include "GameConstants.h"

BaseEnemy::BaseEnemy()
{
	map = nullptr;
	facingLeft = false;
	bJumping = false;
	onGround = false;
	alive = true;
	bDying = false;
	health = 2;
	jumpAngle = 0;
	startY = 0;
	pathRecalcTimer = 0;
	pathIndex = 0;
	springCooldown = 0;
	dashCooldown = 0;
	hitTimer = 0;
	knockbackFrames = 0;
	knockbackDir = 0;
	dropThroughTimerMs = 0;
	dropCommitTimerMs = 0;
	dashTimeLeftMs = 0;
	tileMapDispl = glm::ivec2(0);
	posEnemy = glm::ivec2(0);
	posEnemyF = glm::vec2(0.0f);
	verticalVelocity = 0.0f;
	dashVelocity = 0.0f;
	dashVelocityStart = 0.0f;
	ladderAnimActive = false;
}

void BaseEnemy::setTileMap(TileMap *tileMap)
{
	map = tileMap;
}

void BaseEnemy::setActive(bool value)
{
	alive = value;
	if (!value)
		bDying = false;
}

void BaseEnemy::resetBaseState(const glm::ivec2 &tileMapPos)
{
	facingLeft = false;
	bJumping = false;
	onGround = false;
	alive = true;
	bDying = false;
	health = 2;
	jumpAngle = 0;
	startY = 0;
	pathRecalcTimer = 0;
	pathIndex = 0;
	springCooldown = 0;
	dashCooldown = 0;
	hitTimer = 0;
	knockbackFrames = 0;
	knockbackDir = 0;
	dropThroughTimerMs = 0;
	dropCommitTimerMs = 0;
	posEnemyF = glm::vec2(0.0f, 0.0f);
	verticalVelocity = 0.0f;
	dashTimeLeftMs = 0;
	dashVelocity = 0.0f;
	dashVelocityStart = 0.0f;
	ladderAnimActive = false;
	tileMapDispl = tileMapPos;
	path.clear();
	pathIndex = 0;
}

void BaseEnemy::setPositionCommon(const glm::vec2 &pos, int renderWidth, int renderHeight, int hitboxWidth, int hitboxHeight, Sprite *sprite)
{
	posEnemyF = pos;
	posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
	float offsetX = 0.5f * float(renderWidth - hitboxWidth);
	float offsetY = float(renderHeight - hitboxHeight);
	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - offsetX, float(tileMapDispl.y + posEnemy.y) - offsetY));
}

void BaseEnemy::computePathCommon(const glm::vec2 &playerPos, const EnemyNavParams &params)
{
	path.clear();
	pathIndex = 0;
	if (!map) return;
	path = computeEnemyGroundPath(map, posEnemy, playerPos, params);
}

void BaseEnemy::updatePathMovementCommon(int deltaTime, float dt, const glm::vec2 &playerPos, Sprite *sprite, const BaseEnemyMoveConfig &config)
{
	ladderAnimActive = false;
	sprite->update(deltaTime);

	int ts = map->getTileSize();

	if (pathRecalcTimer > 0)
		--pathRecalcTimer;

	glm::ivec2 ladderProbePos(int(posEnemyF.x) - 2, int(posEnemyF.y));
	glm::ivec2 ladderProbeSize(config.hitboxWidth + 4, config.hitboxHeight);
	bool onLadderNow = map->isOnLadder(ladderProbePos, ladderProbeSize);

	if (pathRecalcTimer <= 0)
	{
		computePath(playerPos);
		pathRecalcTimer = config.pathRecalcFrames;
	}

	int supportTileY = (int(posEnemyF.y) + config.hitboxHeight) / ts;
	int supportTileX = (int(posEnemyF.x) + config.hitboxWidth / 2) / ts;
	TileType supportType = map->getTileTypeAtPos(glm::ivec2(supportTileX * ts, supportTileY * ts));
	bool onOneWaySupport = (supportType == TileType::ONE_WAY_PLATFORM);

	int myTX = (posEnemy.x + config.hitboxWidth / 2) / ts;
	int myTY = posEnemy.y / ts;

	bool wantLeft = false, wantRight = false, wantJump = false;
	bool wantClimbUp = false, wantClimbDown = false, wantDrop = false;

	while (pathIndex < int(path.size()))
	{
		glm::ivec2 wp = path[pathIndex];
		int targetPX = wp.x * ts + ts / 2;
		int targetPY = wp.y * ts;
		int myCenterPX = posEnemy.x + config.hitboxWidth / 2;

		bool closeX = std::abs(myCenterPX - targetPX) <= 4;
		bool closeY = std::abs(posEnemy.y - targetPY) <= 4;

		if (closeX && closeY) pathIndex++;
		else break;
	}

	if (pathIndex < int(path.size()))
	{
		glm::ivec2 target = path[pathIndex];
		int targetCenterPX = target.x * ts + ts / 2;
		int targetPY = target.y * ts;
		int myCenterPX = posEnemy.x + config.hitboxWidth / 2;

		if (myCenterPX < targetCenterPX - 4) wantRight = true;
		else if (myCenterPX > targetCenterPX + 4) wantLeft = true;

		TileType here = map->getTileTypeAtPos(glm::ivec2(myTX * ts, myTY * ts));
		TileType there = map->getTileTypeAtPos(glm::ivec2(target.x * ts, target.y * ts));
		bool ladderTransition = (here == TileType::LADDER || there == TileType::LADDER);

		if (ladderTransition)
		{
			if (posEnemy.y > targetPY + 4) wantClimbUp = true;
			else if (posEnemy.y < targetPY - 4) wantClimbDown = true;
		}
		else if (target.y > myTY)
		{
			wantDrop = onOneWaySupport;
		}
		else if (target.y < myTY)
		{
			wantJump = true;
		}
	}

	bool wantsLadderDetach = (onLadderNow && (wantLeft || wantRight) && !wantClimbUp && !wantClimbDown);

	if ((wantLeft || wantRight) && onLadderNow && !wantClimbUp && !wantClimbDown)
	{
		int sideTX = wantLeft ? (myTX - 1) : (myTX + 1);
		if (sideTX >= 0 && sideTX < map->getMapSize().x)
		{
			int feetTY = (int(posEnemyF.y) + config.hitboxHeight - 1) / ts;
			if (map->getTileTypeAtPos(glm::ivec2(sideTX * ts, feetTY * ts)) == TileType::SOLID)
			{
				int headTY = (int(posEnemyF.y) - 1) / ts;
				if (headTY >= 0 && map->getTileTypeAtPos(glm::ivec2(myTX * ts, headTY * ts)) == TileType::SOLID)
				{
					wantJump = true;
					wantClimbUp = false;
				}
				else
				{
					wantClimbUp = true;
					wantLeft = false;
					wantRight = false;
					wantsLadderDetach = false;
				}
			}
		}
	}

	if (dropCommitTimerMs > 0)
	{
		wantDrop = true;
		wantClimbUp = false;
		wantClimbDown = false;
		wantJump = false;
		onLadderNow = false;
		wantsLadderDetach = true;
	}

	bool blocked = false;
	if (dashTimeLeftMs > 0)
	{
		float ratio = float(dashTimeLeftMs) / float(config.dashDurationMs);
		dashVelocity = dashVelocityStart * ratio;
		float dashDelta = dashVelocity * float(deltaTime);
		int dashSteps = int(std::ceil(std::abs(dashDelta)));
		int dashStepDir = (dashDelta < 0.0f) ? -1 : 1;
		for (int step = 0; step < dashSteps; ++step)
		{
			posEnemyF.x += float(dashStepDir);
			glm::ivec2 dashPos(int(posEnemyF.x), int(posEnemyF.y));
			if (map->checkCollision(dashPos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), dashStepDir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x))
			{
				posEnemyF.x = float(dashPos.x);
				dashTimeLeftMs = 0;
				dashVelocity = 0.0f;
				blocked = true;
				break;
			}
		}
		posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
	}
	else
	{
		if (!wantClimbUp && !wantClimbDown && wantLeft)
		{
			facingLeft = true;
			sprite->setFlipHorizontal(true);
			posEnemyF.x -= config.moveSpeed;
			glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
			if (map->checkCollision(movePos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::LEFT, &movePos.x))
				blocked = true;
			posEnemyF.x = float(movePos.x);
			posEnemy = movePos;
		}
		else if (!wantClimbUp && !wantClimbDown && wantRight)
		{
			facingLeft = false;
			sprite->setFlipHorizontal(false);
			posEnemyF.x += config.moveSpeed;
			glm::ivec2 movePos(int(posEnemyF.x), int(posEnemyF.y));
			if (map->checkCollision(movePos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::RIGHT, &movePos.x))
				blocked = true;
			posEnemyF.x = float(movePos.x);
			posEnemy = movePos;
		}
	}

	ladderProbePos = glm::ivec2(int(posEnemyF.x) - 2, int(posEnemyF.y));
	onLadderNow = map->isOnLadder(ladderProbePos, ladderProbeSize);

	int ladderCol = myTX;
	if (onLadderNow || wantClimbUp || wantClimbDown)
	{
		int y0 = int(posEnemyF.y) / ts;
		int y1 = (int(posEnemyF.y) + config.hitboxHeight - 1) / ts;
		y0 = std::max(0, y0);
		y1 = std::min(map->getMapSize().y - 1, y1);
		for (int dc = 0; dc < 3; ++dc)
		{
			int cand = (dc == 0) ? myTX : (dc == 1 ? myTX - 1 : myTX + 1);
			if (cand < 0 || cand >= map->getMapSize().x) continue;
			for (int y = y0; y <= y1; ++y)
			{
				if (map->getTileTypeAtPos(glm::ivec2(cand * ts, y * ts)) == TileType::LADDER)
				{
					ladderCol = cand;
					dc = 3;
					break;
				}
			}
		}
	}

	bool climbingThisFrame = (wantClimbUp || wantClimbDown);
	if (climbingThisFrame)
	{
		const float climbSpeed = 120.0f * dt;
		posEnemyF.x = float(ladderCol * ts + (ts - config.hitboxWidth) / 2);
		posEnemyF.y += wantClimbUp ? -climbSpeed : climbSpeed;
		glm::ivec2 climbPos(int(posEnemyF.x), int(posEnemyF.y));
		map->checkCollision(climbPos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), wantClimbUp ? CollisionDir::UP : CollisionDir::DOWN, &climbPos.y, wantClimbDown);
		posEnemyF.y = float(climbPos.y);
		posEnemy = climbPos;
		onGround = false;
		bJumping = false;
		verticalVelocity = 0.0f;
		dashTimeLeftMs = 0;
		dashVelocity = 0.0f;
	}

	int holdGroundY = 0;
	glm::ivec2 holdProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
	bool groundedForHold = map->checkCollision(holdProbe, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::DOWN, &holdGroundY, false);
	bool holdLadder = onLadderNow && !groundedForHold && !climbingThisFrame && !wantJump && !wantDrop && !wantsLadderDetach && dropThroughTimerMs <= 0 && dropCommitTimerMs <= 0;
	if (holdLadder)
	{
		posEnemyF.x = float(ladderCol * ts + (ts - config.hitboxWidth) / 2);
		posEnemy = glm::ivec2(int(posEnemyF.x), int(posEnemyF.y));
		onGround = false;
		bJumping = false;
		verticalVelocity = 0.0f;
		dashTimeLeftMs = 0;
		dashVelocity = 0.0f;
	}

	if (wantDrop && onGround && onOneWaySupport && !onLadderNow)
	{
		dropCommitTimerMs = 300;
		dropThroughTimerMs = 300;
		posEnemyF.y += 1.0f;
		onGround = false;
		bJumping = false;
		verticalVelocity = std::max(verticalVelocity, 80.0f);
	}

	if ((wantJump || blocked) && !bJumping && (onGround || onLadderNow) && !wantClimbUp && !wantClimbDown)
	{
		bJumping = true;
		onGround = false;
		verticalVelocity = -config.jumpVelocity;
		onLadderNow = false;
	}

	if (!climbingThisFrame && !holdLadder)
	{
		glm::ivec2 groundProbe(int(posEnemyF.x), int(posEnemyF.y + 1.0f));
		int groundedY = 0;
		bool dropThrough = dropThroughTimerMs > 0;
		if (map->checkCollision(groundProbe, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::DOWN, &groundedY, dropThrough) && verticalVelocity >= 0.0f)
		{
			onGround = true;
			posEnemyF.y = float(groundedY);
			verticalVelocity = 0.0f;
			bJumping = false;
			posEnemy.y = groundedY;
		}
	}

	if (climbingThisFrame || holdLadder)
	{
		ladderAnimActive = true;
		if (sprite->animation() != config.climbAnim)
			sprite->changeAnimation(config.climbAnim);
	}
	else if (onGround)
	{
		if (sprite->animation() != config.runAnim)
			sprite->changeAnimation(config.runAnim);
	}
	else
	{
		if (verticalVelocity < 0.0f && sprite->animation() != config.jumpUpAnim)
			sprite->changeAnimation(config.jumpUpAnim);
		else if (verticalVelocity >= 0.0f && sprite->animation() != config.jumpFallAnim)
			sprite->changeAnimation(config.jumpFallAnim);
	}

	if (!climbingThisFrame && !holdLadder)
	{
		verticalVelocity += GRAVITY * dt;
		posEnemyF.y += verticalVelocity * dt;
		glm::ivec2 fallPos(int(posEnemyF.x), int(posEnemyF.y));
		if (verticalVelocity > 0.0f)
		{
			bool dropThrough = dropThroughTimerMs > 0;
			onGround = map->checkCollision(fallPos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::DOWN, &fallPos.y, dropThrough);
			if (onGround)
			{
				posEnemyF.y = float(fallPos.y);
				verticalVelocity = 0.0f;
				bJumping = false;
			}
		}
		else if (verticalVelocity < 0.0f)
		{
			if (map->checkCollision(fallPos, glm::ivec2(config.hitboxWidth, config.hitboxHeight), CollisionDir::UP, &fallPos.y))
			{
				posEnemyF.y = float(fallPos.y);
				verticalVelocity = 0.0f;
			}
			bJumping = true;
		}
		posEnemy = fallPos;
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

	sprite->setPosition(glm::vec2(float(tileMapDispl.x + posEnemy.x) - config.renderOffsetX, float(tileMapDispl.y + posEnemy.y) - config.renderOffsetY));
}