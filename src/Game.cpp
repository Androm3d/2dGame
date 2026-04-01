#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <queue>
#include <algorithm>
#include "Game.h"
#include "AudioManager.h"

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
	addDoorLinkBidirectional("../levels/map_2_1.json", 0, "../levels/room_D.json", 0);
	addDoorLinkBidirectional("../levels/map_2_1.json", 1, "../levels/room_E.json", 0);
	addDoorLinkBidirectional("../levels/map_1_1.json", 0, "../levels/room_F.json", 0);
	addDoorLinkBidirectional("../levels/map_0_1.json", 0, "../levels/room_G.json", 0);
	addDoorLinkBidirectional("../levels/map_0_0.json", 0, "../levels/room_H.json", 0);
	addDoorLinkBidirectional("../levels/map_0_0.json", 1, "../levels/room_I.json", 0);
	addDoorLinkBidirectional("../levels/map_1_0.json", 0, "../levels/room_J.json", 0);
	addDoorLinkBidirectional("../levels/map_2_0.json", 2, "../levels/room_K.json", 0);
	addDoorLinkBidirectional("../levels/map_2_0.json", 1, "../levels/room_L.json", 0);
	addDoorLinkBidirectional("../levels/map_2_0.json", 0, "../levels/room_M.json", 0);
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

void Game::grantAllKeysInCurrentWorld()
{
	const std::string worldMap = getCurrentWorldMapName();
	std::queue<std::string> q;
	std::unordered_set<std::string> visited;

	q.push(worldMap);
	visited.insert(worldMap);

	while (!q.empty()) {
		const std::string room = q.front();
		q.pop();

		roomCollectedKeys[room] = getRoomTotalKeys(room);

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

	keysCollected = getTotalKeysInCurrentWorld();
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
	hasNextSpawnPortal = false;
	nextSpawnPortalMap.clear();
	nextSpawnPortalSide = PortalEntrySide::NONE;
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

void Game::setNextSpawnPortal(const std::string &mapName, PortalEntrySide side)
{
	hasNextSpawnPortal = (side != PortalEntrySide::NONE);
	nextSpawnPortalMap = mapName;
	nextSpawnPortalSide = side;
	hasNextSpawnDoor = false;
	nextSpawnMap.clear();
	nextSpawnDoorIndex = -1;
}

bool Game::consumeNextSpawnPortal(const std::string &mapName, PortalEntrySide &sideOut)
{
	if (!hasNextSpawnPortal || nextSpawnPortalMap != mapName || nextSpawnPortalSide == PortalEntrySide::NONE)
		return false;

	sideOut = nextSpawnPortalSide;
	hasNextSpawnPortal = false;
	nextSpawnPortalMap.clear();
	nextSpawnPortalSide = PortalEntrySide::NONE;
	return true;
}

void Game::saveRoomRuntimeState(const std::string &mapName, const RoomRuntimeState &state)
{
	roomRuntimeStates[mapName] = state;
}

bool Game::loadRoomRuntimeState(const std::string &mapName, RoomRuntimeState &stateOut) const
{
	auto it = roomRuntimeStates.find(mapName);
	if (it == roomRuntimeStates.end())
		return false;
	stateOut = it->second;
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
	hasNextSpawnPortal = false;
	nextSpawnPortalMap.clear();
	nextSpawnPortalSide = PortalEntrySide::NONE;
	roomTotalKeys.clear();
	roomCollectedKeys.clear();
	roomRuntimeStates.clear();
	configureRoomGraph();
	AudioManager::instance().init();
	glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
	currentState = GameState::MENU;
	canResumePlay = false;
	menuOpenedFromDeath = false;
	menuSelection = 0;
	godMode = false;
	scene.init();
	AudioManager::instance().playMenuMusic(true);
}

void Game::resetGameState()
{
	lives = 3;
	keysCollected = 0;
	totalKeysInLevel = 0;
	hasSword = false;
	hasShield = false;
	currentRoomX = 0;
	currentRoomY = 2;
	inSideRoom = false;
	sideRoomMapName.clear();
	jumpInputBlocked = false;
	hasNextSpawnDoor = false;
	nextSpawnMap.clear();
	nextSpawnDoorIndex = -1;
	hasNextSpawnPortal = false;
	nextSpawnPortalMap.clear();
	nextSpawnPortalSide = PortalEntrySide::NONE;
	roomTotalKeys.clear();
	roomCollectedKeys.clear();
	roomCollectedHeals.clear();
	roomCollectedShields.clear();
	roomCollectedSword.clear();
	roomRuntimeStates.clear();
	godMode = false;
	scene.init();
	canResumePlay = true;
	menuOpenedFromDeath = false;
}

void Game::transitionToState(GameState newState)
{
	GameState oldState = currentState;
	currentState = newState;

	if (newState == GameState::PLAY) {
		if (!canResumePlay || menuOpenedFromDeath)
			resetGameState();
		AudioManager::instance().playGameMusic(menuOpenedFromDeath || !canResumePlay);
		menuOpenedFromDeath = false;
	}
	else if (newState == GameState::MENU) {
		bool restartMenu = (oldState == GameState::PLAY);
		AudioManager::instance().playMenuMusic(restartMenu);
	}
}


bool Game::update(int deltaTime)
{
	// Clamp frame spikes (e.g., app resumed from background) to avoid physics tunneling.
	if (deltaTime < 0) deltaTime = 0;
	if (deltaTime > 50) deltaTime = 50;

	if (currentState == GameState::PLAY) {
		scene.update(deltaTime);
		
		if (lives <= 0) {
			menuOpenedFromDeath = true;
			canResumePlay = false;
			transitionToState(GameState::MENU);
		}
	}

	return bPlay;
}

void Game::render()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	if (currentState == GameState::MENU) {
		scene.renderMenuScreen(menuSelection);
	} else if (currentState == GameState::INSTRUCTIONS) {
		scene.renderInstructionsScreen();
	} else if (currentState == GameState::CREDITS) {
		scene.renderCreditsScreen();
	} else if (currentState == GameState::GAME_WIN) {
		scene.renderWinScreen();
	} else if (currentState == GameState::PLAY) {
		scene.render();
	}
}


void Game::keyPressed(int key)
{
	if(key == GLFW_KEY_ESCAPE) {
		if (currentState == GameState::PLAY) {
			menuOpenedFromDeath = false;
			canResumePlay = true;
			transitionToState(GameState::MENU);
		} else if (currentState == GameState::INSTRUCTIONS || currentState == GameState::CREDITS || currentState == GameState::GAME_WIN) {
			transitionToState(GameState::MENU);
		} else {
			AudioManager::instance().shutdown();
			bPlay = false;
		}
	}
	
	if (currentState == GameState::MENU) {
		if (key == GLFW_KEY_UP) {
			menuSelection = (menuSelection - 1 + numMenuOptions) % numMenuOptions;
		} else if (key == GLFW_KEY_DOWN) {
			menuSelection = (menuSelection + 1) % numMenuOptions;
		} else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
			if (menuSelection == 0) {
				transitionToState(GameState::PLAY);
			} else if (menuSelection == 1) {
				transitionToState(GameState::INSTRUCTIONS);
			} else if (menuSelection == 2) {
				transitionToState(GameState::CREDITS);
			}
		}
	}
	
	if (currentState == GameState::PLAY) {
		if (key == GLFW_KEY_G) {
			godMode = !godMode;
		}
		if (key == GLFW_KEY_K) {
			grantAllKeysInCurrentWorld();
		}
		if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
			int level = key - GLFW_KEY_1 + 1;
			switch (level) {
				case 1: currentRoomX = 0; currentRoomY = 2; break;
				case 2: currentRoomX = 1; currentRoomY = 2; break;
				case 3: currentRoomX = 2; currentRoomY = 2; break;
				case 4: currentRoomX = 2; currentRoomY = 1; break;
				case 5: currentRoomX = 1; currentRoomY = 1; break;
				case 6: currentRoomX = 0; currentRoomY = 1; break;
				case 7: currentRoomX = 0; currentRoomY = 0; break;
				case 8: currentRoomX = 1; currentRoomY = 0; break;
				case 9: currentRoomX = 2; currentRoomY = 0; break;
			}
			inSideRoom = false;
			sideRoomMapName.clear();
			scene.init();
		}
	}
	
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



