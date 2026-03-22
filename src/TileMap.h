#ifndef _TILE_MAP_INCLUDE
#define _TILE_MAP_INCLUDE


#include <glm/glm.hpp>
#include <unordered_map>
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
	STAIR,
	DOOR,
	HAZARD
};

enum class CollisionDir { LEFT, RIGHT, UP, DOWN };

class TileMap
{

private:
	TileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program);

public:
	// Tile maps can only be created inside an OpenGL context
	static TileMap *createTileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program);

	~TileMap();

	void render() const;
	void free();
	
	int getTileSize() const { return tileSize; }

    bool checkCollision(const glm::ivec2 &pos, const glm::ivec2 &size, CollisionDir dir, int *correctedPos) const;
	TileType getTileType(const int tileId) const;
	
private:
	bool loadLevelJSON(const std::string &levelFile);
	bool loadLevel(const string &levelFile);
	void prepareArrays(const glm::vec2 &minCoords, ShaderProgram &program);
	void setupTileDictionary();

private:
	GLuint vao;
	GLuint vbo;
	GLint posLocation, texCoordLocation;
	int nTiles;
	glm::ivec2 position, mapSize, tilesheetSize;
	int tileSize, blockSize;
	Texture tilesheet;
	glm::vec2 tileTexSize;
	int *map;
	std::unordered_map<int, TileType> tileDictionary;

};


#endif // _TILE_MAP_INCLUDE


