#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <queue>
#include <algorithm>
#include "Game.h"

void Game::addDoorLinkBidirectional(const std::string &fromMap, int fromDoorIndex, const std::string &toMap, int toDoorIndex)
{
	auto &fromList = doorGraph[fromMap];
	if (fromDoorIndex >= int(fromList.size()))
		fromList.resize(fromDoorIndex + 1);
	fromList[fromDoorIndex].targetMap = toMap;
	fromList[fromDoorIndex].targetDoorIndex = toDoorIndex;

	auto &toList = doorGraph[toMap];
	if (toDoorIndex >= int(toList.size()))
		toList.resize(toDoorIndex + 1);
	toList[toDoorIndex].targetMap = fromMap;
	toList[toDoorIndex].targetDoorIndex = fromDoorIndex;
}

void Game::configureRoomGraph()
{
	doorGraph.clear();

	// Graph edges: each door index in one room points to a destination room,
	// and this helper also writes the reverse edge on the destination side.
	addDoorLinkBidirectional("../levels/map_0_2.json", 0, "../levels/room_A.json", 0);
	addDoorLinkBidirectional("../levels/map_0_2.json", 1, "../levels/room_B.json", 0);
	addDoorLinkBidirectional("../levels/map_1_2.json", 0, "../levels/room_C.json", 0);
}

bool Game::getDoorLink(const std::string &fromMap, int doorIndex, DoorLink &outLink) const
{
	auto it = doorGraph.find(fromMap);
	if (it == doorGraph.end())
		return false;

	const std::vector<DoorLink> &destinations = it->second;
	if (doorIndex < 0 || doorIndex >= int(destinations.size()))
		return false;

	if (destinations[doorIndex].targetMap.empty())
		return false;

	outLink = destinations[doorIndex];
	return true;
}

std::string Game::getCurrentWorldMapName() const
{
	return "../levels/map_" + std::to_string(currentRoomX) + "_" + std::to_string(currentRoomY) + ".json";
}

int Game::getRoomTotalKeys(const std::string &mapName)
{
	auto it = roomTotalKeys.find(mapName);
	if (it != roomTotalKeys.end())
		return it->second;

	int total = TileMap::countKeysInLevelFile(mapName);
	roomTotalKeys[mapName] = total;
	return total;
}

void Game::registerRoomKeyTotal(const std::string &mapName, int totalKeys)
{
	roomTotalKeys[mapName] = totalKeys;
	if (roomCollectedKeys.find(mapName) == roomCollectedKeys.end())
		roomCollectedKeys[mapName] = 0;
}

int Game::getCollectedKeysForRoom(const std::string &mapName) const
{
	auto it = roomCollectedKeys.find(mapName);
	if (it == roomCollectedKeys.end())
		return 0;
	return it->second;
}

void Game::collectKeyInCurrentRoom()
{
	const std::string mapName = getCurrentMapName();
	int total = getRoomTotalKeys(mapName);
	int &collected = roomCollectedKeys[mapName];
	collected = std::min(collected + 1, total);
	keysCollected++;  // Also update the HUD counter
}

void Game::registerRoomHealSpawns(const std::string &mapName, int totalHeals)
{
	if (roomCollectedHeals.find(mapName) == roomCollectedHeals.end())
		roomCollectedHeals[mapName] = 0;
}

int Game::getCollectedHealsForRoom(const std::string &mapName) const
{
	auto it = roomCollectedHeals.find(mapName);
	if (it == roomCollectedHeals.end())
		return 0;
	return it->second;
}

void Game::collectHealInCurrentRoom()
{
	const std::string mapName = getCurrentMapName();
	roomCollectedHeals[mapName] = roomCollectedHeals[mapName] + 1;
}

void Game::registerRoomShieldSpawns(const std::string &mapName, int totalShields)
{
	if (roomCollectedShields.find(mapName) == roomCollectedShields.end())
		roomCollectedShields[mapName] = 0;
}

int Game::getCollectedShieldsForRoom(const std::string &mapName) const
{
	auto it = roomCollectedShields.find(mapName);
	if (it == roomCollectedShields.end())
		return 0;
	return it->second;
}

void Game::collectShieldInCurrentRoom()
{
	const std::string mapName = getCurrentMapName();
	roomCollectedShields[mapName] = roomCollectedShields[mapName] + 1;
}

void Game::registerRoomSwordSpawn(const std::string &mapName)
{
	if (roomCollectedSword.find(mapName) == roomCollectedSword.end())
		roomCollectedSword[mapName] = false;
}

bool Game::hasSwordBeenCollectedInRoom(const std::string &mapName) const
{
	auto it = roomCollectedSword.find(mapName);
	if (it == roomCollectedSword.end())
		return false;
	return it->second;
}

void Game::collectSwordInCurrentRoom()
{
	const std::string mapName = getCurrentMapName();
	roomCollectedSword[mapName] = true;
}

void Game::preloadConnectedRoomKeys()
{
	const std::string worldMap = getCurrentWorldMapName();
	std::queue<std::string> q;
	std::unordered_set<std::string> visited;

	q.push(worldMap);
	visited.insert(worldMap);

	while (!q.empty()) {
		const std::string room = q.front();
		q.pop();

		// Ensure this room's key count is registered
		getRoomTotalKeys(room);

		auto it = doorGraph.find(room);
		if (it == doorGraph.end())
			continue;

		for (const DoorLink &link : it->second) {
			if (link.targetMap.empty())
				continue;
			if (visited.insert(link.targetMap).second)
				q.push(link.targetMap);
		}
	}
}

int Game::getTotalKeysInCurrentWorld()
{
	const std::string worldMap = getCurrentWorldMapName();
	std::queue<std::string> q;
	std::unordered_set<std::string> visited;
	int totalKeys = 0;

	q.push(worldMap);
	visited.insert(worldMap);

	while (!q.empty()) {
		const std::string room = q.front();
		q.pop();

		totalKeys += getRoomTotalKeys(room);

		auto it = doorGraph.find(room);
		if (it == doorGraph.end())
			continue;

		for (const DoorLink &link : it->second) {
			if (link.targetMap.empty())
				continue;
			if (visited.insert(link.targetMap).second)
				q.push(link.targetMap);
		}
	}

	return totalKeys;
}

int Game::getCollectedKeysInCurrentWorld() const
{
	const std::string worldMap = getCurrentWorldMapName();
	std::queue<std::string> q;
	std::unordered_set<std::string> visited;
	int collectedKeys = 0;

	q.push(worldMap);
	visited.insert(worldMap);

	while (!q.empty()) {
		const std::string room = q.front();
		q.pop();

		collectedKeys += getCollectedKeysForRoom(room);

		auto it = doorGraph.find(room);
		if (it == doorGraph.end())
			continue;

		for (const DoorLink &link : it->second) {
			if (link.targetMap.empty())
				continue;
			if (visited.insert(link.targetMap).second)
				q.push(link.targetMap);
		}
	}

	return collectedKeys;
}

bool Game::canUsePortalsFromCurrentWorld()
{
	const std::string worldMap = getCurrentWorldMapName();
	std::queue<std::string> q;
	std::unordered_set<std::string> visited;

	q.push(worldMap);
	visited.insert(worldMap);

	while (!q.empty()) {
		const std::string room = q.front();
		q.pop();

		int total = getRoomTotalKeys(room);
		int collected = getCollectedKeysForRoom(room);
		if (collected < total)
			return false;

		auto it = doorGraph.find(room);
		if (it == doorGraph.end())
			continue;

		for (const DoorLink &link : it->second) {
			if (link.targetMap.empty())
				continue;
			if (visited.insert(link.targetMap).second)
				q.push(link.targetMap);
		}
	}

	return true;
}

void Game::setNextSpawnDoor(const std::string &mapName, int doorIndex)
{
	hasNextSpawnDoor = true;
	nextSpawnMap = mapName;
	nextSpawnDoorIndex = doorIndex;
}

bool Game::consumeNextSpawnDoor(const std::string &mapName, int &doorIndexOut)
{
	if (!hasNextSpawnDoor || nextSpawnMap != mapName || nextSpawnDoorIndex < 0)
		return false;

	doorIndexOut = nextSpawnDoorIndex;
	hasNextSpawnDoor = false;
	nextSpawnMap.clear();
	nextSpawnDoorIndex = -1;
	return true;
}

void Game::enterSideRoom(const std::string &roomMapName)
{
	inSideRoom = true;
	sideRoomMapName = roomMapName;
}

void Game::exitSideRoom()
{
	inSideRoom = false;
	sideRoomMapName.clear();
}

void Game::reloadScene()
{
	scene.init();
}


void Game::init()
{
	bPlay = true;
	for(int i = 0; i <= GLFW_KEY_LAST; ++i)
		keys[i] = false;
	jumpInputBlocked = false;
	hasNextSpawnDoor = false;
	nextSpawnMap.clear();
	nextSpawnDoorIndex = -1;
	roomTotalKeys.clear();
	roomCollectedKeys.clear();
	configureRoomGraph();
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	scene.init();
}

bool Game::update(int deltaTime)
{
	scene.update(deltaTime);

	return bPlay;
}

void Game::render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	scene.render();
}

void Game::keyPressed(int key)
{
	if(key == GLFW_KEY_ESCAPE) // Escape code
		bPlay = false;
	keys[key] = true;
}

void Game::keyReleased(int key)
{
	keys[key] = false;
}

void Game::mouseMove(int x, int y)
{
}

void Game::mousePress(int button)
{
}

void Game::mouseRelease(int button)
{
}

bool Game::getKey(int key) const
{
	return keys[key];
}



