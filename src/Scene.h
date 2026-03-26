#ifndef _SCENE_INCLUDE
#define _SCENE_INCLUDE


#include <glm/glm.hpp>
#include "ShaderProgram.h"
#include "TileMap.h"
#include "Player.h"


// Scene contains all the entities of our game.
// It is responsible for updating and render them.


class Scene
{

public:
	Scene();
	~Scene();

	void init();
	void update(int deltaTime);
	void render();
	bool checkAABB(const glm::vec2& pos1, const glm::ivec2& size1, const glm::vec2& pos2, const glm::ivec2& size2);

	const std::vector<Sprite*>& getKeys() const { return keys; }
	const std::vector<Sprite*>& getHeals() const { return heals; }
	const std::vector<Sprite*>& getShields() const { return shields; }
	const std::vector<Sprite*>& getWeights() const { return weights; }
	const Sprite* getSword() const { return sword; }
	const std::vector<Sprite*>& getPortals() const { return portals; }
	const std::vector<Sprite*>& getDoors() const { return doors; }
	

private:
	void initShaders();

private:
	TileMap *map;
	Player *player;
	ShaderProgram texProgram;
	float currentTime;
	glm::mat4 projection;

	Texture texKey, texHeal, texShield, texWeight, texSword;
	std::vector<Sprite*> keys;
	std::vector<Sprite*> heals;
	Sprite* sword;
	std::vector<Sprite*> shields;
	std::vector<Sprite*> weights;

	Texture texDoor, texPortal;
	std::vector<Sprite*> portals;
	std::vector<Sprite*> doors;

	ShaderProgram bgProgram;
	GLuint bgVao, bgVbo;

};


#endif // _SCENE_INCLUDE

