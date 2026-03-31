#ifndef _GAME_INCLUDE
#define _GAME_INCLUDE


#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <GLFW/glfw3.h>
#include "Scene.h"


#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 480


// Game is a singleton (a class with a single instance) that represents our whole application

enum class GameState { MENU, PLAY, INSTRUCTIONS, CREDITS };

class Game
{

public:
	struct DoorLink {
		std::string targetMap;
		int targetDoorIndex = -1;
	};

private:
	using DoorGraph = std::unordered_map<std::string, std::vector<DoorLink>>;

	Game() {}
	void configureRoomGraph();
	void addDoorLinkBidirectional(const std::string &fromMap, int fromDoorIndex, const std::string &toMap, int toDoorIndex);
	std::string getCurrentWorldMapName() const;
	int getRoomTotalKeys(const std::string &mapName);
	
public:
    GameState currentState = GameState::MENU;
    
    int currentLevel = 1;
    int lives = 3;        
    int keysCollected = 0; 
    int totalKeysInLevel = 0; 

    bool hasSword = false;
    bool hasShield = false;
	bool godMode = false;

	int currentRoomX = 0;
	int currentRoomY = 2;

	bool inSideRoom = false;
	std::string sideRoomMapName;

	std::string getCurrentMapName() {
		if (inSideRoom && !sideRoomMapName.empty())
			return sideRoomMapName;
		return "../levels/map_" + std::to_string(currentRoomX) + "_" + std::to_string(currentRoomY) + ".json";
    }
	void enterSideRoom(const std::string &roomMapName);
	void exitSideRoom();
	void reloadScene();
	bool getDoorLink(const std::string &fromMap, int doorIndex, DoorLink &outLink) const;

	void registerRoomKeyTotal(const std::string &mapName, int totalKeys);
	int getCollectedKeysForRoom(const std::string &mapName) const;
	void collectKeyInCurrentRoom();
	void registerRoomHealSpawns(const std::string &mapName, int totalHeals);
	int getCollectedHealsForRoom(const std::string &mapName) const;
	void collectHealInCurrentRoom();
	void registerRoomShieldSpawns(const std::string &mapName, int totalShields);
	int getCollectedShieldsForRoom(const std::string &mapName) const;
	void collectShieldInCurrentRoom();
	void registerRoomSwordSpawn(const std::string &mapName);
	bool hasSwordBeenCollectedInRoom(const std::string &mapName) const;
	void collectSwordInCurrentRoom();
	void preloadConnectedRoomKeys();
	int getTotalKeysInCurrentWorld();
	int getCollectedKeysInCurrentWorld() const;
	bool canUsePortalsFromCurrentWorld();

	void setJumpInputBlocked(bool blocked) { jumpInputBlocked = blocked; }
	bool isJumpInputBlocked() const { return jumpInputBlocked; }

	void setNextSpawnDoor(const std::string &mapName, int doorIndex);
	bool consumeNextSpawnDoor(const std::string &mapName, int &doorIndexOut);

    // Helper methods for the Scene to call
    void addKey() { keysCollected++; }
    bool isLevelCleared() { return keysCollected >= totalKeysInLevel; }
	void loseLife(); // You can implement this to reset the room/go to menu later
	static Game &instance()
	{
		static Game G;
	
		return G;
	}
	
	void init();
	bool update(int deltaTime);
	void render();

	void resetGameState();
	void transitionToState(GameState newState);
	
	// Input callback methods
	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMove(int x, int y);
	void mousePress(int button);
	void mouseRelease(int button);

	bool getKey(int key) const;

private:
	bool bPlay; // Continue to play game?
	bool keys[GLFW_KEY_LAST+1]; // Store key states so that 
							    // we can have access at any time
	bool jumpInputBlocked = false;
	bool hasNextSpawnDoor = false;
	std::string nextSpawnMap;
	int nextSpawnDoorIndex = -1;
	std::unordered_map<std::string, int> roomTotalKeys;
	std::unordered_map<std::string, int> roomCollectedKeys;
	std::unordered_map<std::string, int> roomCollectedHeals;
	std::unordered_map<std::string, int> roomCollectedShields;
	std::unordered_map<std::string, bool> roomCollectedSword;
	DoorGraph doorGraph;
	Scene scene;

	int menuSelection = 0;
	int numMenuOptions = 3;

};


#endif // _GAME_INCLUDE

