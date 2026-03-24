#include <iostream>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Scene.h"
#include "Game.h"


#define SCREEN_X 0
#define SCREEN_Y 0

#define INIT_PLAYER_X_TILES 0
#define INIT_PLAYER_Y_TILES 0


Scene::Scene()
{
	map = NULL;
	player = NULL;
}

Scene::~Scene()
{
	texProgram.free();
	if(map != NULL)
		delete map;
	if(player != NULL)
		delete player;
}


void Scene::init()
{
	initShaders();
	map = TileMap::createTileMap("levels/map.json", glm::vec2(SCREEN_X, SCREEN_Y), texProgram);
	player = new Player();
	player->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	player->setPosition(glm::vec2(INIT_PLAYER_X_TILES * map->getTileSize(), INIT_PLAYER_Y_TILES * map->getTileSize()));
	player->setTileMap(map);
	projection = glm::ortho(0.f, float(SCREEN_WIDTH), float(SCREEN_HEIGHT), 0.f);
	currentTime = 0.0f;

	// texturas
    texKey.loadFromFile("../images/key.png", TEXTURE_PIXEL_FORMAT_RGBA);
    texKey.setMinFilter(GL_NEAREST);
    texKey.setMagFilter(GL_NEAREST); // GL_NEAREST keeps pixel art crisp!

    texSword.loadFromFile("../images/sword.png", TEXTURE_PIXEL_FORMAT_RGBA);
    texSword.setMinFilter(GL_NEAREST);
    texSword.setMagFilter(GL_NEAREST);

    texHeal.loadFromFile("../images/heal.png", TEXTURE_PIXEL_FORMAT_RGBA);
    texHeal.setMinFilter(GL_NEAREST);
    texHeal.setMagFilter(GL_NEAREST);

	texShield.loadFromFile("../images/shield.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texShield.setMinFilter(GL_NEAREST);
	texShield.setMagFilter(GL_NEAREST);

	texWeight.loadFromFile("../images/weight.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texWeight.setMinFilter(GL_NEAREST);
	texWeight.setMagFilter(GL_NEAREST);


	// Crear sprites para cada item en el mapa

	for (glm::ivec2 pos : map->getKeySpawns()) {
        // Create a real Sprite object (you'll need to load a key texture in Scene.h!)
        Sprite* newKey = Sprite::createSprite(glm::vec2(32, 32), glm::vec2(1.0, 1.0), &texKey, &texProgram);
        newKey->setPosition(pos);
        keys.push_back(newKey); // Add it to your vector
    }

	if (!map->getSwordSpawns().empty()) {
		glm::ivec2 pos = map->getSwordSpawns()[0]; 
		Sprite* newSword = Sprite::createSprite(glm::vec2(32, 32), glm::vec2(1.0, 1.0), &texSword, &texProgram);
		newSword->setPosition(pos);
		sword = newSword; // Store it in the sword member variable
	}

	for (glm::ivec2 pos : map->getHealSpawns()) {
		Sprite* newHeal = Sprite::createSprite(glm::vec2(32, 32), glm::vec2(1.0, 1.0), &texHeal, &texProgram);
		newHeal->setPosition(pos);
		heals.push_back(newHeal);
	}

	for (glm::ivec2 pos : map->getShieldSpawns()) {
		Sprite* newShield = Sprite::createSprite(glm::vec2(32, 32), glm::vec2(1.0, 1.0), &texShield, &texProgram);
		newShield->setPosition(pos);
		shields.push_back(newShield);
	}

	for (glm::ivec2 pos : map->getWeightSpawns()) {
		Sprite* newWeight = Sprite::createSprite(glm::vec2(32, 32), glm::vec2(1.0, 1.0), &texWeight, &texProgram);
		newWeight->setPosition(pos);
		weights.push_back(newWeight);
	}


	// fondo con shaders
	    // 1. Compile the Background Shader
    Shader bgVert, bgFrag;
    bgVert.initFromFile(VERTEX_SHADER, "shaders/bg.vert");
    bgFrag.initFromFile(FRAGMENT_SHADER, "shaders/bg.frag");
    
    bgProgram.init();
    bgProgram.addShader(bgVert);
    bgProgram.addShader(bgFrag);
    bgProgram.link();
    bgProgram.bindFragmentOutput("outColor");
    
    bgVert.free();
    bgFrag.free();

// 2. Build the Fullscreen Rectangle (Quad)
    // These are the X, Y coordinates for two triangles making a 640x320 rectangle
    float vertices[12] = {
        0.0f, 0.0f,
        640.0f, 0.0f,
        640.0f, 320.0f,
        0.0f, 0.0f,
        640.0f, 320.0f,
        0.0f, 320.0f
    };

    glGenVertexArrays(1, &bgVao);
    glBindVertexArray(bgVao);
    
    glGenBuffers(1, &bgVbo);
    glBindBuffer(GL_ARRAY_BUFFER, bgVbo);
    glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), vertices, GL_STATIC_DRAW);

    // Bind the "position" variable from our bg.vert shader
    GLint posLocation = bgProgram.bindVertexAttribute("position", 2, 0, 0);
    glEnableVertexAttribArray(posLocation);
}
    

bool Scene::checkAABB(const glm::vec2& pos1, const glm::ivec2& size1, const glm::vec2& pos2, const glm::ivec2& size2) {
    return (pos1.x < pos2.x + size2.x &&
            pos1.x + size1.x > pos2.x &&
            pos1.y < pos2.y + size2.y &&
            pos1.y + size1.y > pos2.y);
}


void Scene::update(int deltaTime)
{
	currentTime += deltaTime;
    player->update(deltaTime);

    glm::vec2 pPos = player->getPosition();
    glm::ivec2 pSize = glm::ivec2(32, 64); // TODO: ver si sería mejor coger el hitbox del define de player.cpp

    // un bucle por tipo de item, como solo hay 5 no pasa nada pero si añadimos más habría que crear una clase Item o algo así para no repetir código
    for (int i = keys.size() - 1; i >= 0; i--) {
        if (checkAABB(pPos, pSize, keys[i]->getPosition(), glm::ivec2(32, 32))) {
            
            Game::instance().addKey();     
            delete keys[i];                
            keys.erase(keys.begin() + i); 
            std::cout << "KEY PICKED UP" << std::endl;
        }
    }

	if ((sword != nullptr || sword != NULL) && checkAABB(pPos, pSize, sword->getPosition(), glm::ivec2(32, 32))) {
		
		Game::instance().hasSword = true; 
		delete sword;
		sword = nullptr;
		std::cout << "GOT THE SWORD" << std::endl;
	}

	for (int i = heals.size() - 1; i >= 0; i--) {
		if (checkAABB(pPos, pSize, heals[i]->getPosition(), glm::ivec2(32, 32))) {
			Game::instance().lives++; 
			delete heals[i];
			heals.erase(heals.begin() + i);
			std::cout << "HEAL PICKED UP" << std::endl;
		}
	}

	for (int i = shields.size() - 1; i >= 0; i--) {
		if (checkAABB(pPos, pSize, shields[i]->getPosition(), glm::ivec2(32, 32))) {
			Game::instance().hasShield = true; 
			delete shields[i];
			shields.erase(shields.begin() + i);
			std::cout << "SHIELD PICKED UP" << std::endl;
		}
	}

	for (int i = weights.size() - 1; i >= 0; i--) {
		if (checkAABB(pPos, pSize, weights[i]->getPosition(), glm::ivec2(32, 32))) {
			// TODO: poder empujar el peso, por ahora se borra y ya
			delete weights[i];
			weights.erase(weights.begin() + i);
			std::cout << "WEIGHT PICKED UP" << std::endl;
		}
	}
    

    
}

void Scene::render()
{
	glm::mat4 modelview;

	bgProgram.use();
    bgProgram.setUniformMatrix4f("projection", projection);
    
    // Pass time in seconds (currentTime is usually in milliseconds)
    bgProgram.setUniform1f("time", currentTime / 1000.0f); 
    
    glBindVertexArray(bgVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

	texProgram.use();
	texProgram.setUniformMatrix4f("projection", projection);
	texProgram.setUniform4f("color", 1.0f, 1.0f, 1.0f, 1.0f);
	modelview = glm::mat4(1.0f);
	texProgram.setUniformMatrix4f("modelview", modelview);
	texProgram.setUniform2f("texCoordDispl", 0.f, 0.f);
	map->render();
	player->render();

	for (Sprite* key : keys) { key->render(); }
	if (sword) sword->render();
	for (Sprite* heal : heals) { heal->render(); }
	for (Sprite* shield : shields) { shield->render(); }
	for (Sprite* weight : weights) { weight->render(); }

}

void Scene::initShaders()
{
	Shader vShader, fShader;

	vShader.initFromFile(VERTEX_SHADER, "shaders/texture.vert");
	if(!vShader.isCompiled())
	{
		cout << "Vertex Shader Error" << endl;
		cout << "" << vShader.log() << endl << endl;
	}
	fShader.initFromFile(FRAGMENT_SHADER, "shaders/texture.frag");
	if(!fShader.isCompiled())
	{
		cout << "Fragment Shader Error" << endl;
		cout << "" << fShader.log() << endl << endl;
	}
	texProgram.init();
	texProgram.addShader(vShader);
	texProgram.addShader(fShader);
	texProgram.link();
	if(!texProgram.isLinked())
	{
		cout << "Shader Linking Error" << endl;
		cout << "" << texProgram.log() << endl << endl;
	}
	texProgram.bindFragmentOutput("outColor");
	vShader.free();
	fShader.free();
}



