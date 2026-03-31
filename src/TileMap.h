#ifndef _TILE_MAP_INCLUDE
#define _TILE_MAP_INCLUDE


#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include "Texture.h"
#include "ShaderProgram.h"


// Class Tilemap is capable of loading a tile map from a text file in a very
// simple format (see level01.txt for an example). With this information
// it builds a single VBO that contains all tiles. As a result the render
// method draws the whole map independently of what is visible.
enum class TileType {
	EMPTY,
	SOLID,
	ONE_WAY_PLATFORM,
	LADDER,
	DOOR,
	PORTAL,
	KEY,
	SWORD,
	HEAL,
	SHIELD, 
	WEIGHT,
	SPAWN,
	SPRING,
	DASH,
	ENEMY_SPAWN_1,
	ENEMY_SPAWN_2,
	ENEMY_SPAWN_3
};

enum class CollisionDir { LEFT, RIGHT, UP, DOWN };

class TileMap
{

private:
	TileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program);

public:
	// Tile maps can only be created inside an OpenGL context
	static TileMap *createTileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program);
	static int countKeysInLevelFile(const std::string &levelFile);

	~TileMap();

	void render() const;
	void free();
	
	int getTileSize() const { return tileSize; }
	glm::ivec2 getMapSize() const { return mapSize; }
	glm::ivec2 getRoomSize() const { return roomSize; }

	bool checkCollision(const glm::ivec2 &pos, const glm::ivec2 &size, CollisionDir dir, int *correctedPos = nullptr, bool dropThrough = false) const;
	bool isOnLadder(const glm::ivec2 &pos, const glm::ivec2 &size) const;
	bool isOnSpring(const glm::ivec2 &pos, const glm::ivec2 &size) const;
	bool isOnDash(const glm::ivec2 &pos, const glm::ivec2 &size) const;
	TileType getTileType(const int tileId) const;
	TileType getTileTypeAtPos(const glm::ivec2 &pos) const;


	const std::vector<glm::ivec2>& getDoorSpawns() const { return doorSpawnLocations; }
	const std::vector<glm::ivec2>& getKeySpawns() const { return keySpawnLocations; }
	const std::vector<glm::ivec2>& getHealSpawns() const { return healSpawnLocations; }
	const std::vector<glm::ivec2>& getShieldSpawns() const { return shieldSpawnLocations; }
	const std::vector<glm::ivec2>& getWeightSpawns() const { return weightSpawnLocations; }
	const std::vector<glm::ivec2>& getSwordSpawns() const { return swordSpawnLocations; }
	const std::vector<glm::ivec2>& getPortals() const { return portalSpawnLocations; }
	const std::vector<glm::ivec2>& getDoors() const { return doorSpawnLocations; }
	const std::vector<glm::ivec2>& getSpawnLocations() const { return spawnLocations; }

private:
	bool loadLevelJSON(const std::string &levelFile);
	bool loadLevel(const string &levelFile);
	void prepareArrays(const glm::vec2 &minCoords, ShaderProgram &program);

private:
	GLuint vao;
	GLuint vbo;
	GLint posLocation, texCoordLocation;
	int nTiles;
	glm::ivec2 position, mapSize, tilesheetSize, roomSize;
	int tileSize, blockSize;
	Texture tilesheet;
	glm::vec2 tileTexSize;
	int *map;
	std::unordered_map<int, TileType> tileDictionary;
	std::vector<glm::ivec2> doorSpawnLocations;
	std::vector<glm::ivec2> keySpawnLocations;
	std::vector<glm::ivec2> healSpawnLocations;
	std::vector<glm::ivec2> shieldSpawnLocations;
	std::vector<glm::ivec2> weightSpawnLocations;
	std::vector<glm::ivec2> swordSpawnLocations;
	std::vector<glm::ivec2> portalSpawnLocations;
	std::vector<glm::ivec2> spawnLocations;
};


#endif // _TILE_MAP_INCLUDE


