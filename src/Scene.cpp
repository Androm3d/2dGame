#include <iostream>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Scene.h"
#include "Game.h"


#define SCREEN_X 0
#define SCREEN_Y 0


Scene::Scene()
{
	map = NULL;
	player = NULL;
	sword = nullptr;
	bgVao = 0;
	bgVbo = 0;
}

Scene::~Scene()
{
	clearLevelEntities();
	if (bgVbo != 0) glDeleteBuffers(1, &bgVbo);
	if (bgVao != 0) glDeleteVertexArrays(1, &bgVao);
	texProgram.free();
	bgProgram.free();
}

void Scene::clearLevelEntities()
{
	for (Sprite* key : keys) delete key;
	keys.clear();
	for (Sprite* heal : heals) delete heal;
	heals.clear();
	for (Sprite* shield : shields) delete shield;
	shields.clear();
	for (Sprite* weight : weights) delete weight;
	weights.clear();
	for (Sprite* door : doors) delete door;
	doors.clear();
	for (Sprite* portal : portals) delete portal;
	portals.clear();

	if (sword != nullptr) {
		delete sword;
		sword = nullptr;
	}
	if (map != nullptr) {
		delete map;
		map = nullptr;
	}
	if (player != nullptr) {
		delete player;
		player = nullptr;
	}
}


void Scene::init()
{
	clearLevelEntities();
	if (bgVbo != 0) {
		glDeleteBuffers(1, &bgVbo);
		bgVbo = 0;
	}
	if (bgVao != 0) {
		glDeleteVertexArrays(1, &bgVao);
		bgVao = 0;
	}
	texProgram.free();
	bgProgram.free();

	initShaders();
    std::string mapFile = Game::instance().getCurrentMapName();
    map = TileMap::createTileMap(mapFile, glm::vec2(0, 0), texProgram);	player = new Player();

	glm::ivec2 mapTiles = map->getMapSize();
	float mapPixelWidth = mapTiles.x * map->getTileSize();
	float mapPixelHeight = mapTiles.y * map->getTileSize();

	glm::vec2 playerInitPos(0, 0);

	player->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	if (map->getSpawnLocations().empty()) {
		std::cerr << "Warning: No spawn point defined in the map! Defaulting to (0,0)." << std::endl;
		
	}
	else {
		std::cout << "Player spawn point: (" << map->getSpawnLocations()[0].x << ", " << map->getSpawnLocations()[0].y << ")" << std::endl;
		playerInitPos =  glm::vec2(map->getSpawnLocations()[0]);
	}
	
	player->setPosition(playerInitPos);
	player->setTileMap(map);
	projection = glm::ortho(0.f, mapPixelWidth, mapPixelHeight, 0.f);
	currentTime = 0.0f;

	for (Sprite* k : keys) delete k; keys.clear();
	for (Sprite* d : doors) delete d; doors.clear();
	for (Sprite* p : portals) delete p; portals.clear();
	for (Sprite* h : heals) delete h; heals.clear();
	for (Sprite* s : shields) delete s; shields.clear();
	for (Sprite* w : weights) delete w; weights.clear();
	if (sword != nullptr) {  delete sword;  sword = nullptr; }

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


	// puertas y portales
	// 1. Load the texture
	texDoor.loadFromFile("images/door.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texDoor.setMinFilter(GL_NEAREST);
	texDoor.setMagFilter(GL_NEAREST);

	glm::vec2 doorSizeInTexture(
		float(32.0f) / float(texDoor.width()),
		float(64.0f) / float(texDoor.height())
	);

	// 2. Spawn Doors
	for (glm::ivec2 pos : map->getDoors()) {
		Sprite* door = Sprite::createSprite(glm::vec2(32, 64), doorSizeInTexture, &texDoor, &texProgram);
		
		door->setNumberAnimations(2);
		
		// Animation 0: Closed (Just the very first frame)
		door->setAnimationSpeed(0, 1);
		door->addKeyframe(0, glm::vec2(0.0f, 0.0f)); 
		
		// Animation 1: Opening (7 frames, horizontally)
		door->setAnimationSpeed(1, 10); // 10 frames per second looks good
		door->setAnimationLoop(1, false); // CRITICAL: Don't loop! Stop when fully open.
		for (int f = 0; f < 7; f++) {
			// Change the X coordinate (f * width), leave Y at 0.0f
			door->addKeyframe(1, glm::vec2(f * doorSizeInTexture.x, 0.0f)); 
		}
		
		door->changeAnimation(0); // Start closed
		door->setPosition(glm::vec2(pos.x, pos.y - 32));
		doors.push_back(door);
	}


	// portales
	texPortal.loadFromFile("images/portal.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texPortal.setMinFilter(GL_NEAREST);
	texPortal.setMagFilter(GL_NEAREST);

	glm::vec2 portalSizeInTexture(
		float(32.0f) / float(texPortal.width()),
		float(64.0f) / float(texPortal.height())
	);

	for (glm::ivec2 pos : map->getPortals()) {
		Sprite* portal = Sprite::createSprite(glm::vec2(32, 64), portalSizeInTexture, &texPortal, &texProgram);
		
		portal->setNumberAnimations(1);
		portal->setAnimationSpeed(0, 10); 
		portal->setAnimationLoop(0, true); // CRITICAL: Loop forever
		
		// 10 frames, horizontally
		for (int f = 0; f < 10; f++) {
			// Notice the first argument is 0 (we are adding frames to Animation 0)
			portal->addKeyframe(0, glm::vec2(f * portalSizeInTexture.x, 0.0f)); 
		}
		
		portal->changeAnimation(0); 
		portal->setPosition(glm::vec2(pos.x, pos.y - 32));
		portals.push_back(portal);
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
        SCREEN_WIDTH, 0.0f,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        0.0f, 0.0f,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        0.0f, SCREEN_HEIGHT
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
    glm::ivec2 pSize = glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT);

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
		glm::vec2 wPos = weights[i]->getPosition();
		int pushStep = 2;
		int fallStep = 2;
		glm::ivec2 weightSize(32, 32);

		if (checkAABB(pPos, pSize, wPos, weightSize)) {
			int newX = static_cast<int>(wPos.x);
			if (Game::instance().getKey(GLFW_KEY_LEFT))
				newX -= pushStep;
			else if (Game::instance().getKey(GLFW_KEY_RIGHT))
				newX += pushStep;

			if (newX != static_cast<int>(wPos.x)) {
				glm::ivec2 weightPos(newX, static_cast<int>(wPos.y));
				CollisionDir dir = (newX < static_cast<int>(wPos.x)) ? CollisionDir::LEFT : CollisionDir::RIGHT;
				if (!map->checkCollision(weightPos, weightSize, dir)) {
					weights[i]->setPosition(glm::vec2(float(newX), wPos.y));
					wPos.x = float(newX);
				}
			}
		}

		// Gravity: weights should fall when nothing supports them below e
		glm::ivec2 fallCheckPos(static_cast<int>(wPos.x), static_cast<int>(wPos.y) + fallStep);
		int correctedY = 0;
		// no cayendo 
		if (map->checkCollision(fallCheckPos, weightSize, CollisionDir::DOWN, &correctedY)) {
			weights[i]->setPosition(glm::vec2(wPos.x, float(correctedY)));
		} 
		// cayendo
		else {
			weights[i]->setPosition(glm::vec2(wPos.x, wPos.y + 2*float(fallStep)));
		}

		weights[i]->update(deltaTime);
	}

    // --- CHECK EXIT DOORS ---
    for (Sprite* portal : portals) {
        // We use a small AABB to make sure Bugs is standing right in front of it
        if (checkAABB(pPos, pSize, portal->getPosition(), glm::ivec2(32, 64))) {
            
            // Player pressed UP?
			if (Game::instance().getKey(GLFW_KEY_UP)) {
				
				glm::vec2 portPos = portal->getPosition();
				int currentX = Game::instance().currentRoomX;
				int currentY = Game::instance().currentRoomY;

				// Is the portal on the Right side of the screen? (e.g., X > 800)
				if (portPos.x > SCREEN_WIDTH * 0.8f) {
					Game::instance().currentRoomX = currentX + 1; // Go Right
				}
				// Is it on the Left side?
				else if (portPos.x < SCREEN_WIDTH * 0.2f) {
					Game::instance().currentRoomX = currentX - 1; // Go Left
				}
				// Is it at the Top?
				else if (portPos.y < SCREEN_HEIGHT * 0.2f) {
					Game::instance().currentRoomY = currentY - 1; // Go Up
				}
				// Must be at the Bottom!
				else {
					Game::instance().currentRoomY = currentY + 1; // Go Down
				}

				// Now reload the Scene with the new map!
				cout << "Teleporting to: " << Game::instance().getCurrentMapName() << endl;
				
				Game::instance().exitSideRoom();
				Game::instance().reloadScene();
				return;
			}
        }
		portal ->update(deltaTime); 
    }

    // --- CHECK ROOM DOORS ---
	for (int i = 0; i < doors.size(); i++) {
		Sprite* door = doors[i];

		if (checkAABB(pPos, pSize, door->getPosition(), glm::ivec2(32, 64))) {
			if (Game::instance().getKey(GLFW_KEY_UP)) {
				door->changeAnimation(1); // Open the door!

				std::string targetRoom = "";
				std::string currentMap = Game::instance().getCurrentMapName();

				// Hardcode your side-room destinations here:
				if (currentMap == "levels/map_0_2.json") {
					if (i == 0) targetRoom = "levels/room_A.json";
					if (i == 1) targetRoom = "levels/room_B.json";
				}
				else if (currentMap == "levels/map_1_2.json") {
					if (i == 0) targetRoom = "levels/room_C.json";
				}

				cout << "Entering side room: " << targetRoom << endl;
				if (!targetRoom.empty()) {
					Game::instance().enterSideRoom(targetRoom);
					Game::instance().reloadScene();
					return;
				}
			}
		}
		door->update(deltaTime);
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

	for (Sprite* key : keys) { key->render(); }
	if (sword) sword->render();
	for (Sprite* heal : heals) { heal->render(); }
	for (Sprite* shield : shields) { shield->render(); }
	for (Sprite* weight : weights) { weight->render(); }
	for (Sprite* door : doors) { door->render(); }
	for (Sprite* portal : portals) { portal->render(); }

	player->render();

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



