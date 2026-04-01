#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include "TileMap.h"
#include "external/json/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

namespace {
bool openWithFallback(ifstream &fin, const string &path, string &resolvedPath)
{
	fin.clear();
	fin.open(path.c_str());
	if(fin.is_open())
	{
		resolvedPath = path;
		return true;
	}

	const string fallback = "" + path;
	fin.clear();
	fin.open(fallback.c_str());
	if(fin.is_open())
	{
		resolvedPath = fallback;
		return true;
	}

	return false;
}
}

TileMap *TileMap::createTileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program)
{
	TileMap *map = new TileMap(levelFile, minCoords, program);
	return map;
}

int TileMap::countKeysInLevelFile(const std::string &levelFile)
{
	if (levelFile.length() < 5 || levelFile.substr(levelFile.length() - 5) != ".json")
		return 0;

	ifstream fin;
	string resolvedPath;
	if (!openWithFallback(fin, levelFile, resolvedPath)) {
		return 0;
	}

	json j;
	fin >> j;

	std::unordered_set<int> keyTileIds;
	for (const auto &tileset : j["tilesets"]) {
		int firstgid = tileset.value("firstgid", 1);
		if (!tileset.contains("tiles"))
			continue;

		for (const auto &tile : tileset["tiles"]) {
			if (!tile.contains("properties"))
				continue;

			for (const auto &prop : tile["properties"]) {
				if (prop.value("name", "") == "type" && prop.value("value", "") == "KEY") {
					int localId = tile.value("id", -1);
					if (localId >= 0)
						keyTileIds.insert(firstgid + localId);
				}
			}
		}
	}

	int keyCount = 0;
	if (j.contains("layers") && !j["layers"].empty() && j["layers"][0].contains("data")) {
		for (const auto &raw : j["layers"][0]["data"]) {
			int tileId = int(raw) & 0x1FFFFFFF;
			if (keyTileIds.find(tileId) != keyTileIds.end())
				keyCount++;
		}
	}

	return keyCount;
}


TileMap::TileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program)
{
	vao = 0;
	vbo = 0;
	posLocation = -1;
	texCoordLocation = -1;
	nTiles = 0;
	map = NULL;
	mapSize = glm::ivec2(0);
	tilesheetSize = glm::ivec2(1);
	tileSize = 1;
	blockSize = 1;
	tileTexSize = glm::vec2(1.f, 1.f);

	bool loaded = false;

    if (levelFile.length() >= 5 && levelFile.substr(levelFile.length() - 5) == ".json") {
        loaded = loadLevelJSON(levelFile);
    } else {
        loaded = loadLevel(levelFile);
    }

	if(loaded)
		prepareArrays(minCoords, program);
}

TileMap::~TileMap()
{
	if(map != NULL)
		delete [] map;
	free();
}

TileType TileMap::getTileType(const int tileId) const
{
	// Look up the tile ID in our dictionary
	auto it = tileDictionary.find(tileId);
	if (it != tileDictionary.end()) {
		return it->second; // Return the type (SOLID, STAIR, etc.)
	}

	return TileType::EMPTY;
}

TileType TileMap::getTileTypeAtPos(const glm::ivec2 &pos) const
{
	int x = pos.x / tileSize;
	int y = pos.y / tileSize;
	
	// Out of bounds check
	if (x < 0 || x >= mapSize.x || y < 0 || y >= mapSize.y) return TileType::EMPTY;
	
	return getTileType(map[y * mapSize.x + x]);
}


void TileMap::render() const
{
	if(nTiles <= 0 || vao == 0)
		return;

	glEnable(GL_TEXTURE_2D);
	tilesheet.use();
	glBindVertexArray(vao);
	glEnableVertexAttribArray(posLocation);
	glEnableVertexAttribArray(texCoordLocation);
	glDrawArrays(GL_TRIANGLES, 0, 6 * nTiles);
	glDisable(GL_TEXTURE_2D);
}

void TileMap::free()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
}

bool TileMap::loadLevelJSON(const string &levelFile)
{
	ifstream fin;
	string resolvedPath;
	if(!openWithFallback(fin, levelFile, resolvedPath)) {
        return false;
    }

    json j;
    fin >> j;
    fin.close();

    mapSize.x = j["width"];
    mapSize.y = j["height"];
    tileSize = j["tilewidth"];
    blockSize = tileSize;

    // Room size fallback (default view layout)
    roomSize = glm::ivec2(20, 10);
    if (j.contains("properties")) {
        for (const auto& prop : j["properties"]) {
            if (prop["name"] == "roomWidth") {
                roomSize.x = prop["value"];
            } else if (prop["name"] == "roomHeight") {
                roomSize.y = prop["value"];
            }
        }
    }

    // --- SAFELY READ TILESET INFO ---
    string rawImagePath;
    int imgWidth, imgHeight;

    if (j["tilesets"][0].contains("image")) {
        // The tileset is embedded properly
        rawImagePath = j["tilesets"][0]["image"];
        imgWidth = j["tilesets"][0]["imagewidth"];
        imgHeight = j["tilesets"][0]["imageheight"];
    } else {
        // The tileset is an external .tsx file
        rawImagePath = "tiles.png";
        // If your tile is 32x32, and the image is 10 tiles wide / 10 tiles high:
        imgWidth = 320;  
        imgHeight = 320; 
    }

    // Robust Image Path Handling
    size_t lastSlash = rawImagePath.find_last_of("/\\");
    string filename = (lastSlash == string::npos) ? rawImagePath : rawImagePath.substr(lastSlash + 1);
    string tilesheetFile = "images/" + filename;
    
    tilesheetSize.x = imgWidth / tileSize;
    tilesheetSize.y = imgHeight / tileSize;

    tilesheet.loadFromFile(tilesheetFile, TEXTURE_PIXEL_FORMAT_RGBA);
    tilesheet.setWrapS(GL_CLAMP_TO_EDGE);
    tilesheet.setWrapT(GL_CLAMP_TO_EDGE);
    tilesheet.setMinFilter(GL_NEAREST);
    tilesheet.setMagFilter(GL_NEAREST);
    
    tileTexSize = glm::vec2(1.f / tilesheetSize.x, 1.f / tilesheetSize.y);
    
    // limpiar datos anteriores
    tileDictionary.clear();
    doorSpawnLocations.clear();
    keySpawnLocations.clear();
    portalSpawnLocations.clear();
	healSpawnLocations.clear();
	shieldSpawnLocations.clear();
	weightSpawnLocations.clear();
	swordSpawnLocations.clear();
	spawnLocations.clear();
	springSpawnLocations.clear();
	dashSpawnLocations.clear();
	dashSpawnFacingLeft.clear();
	enemy1SpawnLocations.clear();
	enemy2SpawnLocations.clear();
	enemy3SpawnLocations.clear();

    // leer propiedades del tileset
    // (Changed jsonDocument to j)
    for (const auto& tileset : j["tilesets"]) {
        int firstgid = tileset["firstgid"];

			if (tileset.contains("tiles")) {
				for (const auto& tile : tileset["tiles"]) {
					int localId = tile["id"];
					int globalId = localId + firstgid;

					std::string typeVal;
					if (tile.contains("properties")) {
						for (const auto& prop : tile["properties"]) {
							if (prop["name"] == "type")
								typeVal = prop["value"];
						}
					}

					if (!typeVal.empty()) {
						if (typeVal == "SOLID") tileDictionary[globalId] = TileType::SOLID;
						else if (typeVal == "ONE_WAY_PLATFORM") tileDictionary[globalId] = TileType::ONE_WAY_PLATFORM;
						else if (typeVal == "LADDER") tileDictionary[globalId] = TileType::LADDER;
						else if (typeVal == "DOOR") tileDictionary[globalId] = TileType::DOOR;
						else if (typeVal == "PORTAL") tileDictionary[globalId] = TileType::PORTAL;
						else if (typeVal == "KEY") tileDictionary[globalId] = TileType::KEY;
						else if (typeVal == "SWORD") tileDictionary[globalId] = TileType::SWORD;
						else if (typeVal == "HEAL") tileDictionary[globalId] = TileType::HEAL;
						else if (typeVal == "SHIELD") tileDictionary[globalId] = TileType::SHIELD;
						else if (typeVal == "WEIGHT") tileDictionary[globalId] = TileType::WEIGHT;
						else if (typeVal == "SPAWN") tileDictionary[globalId] = TileType::SPAWN;
						else if (typeVal == "SPRING") tileDictionary[globalId] = TileType::SPRING;
						else if (typeVal == "DASH_LEFT") tileDictionary[globalId] = TileType::DASH_LEFT;
						else if (typeVal == "DASH_RIGHT") tileDictionary[globalId] = TileType::DASH_RIGHT;
						else if (typeVal == "SPAWN_ENEMY_1") tileDictionary[globalId] = TileType::ENEMY_SPAWN_1;
						else if (typeVal == "SPAWN_ENEMY_2") tileDictionary[globalId] = TileType::ENEMY_SPAWN_2;
						else if (typeVal == "SPAWN_ENEMY_3") tileDictionary[globalId] = TileType::ENEMY_SPAWN_3;
					}
				}
			}
    }

    // leer datos del mapa y colocar tiles/spawns
    map = new int[mapSize.x * mapSize.y];
    auto data = j["layers"][0]["data"];
    
    for(int i = 0; i < mapSize.x * mapSize.y; i++)
    {
        unsigned int raw_tile = data[i];
        unsigned int tile_id = raw_tile & 0x1FFFFFFF; // Mask out flip flags

        // Calculate the actual pixel coordinates for this specific tile
        int x = (i % mapSize.x) * tileSize;
        int y = (i / mapSize.x) * tileSize;

        // Check our dictionary to see if this tile is an Entity
        auto it = tileDictionary.find(tile_id);
        if (it != tileDictionary.end()) {
            
            if (it->second == TileType::DOOR) {
                doorSpawnLocations.push_back(glm::ivec2(x, y));
                tile_id = 0; // borrar tile para renderizar el sprite por encima
				
				// borrar parte de arriba de la puerta
				if (i >= mapSize.x) {
					map[i - mapSize.x] = 0; 
				}
            }
			else if (it->second == TileType::PORTAL) {
				portalSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;

				if (i >= mapSize.x) {
					map[i - mapSize.x] = 0; 
				}
			}
            else if (it->second == TileType::KEY) {
                keySpawnLocations.push_back(glm::ivec2(x, y));
                tile_id = 0; // Turn it into air!
            }
            else if (it->second == TileType::SWORD) {
				swordSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0; 
            }
            else if (it->second == TileType::HEAL) {
				healSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
            }
            else if (it->second == TileType::SHIELD) {
				shieldSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
            }
            else if (it->second == TileType::WEIGHT) {
				weightSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
            }
            else if (it->second == TileType::SPAWN) {
				spawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
            }
			else if (it->second == TileType::SPRING) {
				springSpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
			}
			else if (it->second == TileType::DASH_LEFT || it->second == TileType::DASH_RIGHT) {
				dashSpawnLocations.push_back(glm::ivec2(x, y));
				dashSpawnFacingLeft.push_back(it->second == TileType::DASH_LEFT);
				tile_id = 0;
			}
			else if (it->second == TileType::ENEMY_SPAWN_1) {
				enemy1SpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
			}
			else if (it->second == TileType::ENEMY_SPAWN_2) {
				enemy2SpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
			}
			else if (it->second == TileType::ENEMY_SPAWN_3) {
				enemy3SpawnLocations.push_back(glm::ivec2(x, y));
				tile_id = 0;
			}

        }

        // Save the tile to the visual map (Entities are now saved as 0)
        map[i] = tile_id;
    }

    return true;
}

bool TileMap::loadLevel(const string &levelFile)
{
	ifstream fin;
	string line, tilesheetFile;
	stringstream sstream;
	char tile;
	string resolvedPath;
	
	if(!openWithFallback(fin, levelFile, resolvedPath))
		return false;
	getline(fin, line);
	if(line.compare(0, 7, "TILEMAP") != 0)
		return false;
	getline(fin, line);
	sstream.str(line);
	sstream >> mapSize.x >> mapSize.y;
	getline(fin, line);
	sstream.str(line);
	sstream >> tileSize >> blockSize;
	getline(fin, line);
	sstream.str(line);
	sstream >> tilesheetFile;
	tilesheet.loadFromFile(tilesheetFile, TEXTURE_PIXEL_FORMAT_RGBA);
	tilesheet.setWrapS(GL_CLAMP_TO_EDGE);
	tilesheet.setWrapT(GL_CLAMP_TO_EDGE);
	tilesheet.setMinFilter(GL_NEAREST);
	tilesheet.setMagFilter(GL_NEAREST);
	getline(fin, line);
	sstream.str(line);
	sstream >> tilesheetSize.x >> tilesheetSize.y;
	tileTexSize = glm::vec2(1.f / tilesheetSize.x, 1.f / tilesheetSize.y);
	
	map = new int[mapSize.x * mapSize.y];
	for(int j=0; j<mapSize.y; j++)
	{
		for(int i=0; i<mapSize.x; i++)
		{
			fin.get(tile);
			if(tile == ' ')
				map[j*mapSize.x+i] = 0;
			else
				map[j*mapSize.x+i] = tile - int('0');
		}
		fin.get(tile);
#ifndef _WIN32
		fin.get(tile);
#endif
	}
	fin.close();
	
	return true;
}

void TileMap::prepareArrays(const glm::vec2 &minCoords, ShaderProgram &program)
{
	if(map == NULL || mapSize.x <= 0 || mapSize.y <= 0)
		return;

	int tile;
	glm::vec2 posTile, texCoordTile[2], halfTexel;
	vector<float> vertices;
	
	nTiles = 0;
	halfTexel = glm::vec2(0.5f / tilesheet.width(), 0.5f / tilesheet.height());
	for(int j=0; j<mapSize.y; j++)
	{
		for(int i=0; i<mapSize.x; i++)
		{
			tile = map[j * mapSize.x + i];
			if(tile != 0)
			{
				// Non-empty tile
				nTiles++;
				posTile = glm::vec2(minCoords.x + i * tileSize, minCoords.y + j * tileSize);
				texCoordTile[0] = glm::vec2(float((tile-1)%tilesheetSize.x) / tilesheetSize.x, float((tile-1)/tilesheetSize.x) / tilesheetSize.y);
				texCoordTile[1] = texCoordTile[0] + tileTexSize;
				//texCoordTile[0] += halfTexel;
				texCoordTile[1] -= halfTexel;
				// First triangle
				vertices.push_back(posTile.x); vertices.push_back(posTile.y);
				vertices.push_back(texCoordTile[0].x); vertices.push_back(texCoordTile[0].y);
				vertices.push_back(posTile.x + blockSize); vertices.push_back(posTile.y);
				vertices.push_back(texCoordTile[1].x); vertices.push_back(texCoordTile[0].y);
				vertices.push_back(posTile.x + blockSize); vertices.push_back(posTile.y + blockSize);
				vertices.push_back(texCoordTile[1].x); vertices.push_back(texCoordTile[1].y);
				// Second triangle
				vertices.push_back(posTile.x); vertices.push_back(posTile.y);
				vertices.push_back(texCoordTile[0].x); vertices.push_back(texCoordTile[0].y);
				vertices.push_back(posTile.x + blockSize); vertices.push_back(posTile.y + blockSize);
				vertices.push_back(texCoordTile[1].x); vertices.push_back(texCoordTile[1].y);
				vertices.push_back(posTile.x); vertices.push_back(posTile.y + blockSize);
				vertices.push_back(texCoordTile[0].x); vertices.push_back(texCoordTile[1].y);
			}
		}
	}

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 24 * nTiles * sizeof(float), &vertices[0], GL_STATIC_DRAW);
	posLocation = program.bindVertexAttribute("position", 2, 4*sizeof(float), 0);
	texCoordLocation = program.bindVertexAttribute("texCoord", 2, 4*sizeof(float), (void *)(2*sizeof(float)));
}

// Collision tests for axis aligned bounding boxes.
// Method collisionMoveDown also corrects Y coordinate if the box is
// already intersecting a tile below.
bool TileMap::checkCollision(const glm::ivec2 &pos, const glm::ivec2 &size, CollisionDir dir, int *correctedPos, bool dropThrough) const
{
	int x0, x1, y0, y1;
	const int sideShave = 2;
	
	x0 = pos.x / tileSize;
	x1 = (pos.x + size.x - 1) / tileSize;
	y0 = pos.y / tileSize;
	y1 = (pos.y + size.y - 1) / tileSize;

	// C++ integer division truncates towards zero!
	if (pos.x < 0) x0 = (pos.x - tileSize + 1) / tileSize;
	if (pos.y < 0) y0 = (pos.y - tileSize + 1) / tileSize;

	// Treat going out of map bounds as hitting a solid wall
	if (y0 >= mapSize.y) return false; // Let fall completely out of bottom bounds
	if (y1 >= mapSize.y && dir == CollisionDir::DOWN) return false; // Let fall out of bottom bounds

	if (x0 < 0) {
		if (dir == CollisionDir::LEFT) { if (correctedPos) *correctedPos = 0; return true; }
		x0 = 0;
	}
	if (x1 >= mapSize.x) {
		if (dir == CollisionDir::RIGHT) { if (correctedPos) *correctedPos = (mapSize.x * tileSize) - size.x; return true; }
		x1 = mapSize.x - 1;
	}
	if (y0 < 0) {
		if (dir == CollisionDir::UP) return false;
		y0 = 0;
	}
	if (y1 >= mapSize.y) {
		y1 = mapSize.y - 1;
	}

	switch (dir) {
		case CollisionDir::LEFT:
			// Shave a couple pixels top/bottom to avoid snagging on tile seams while moving sideways.
			y0 = (pos.y + sideShave) / tileSize;
			y1 = (pos.y + size.y - 1 - sideShave) / tileSize;
			
			for (int y = y0; y <= y1; y++) {
				if (getTileType(map[y * mapSize.x + x0]) == TileType::SOLID) {
					if (correctedPos) *correctedPos = (x0 + 1) * tileSize;
					return true;
				}
			}
			break;

		case CollisionDir::RIGHT:
			y0 = (pos.y + sideShave) / tileSize;
			y1 = (pos.y + size.y - 1 - sideShave) / tileSize;
			
			for (int y = y0; y <= y1; y++) {
				if (getTileType(map[y * mapSize.x + x1]) == TileType::SOLID) {
					if (correctedPos) *correctedPos = x1 * tileSize - size.x;
					return true;
				}
			}
			break;

		case CollisionDir::DOWN:
			for (int x = x0; x <= x1; x++) {
				TileType type = getTileType(map[y1 * mapSize.x + x]);

				if (type == TileType::SOLID) {
					if (correctedPos) *correctedPos = y1 * tileSize - size.y;
					return true;
				}
				else if (type == TileType::ONE_WAY_PLATFORM && !dropThrough) {
					int tileTop = y1 * tileSize;
					if (correctedPos) *correctedPos = tileTop - size.y;
					return true;
				}
			}
			break;

		case CollisionDir::UP:
			for (int x = x0; x <= x1; x++) {
				if (getTileType(map[y0 * mapSize.x + x]) == TileType::SOLID) {
					if (correctedPos) *correctedPos = (y0 + 1) * tileSize;
					return true;
				}
			}
			break;
	}
	
	return false;
}

bool TileMap::isOnLadder(const glm::ivec2 &pos, const glm::ivec2 &size) const
{
	// Check if the center-bottom area of the player overlaps a ladder tile
	int centerX = (pos.x + size.x / 2) / tileSize;
	int y0 = pos.y / tileSize;
	int y1 = (pos.y + size.y - 1) / tileSize;

	if (centerX < 0 || centerX >= mapSize.x || y0 < 0 || y1 >= mapSize.y)
		return false;

	for (int y = y0; y <= y1; y++)
	{
		if (getTileType(map[y * mapSize.x + centerX]) == TileType::LADDER)
			return true;
	}
	return false;
}

bool TileMap::isOnSpring(const glm::ivec2 &pos, const glm::ivec2 &size) const
{
	glm::ivec2 spriteSize(tileSize, tileSize);
	for (const auto &spawn : springSpawnLocations)
	{
		bool overlapX = pos.x < spawn.x + spriteSize.x && pos.x + size.x > spawn.x;
		bool overlapY = pos.y < spawn.y + spriteSize.y && pos.y + size.y > spawn.y;
		if (overlapX && overlapY)
			return true;
	}
	return false;
}

bool TileMap::isOnDash(const glm::ivec2 &pos, const glm::ivec2 &size, bool *facingLeft) const
{
	glm::ivec2 spriteSize(tileSize, tileSize);
	for (size_t i = 0; i < dashSpawnLocations.size(); ++i)
	{
		const auto &spawn = dashSpawnLocations[i];
		bool overlapX = pos.x < spawn.x + spriteSize.x && pos.x + size.x > spawn.x;
		bool overlapY = pos.y < spawn.y + spriteSize.y && pos.y + size.y > spawn.y;
		if (overlapX && overlapY)
		{
			if (facingLeft && i < dashSpawnFacingLeft.size())
				*facingLeft = dashSpawnFacingLeft[i];
			return true;
		}
	}
	return false;
}