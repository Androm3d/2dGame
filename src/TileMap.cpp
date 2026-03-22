#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "TileMap.h"
#include "../external/json/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

TileMap *TileMap::createTileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program)
{
	TileMap *map = new TileMap(levelFile, minCoords, program);
	map->setupTileDictionary();
	return map;
}


TileMap::TileMap(const string &levelFile, const glm::vec2 &minCoords, ShaderProgram &program)
{

    if (levelFile.length() >= 5 && levelFile.substr(levelFile.length() - 5) == ".json") {
        loadLevelJSON(levelFile);
    } else {
        loadLevel(levelFile); 
    }
    
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

void TileMap::setupTileDictionary()
{
	// 1. One-Way Platforms (Tree leaves)
	tileDictionary[109] = TileType::ONE_WAY_PLATFORM;
	tileDictionary[110] = TileType::ONE_WAY_PLATFORM;
	tileDictionary[111] = TileType::ONE_WAY_PLATFORM;
	tileDictionary[117] = TileType::ONE_WAY_PLATFORM;
	tileDictionary[118] = TileType::ONE_WAY_PLATFORM;
	tileDictionary[119] = TileType::ONE_WAY_PLATFORM;

	// 2. Solid Blocks (I am guessing these based on your JSON so Bugs has a floor!)
	// You might need to add/remove some here depending on your sprite sheet
	for (int i = 1; i <= 7; i++) tileDictionary[i] = TileType::SOLID; // Ground/Bridge
	for (int i = 34; i <= 36; i++) tileDictionary[i] = TileType::SOLID; // Ground
	
	// Notice we DO NOT add the water (89, 97, 98) 
	// or the vegetation/bridge base (65-72, 121-130). 
	// Because they aren't here, they default to EMPTY!
}

void TileMap::render() const
{
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
    ifstream fin(levelFile.c_str());
    if(!fin.is_open()) {
        cout << "Error: Could not open JSON file: " << levelFile << endl;
        return false;
    }

    json j;
    fin >> j;
    fin.close();

    mapSize.x = j["width"];
    mapSize.y = j["height"];
    tileSize = j["tilewidth"];
    blockSize = tileSize;

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
        cout << "WARNING: Tileset is not embedded in Tiled! Falling back to default." << endl;
        rawImagePath = "tiles.png"; 
        
        // Put the width/height of your actual tiles.png here as a fallback
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
    
    map = new int[mapSize.x * mapSize.y];
    auto data = j["layers"][0]["data"];
    
    for(int i = 0; i < mapSize.x * mapSize.y; i++)
    {
        unsigned int raw_tile = data[i];
        unsigned int tile_id = raw_tile & 0x1FFFFFFF; // Mask out flip flags
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
	
	fin.open(levelFile.c_str());
	if(!fin.is_open())
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
bool TileMap::checkCollision(const glm::ivec2 &pos, const glm::ivec2 &size, CollisionDir dir, int *correctedPos) const
{
	int x0, x1, y0, y1;
	
	x0 = pos.x / tileSize;
	x1 = (pos.x + size.x - 1) / tileSize;
	y0 = pos.y / tileSize;
	y1 = (pos.y + size.y - 1) / tileSize;

	// Treat going out of map bounds as hitting a solid wall
	if (x0 < 0 || x1 >= mapSize.x || y0 < 0 || y1 >= mapSize.y) return true;

	switch (dir) {
		case CollisionDir::LEFT:
			for (int y = y0; y <= y1; y++) {
				if (getTileType(map[y * mapSize.x + x0]) == TileType::SOLID) {
					if (correctedPos) *correctedPos = (x0 + 1) * tileSize;
					return true;
				}
			}
			break;

		case CollisionDir::RIGHT:
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
				else if (type == TileType::ONE_WAY_PLATFORM) {
					// ONE-WAY MAGIC: Only act solid if the player's feet are entering the top of the tile.
					// This prevents snapping to the top when jumping UP through it.
					int playerBottom = pos.y + size.y - 1;
					int tileTop = y1 * tileSize;
					
					// If we are falling downwards into the top ~8 pixels of the leaf platform
					if (playerBottom - tileTop < 8) { 
						if (correctedPos) *correctedPos = tileTop - size.y;
						return true;
					}
				}
			}
			break;

		case CollisionDir::UP:
			for (int x = x0; x <= x1; x++) {
				// Notice ONE_WAY_PLATFORM isn't checked here. You pass right through!
				if (getTileType(map[y0 * mapSize.x + x]) == TileType::SOLID) {
					if (correctedPos) *correctedPos = (y0 + 1) * tileSize;
					return true;
				}
			}
			break;
	}
	
	return false;
}