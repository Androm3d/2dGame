#include <iostream>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Scene.h"
#include "Game.h"
#include "GameConstants.h"


#define SCREEN_X 0
#define SCREEN_Y 0

static const float WEIGHT_GRAVITY = 1800.0f;
static const float WEIGHT_MAX_FALL_SPEED = 240.0f;
static const float WEIGHT_SPRING_JUMP_VELOCITY = std::sqrt(2.0f * WEIGHT_GRAVITY * (96.0f * 3.0f));
static const int WEIGHT_DASH_DURATION_MS = 1000;
static const float WEIGHT_DASH_DISTANCE_BASE = 60.0f;
static const int WEIGHT_SPRING_COOLDOWN_MS = 180;
static const int WEIGHT_DASH_COOLDOWN_MS = 180;
static const float WEIGHT_SPAWN_RAISE_PX = 12.0f;
static const float PLAYER_SPAWN_CLIP_OFFSET = 12.0f;

namespace {
bool parseWorldMapCoords(const std::string &mapName, int &x, int &y)
{
	return std::sscanf(mapName.c_str(), "../levels/map_%d_%d.json", &x, &y) == 2;
}

bool fileExists(const char *path)
{
	std::ifstream f(path);
	return f.good();
}

const char *pickHudFontPath()
{
	const char *fontPath = "../fonts/samurai.ttf";
	if(fileExists(fontPath))
	return fontPath;
	return nullptr;
}
}


Scene::Scene()
{
	map = NULL;
	player = NULL;
	enemy = NULL;
	enemy2 = NULL;
	enemy3 = NULL;
	sword = nullptr;
	bgVao = 0;
	bgVbo = 0;
	attackHitThisSwing = false;
	attackHitEnemy2ThisSwing = false;
	attackHitEnemy3ThisSwing = false;
	enemy2HitPlayerThisSwing = false;
	enemy3HitPlayerThisSwing = false;
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
	weightVelocities.clear();
	weightSpringCooldownMs.clear();
	weightDashCooldownMs.clear();
	weightDashVelocities.clear();
	weightDashVelocityStarts.clear();
	weightDashTimeLeftMs.clear();
	for (Sprite* spring : springs) delete spring;
	springs.clear();
	for (Sprite* dash : dashes) delete dash;
	dashes.clear();
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
	if (enemy != NULL) {
		delete enemy;
		enemy = NULL;
	}
	if (enemy2 != NULL) {
		delete enemy2;
		enemy2 = NULL;
	}
	if (enemy3 != NULL) {
		delete enemy3;
		enemy3 = NULL;
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
	Game::instance().registerRoomKeyTotal(mapFile, int(map->getKeySpawns().size()));
	Game::instance().preloadConnectedRoomKeys();
	Game::instance().totalKeysInLevel = Game::instance().getTotalKeysInCurrentWorld();
	Game::instance().registerRoomHealSpawns(mapFile, int(map->getHealSpawns().size()));
	Game::instance().registerRoomShieldSpawns(mapFile, int(map->getShieldSpawns().size()));
	if (!map->getSwordSpawns().empty())
		Game::instance().registerRoomSwordSpawn(mapFile);

	glm::ivec2 mapTiles = map->getMapSize();
	float mapPixelWidth = mapTiles.x * map->getTileSize();
	float mapPixelHeight = mapTiles.y * map->getTileSize();

	playerInitPos = glm::vec2(0, 0);

	player->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	if (map->getSpawnLocations().empty()) {
		std::cerr << "Warning: No spawn point defined in the map! Defaulting to (0,0)." << std::endl;
		
	}
	else {
		std::cout << "Player spawn point: (" << map->getSpawnLocations()[0].x << ", " << map->getSpawnLocations()[0].y << ")" << std::endl;
		playerInitPos =  glm::vec2(map->getSpawnLocations()[0]);
		playerInitPos.y -= float(Player::HITBOX_HEIGHT - map->getTileSize());
		playerInitPos.y -= PLAYER_SPAWN_CLIP_OFFSET;
	}

	int spawnDoorIndex = -1;
	if (Game::instance().consumeNextSpawnDoor(mapFile, spawnDoorIndex)) {
		const std::vector<glm::ivec2> &doorSpawns = map->getDoors();
		if (spawnDoorIndex >= 0 && spawnDoorIndex < int(doorSpawns.size())) {
			playerInitPos = glm::vec2(doorSpawns[spawnDoorIndex]);
			playerInitPos.y -= float(map->getTileSize());
			playerInitPos.y -= PLAYER_SPAWN_CLIP_OFFSET;
		}
	}
	
	player->setPosition(playerInitPos);
	player->setTileMap(map);
	{
		glm::ivec2 spawnSnapPos(int(playerInitPos.x), int(playerInitPos.y));
		int spawnGroundY = 0;
		if (map->checkCollision(spawnSnapPos, glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), CollisionDir::DOWN, &spawnGroundY, false))
		{
			playerInitPos.y = float(spawnGroundY) - PLAYER_SPAWN_CLIP_OFFSET;
			player->setPosition(playerInitPos);
		}
	}
	enemy = new Enemy();
	enemy->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	if (!map->getEnemy1Spawns().empty()) {
		enemySpawnPos = glm::vec2(map->getEnemy1Spawns()[0]);
		enemy->setPosition(enemySpawnPos);
		enemy->setActive(false);   // empieza dormido
		enemyActivated = false;
	} else {
		enemy->setActive(false);
		enemyActivated = false;
		enemySpawnPos = glm::vec2(-9999.f);
		std::cerr << "Warning: No SPAWN_ENEMY_1 tile found. Enemy1 disabled." << std::endl;
	}
	enemy->setTileMap(map);
	enemy2 = new Enemy2();
	enemy2->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	if (!map->getEnemy2Spawns().empty()) {
		enemy2SpawnPos = glm::vec2(map->getEnemy2Spawns()[0]);
		enemy2->setPosition(enemy2SpawnPos);
		enemy2->setActive(false);
		enemy2Activated = false;
	} else {
		enemy2->setActive(false);
		enemy2Activated = false;
		enemy2SpawnPos = glm::vec2(-9999.f);
		std::cerr << "Warning: No SPAWN_ENEMY_2 tile found. Enemy2 disabled." << std::endl;
	}
	enemy2->setTileMap(map);
	enemy3 = new Enemy3();
	enemy3->init(glm::ivec2(SCREEN_X, SCREEN_Y), texProgram);
	if (!map->getEnemy3Spawns().empty()) {
		enemy3SpawnPos = glm::vec2(map->getEnemy3Spawns()[0]);
		enemy3->setPosition(enemy3SpawnPos);
		enemy3->setActive(false);
		enemy3Activated = false;
	} else {
		enemy3->setActive(false);
		enemy3Activated = false;
		enemy3SpawnPos = glm::vec2(-9999.f);
		std::cerr << "Warning: No SPAWN_ENEMY_3 tile found. Enemy3 disabled." << std::endl;
	}
	enemy3->setTileMap(map);
	viewWidth = float(map->getRoomSize().x * map->getTileSize());
	viewHeight = float(map->getRoomSize().y * map->getTileSize());
	if (viewWidth > mapPixelWidth) viewWidth = mapPixelWidth;
	if (viewHeight > mapPixelHeight) viewHeight = mapPixelHeight;

	// Resize background VBO to match exact view dimensions natively instead of SCREEN_WIDTH fixed
	float bgVerts[12] = {
		0.0f, 0.0f,
		viewWidth, 0.0f,
		viewWidth, viewHeight,
		0.0f, 0.0f,
		viewWidth, viewHeight,
		0.0f, viewHeight
	};
	glBindBuffer(GL_ARRAY_BUFFER, bgVbo);
	glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), bgVerts, GL_STATIC_DRAW);

	projection = glm::ortho(0.f, viewWidth, viewHeight, 0.f);
	currentTime = 0.0f;
	transitionPending = false;
	transitionDelayMs = 0;
	transitionToSideRoom = false;
	transitionToWorldRoom = false;
	pendingTargetMap.clear();
	pendingTargetDoorIndex = -1;
	upWasDown = Game::instance().getKey(GLFW_KEY_UP);
	updateCamera();

	for (Sprite* k : keys) delete k; keys.clear();
	for (Sprite* d : doors) delete d; doors.clear();
	for (Sprite* p : portals) delete p; portals.clear();
	for (Sprite* h : heals) delete h; heals.clear();
	for (Sprite* s : shields) delete s; shields.clear();
	for (Sprite* w : weights) delete w; weights.clear();
	weightVelocities.clear();
	weightSpringCooldownMs.clear();
	weightDashCooldownMs.clear();
	weightDashVelocities.clear();
	weightDashVelocityStarts.clear();
	weightDashTimeLeftMs.clear();
	for (Sprite* sp : springs) delete sp; springs.clear();
	for (Sprite* d : dashes) delete d; dashes.clear();
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

	texSpring.loadFromFile("../images/spring.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texSpring.setMinFilter(GL_NEAREST);
	texSpring.setMagFilter(GL_NEAREST);

	texDash.loadFromFile("../images/dash.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texDash.setMinFilter(GL_NEAREST);
	texDash.setMagFilter(GL_NEAREST);


	// Crear sprites para cada item en el mapa

	int alreadyCollectedKeys = Game::instance().getCollectedKeysForRoom(mapFile);
	const std::vector<glm::ivec2> &allKeySpawns = map->getKeySpawns();
	for (int keyIndex = alreadyCollectedKeys; keyIndex < int(allKeySpawns.size()); ++keyIndex) {
		glm::ivec2 pos = allKeySpawns[keyIndex];
        // Create a real Sprite object (you'll need to load a key texture in Scene.h!)
        Sprite* newKey = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), glm::vec2(1.0, 1.0), &texKey, &texProgram);
        newKey->setPosition(pos);
        keys.push_back(newKey); // Add it to your vector
    }

	if (!map->getSwordSpawns().empty() && !Game::instance().hasSwordBeenCollectedInRoom(mapFile)) {
		glm::ivec2 pos = map->getSwordSpawns()[0]; 
		Sprite* newSword = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), glm::vec2(1.0, 1.0), &texSword, &texProgram);
		newSword->setPosition(pos);
		sword = newSword; // Store it in the sword member variable
	}

	int alreadyCollectedHeals = Game::instance().getCollectedHealsForRoom(mapFile);
	const std::vector<glm::ivec2> &allHealSpawns = map->getHealSpawns();
	for (int healIndex = alreadyCollectedHeals; healIndex < int(allHealSpawns.size()); ++healIndex) {
		glm::ivec2 pos = allHealSpawns[healIndex];
		Sprite* newHeal = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), glm::vec2(1.0, 1.0), &texHeal, &texProgram);
		newHeal->setPosition(pos);
		heals.push_back(newHeal);
	}

	int alreadyCollectedShields = Game::instance().getCollectedShieldsForRoom(mapFile);
	const std::vector<glm::ivec2> &allShieldSpawns = map->getShieldSpawns();
	for (int shieldIndex = alreadyCollectedShields; shieldIndex < int(allShieldSpawns.size()); ++shieldIndex) {
		glm::ivec2 pos = allShieldSpawns[shieldIndex];
		Sprite* newShield = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), glm::vec2(1.0, 1.0), &texShield, &texProgram);
		newShield->setPosition(pos);
		shields.push_back(newShield);
	}

	for (glm::ivec2 pos : map->getWeightSpawns()) {
		Sprite* newWeight = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), glm::vec2(1.0, 1.0), &texWeight, &texProgram);
		glm::vec2 spawnPos(float(pos.x), float(pos.y) - WEIGHT_SPAWN_RAISE_PX);
		if (spawnPos.y < 0.0f) spawnPos.y = 0.0f;
		// Resolve initial placement onto support tiles to avoid first-frame one-way clipping.
		glm::ivec2 spawnCheckPos(int(spawnPos.x), int(spawnPos.y + 1.0f));
		glm::ivec2 weightSize(map->getTileSize(), map->getTileSize());
		int correctedY = 0;
		if (map->checkCollision(spawnCheckPos, weightSize, CollisionDir::DOWN, &correctedY, false))
			spawnPos.y = float(correctedY);
		newWeight->setPosition(spawnPos);
		weights.push_back(newWeight);
		weightVelocities.push_back(0.0f);
		weightSpringCooldownMs.push_back(0);
		weightDashCooldownMs.push_back(0);
		weightDashVelocities.push_back(0.0f);
		weightDashVelocityStarts.push_back(0.0f);
		weightDashTimeLeftMs.push_back(0);
	}

	glm::vec2 springFrameSize(
		float(map->getTileSize()) / float(texSpring.width()),
		float(map->getTileSize()) / float(texSpring.height())
	);
	for (glm::ivec2 pos : map->getSpringSpawns()) {
		Sprite* spring = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), springFrameSize, &texSpring, &texProgram);
		spring->setNumberAnimations(2);
		spring->setAnimationSpeed(0, 1);
		spring->setAnimationLoop(0, true);
		spring->addKeyframe(0, glm::vec2(0.0f, 0.0f));
		spring->setAnimationSpeed(1, 12);
		spring->setAnimationLoop(1, false);
		for (int f = 1; f < 4; ++f) {
			spring->addKeyframe(1, glm::vec2(f * springFrameSize.x, 0.0f));
		}
		spring->changeAnimation(0);
		spring->setPosition(pos);
		springs.push_back(spring);
	}

	glm::vec2 dashFrameSize(
		44.0f / float(texDash.width()),
		float(map->getTileSize()) / float(texDash.height())
	);
	const auto &dashSpawns = map->getDashSpawns();
	const auto &dashFacingLeft = map->getDashSpawnFacingLeft();
	for (size_t i = 0; i < dashSpawns.size(); ++i) {
		glm::ivec2 pos = dashSpawns[i];
		Sprite* dash = Sprite::createSprite(glm::vec2(map->getTileSize(), map->getTileSize()), dashFrameSize, &texDash, &texProgram);
		dash->setNumberAnimations(1);
		dash->setAnimationSpeed(0, 10);
		dash->setAnimationLoop(0, true);
		for (int f = 0; f < 4; ++f) {
			dash->addKeyframe(0, glm::vec2(f * dashFrameSize.x, 0.0f));
		}
		dash->changeAnimation(0);
		if (i < dashFacingLeft.size())
			dash->setFlipHorizontal(!dashFacingLeft[i]);
		dash->setPosition(pos);
		dashes.push_back(dash);
	}


	// puertas y portales
	// 1. Load the texture
	texDoor.loadFromFile("../images/door.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texDoor.setMinFilter(GL_NEAREST);
	texDoor.setMagFilter(GL_NEAREST);

	glm::vec2 doorSizeInTexture(
		float(map->getTileSize()) / float(texDoor.width()),
		float(2 * map->getTileSize()) / float(texDoor.height())
	);

	// 2. Spawn Doors
	for (glm::ivec2 pos : map->getDoors()) {
		Sprite* door = Sprite::createSprite(glm::vec2(map->getTileSize(), 2 * map->getTileSize()), doorSizeInTexture, &texDoor, &texProgram);
		
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
		door->setPosition(glm::vec2(pos.x, pos.y - map->getTileSize()));
		doors.push_back(door);
	}


	// portales
	texPortal.loadFromFile("../images/portal.png", TEXTURE_PIXEL_FORMAT_RGBA);
	texPortal.setMinFilter(GL_NEAREST);
	texPortal.setMagFilter(GL_NEAREST);

	glm::vec2 portalSizeInTexture(
		float(map->getTileSize()) / float(texPortal.width()),
		float(2 * map->getTileSize()) / float(texPortal.height())
	);

	for (glm::ivec2 pos : map->getPortals()) {
		Sprite* portal = Sprite::createSprite(glm::vec2(map->getTileSize(), 2 * map->getTileSize()), portalSizeInTexture, &texPortal, &texProgram);
		
		portal->setNumberAnimations(1);
		portal->setAnimationSpeed(0, 10); 
		portal->setAnimationLoop(0, true); // CRITICAL: Loop forever
		
		// 10 frames, horizontally
		for (int f = 0; f < 10; f++) {
			// Notice the first argument is 0 (we are adding frames to Animation 0)
			portal->addKeyframe(0, glm::vec2(f * portalSizeInTexture.x, 0.0f)); 
		}
		
		portal->changeAnimation(0); 
		portal->setPosition(glm::vec2(pos.x, pos.y - map->getTileSize()));
		portals.push_back(portal);
	}

	if(!hudReady)
	{
		const char *fontPath = pickHudFontPath();
		if(fontPath != nullptr)
			hudReady = hudText.init(fontPath);
		if(!hudReady)
			std::cerr << "Warning: HUD font failed to initialize. Status text will be hidden." << std::endl;
	}


	// fondo con shaders
	    // 1. Compile the Background Shader
    Shader bgVert, bgFrag;
	bgVert.initFromFile(VERTEX_SHADER, "../shaders/bg.vert");
	bgFrag.initFromFile(FRAGMENT_SHADER, "../shaders/bg.frag");
    
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

void Scene::updateCamera()
{
	if (player == nullptr || map == nullptr)
		return;

	glm::vec2 pPos = player->getPosition();
	float playerCenterX = pPos.x + float(Player::HITBOX_WIDTH) * 0.5f;
	float playerCenterY = pPos.y + float(Player::HITBOX_HEIGHT) * 0.5f;

	float mapPixelWidth = float(map->getMapSize().x * map->getTileSize());
	float mapPixelHeight = float(map->getMapSize().y * map->getTileSize());

	float targetX = playerCenterX - viewWidth * 0.5f;
	float targetY = playerCenterY - viewHeight * 0.5f;

	float maxCamX = std::max(0.f, mapPixelWidth - viewWidth);
	float maxCamY = std::max(0.f, mapPixelHeight - viewHeight);

	cameraX = std::max(0.f, std::min(targetX, maxCamX));
	cameraY = std::max(0.f, std::min(targetY, maxCamY));
}

void Scene::scheduleTransitionToMap(const std::string &targetMap, bool enterSideRoom, int targetDoorIndex)
{
	if (transitionPending)
		return;

	transitionPending = true;
	transitionDelayMs = TRANSITION_DELAY_MS;
	transitionToWorldRoom = false;
	transitionToSideRoom = enterSideRoom;
	pendingTargetMap = targetMap;
	pendingTargetDoorIndex = targetDoorIndex;
}

void Scene::scheduleTransitionToWorld(int targetRoomX, int targetRoomY)
{
	if (transitionPending)
		return;

	transitionPending = true;
	transitionDelayMs = TRANSITION_DELAY_MS;
	transitionToSideRoom = false;
	transitionToWorldRoom = true;
	pendingTargetDoorIndex = -1;
	pendingRoomX = targetRoomX;
	pendingRoomY = targetRoomY;
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
	float dt = float(deltaTime) / 1000.0f;
	bool upIsDown = Game::instance().getKey(GLFW_KEY_UP);
	bool upJustPressed = upIsDown && !upWasDown;
	upWasDown = upIsDown;

	glm::vec2 preUpdatePos = player->getPosition();
	glm::ivec2 pSize = glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT);
	bool nearInteractive = false;
	for (Sprite* portal : portals) {
		if (checkAABB(preUpdatePos, pSize, portal->getPosition(), glm::ivec2(map->getTileSize(), 2 * map->getTileSize()))) {
			nearInteractive = true;
			break;
		}
	}
	if (!nearInteractive) {
		for (Sprite* door : doors) {
			if (checkAABB(preUpdatePos, pSize, door->getPosition(), glm::ivec2(map->getTileSize(), 2 * map->getTileSize()))) {
				nearInteractive = true;
				break;
			}
		}
	}
	Game::instance().setJumpInputBlocked(nearInteractive && upIsDown);

	if (transitionPending) {
		transitionDelayMs -= deltaTime;
		if (transitionDelayMs <= 0) {
			if (transitionToWorldRoom) {
				Game::instance().currentRoomX = pendingRoomX;
				Game::instance().currentRoomY = pendingRoomY;
				Game::instance().exitSideRoom();
			}
			else if (transitionToSideRoom) {
				Game::instance().enterSideRoom(pendingTargetMap);
			}

			if (pendingTargetDoorIndex >= 0)
				Game::instance().setNextSpawnDoor(Game::instance().getCurrentMapName(), pendingTargetDoorIndex);

			Game::instance().reloadScene();
			return;
		}
	}

	// --- Activacion por proximidad: los enemigos no se activan hasta que el jugador este cerca ---
	{
		glm::vec2 pp = player->getPosition();
		float activationRange = float(ENEMY_ACTIVATION_RANGE);
		if (!enemyActivated) {
			float dx = pp.x - enemySpawnPos.x;
			float dy = pp.y - enemySpawnPos.y;
			if (dx * dx + dy * dy < activationRange * activationRange) {
				enemyActivated = true;
				enemy->setActive(true);
			}
		}
		if (!enemy2Activated) {
			float dx = pp.x - enemy2SpawnPos.x;
			float dy = pp.y - enemy2SpawnPos.y;
			if (dx * dx + dy * dy < activationRange * activationRange) {
				enemy2Activated = true;
				enemy2->setActive(true);
			}
		}
		if (!enemy3Activated) {
			float dx = pp.x - enemy3SpawnPos.x;
			float dy = pp.y - enemy3SpawnPos.y;
			if (dx * dx + dy * dy < activationRange * activationRange) {
				enemy3Activated = true;
				enemy3->setActive(true);
			}
		}
	}

	if (player->isAlive() || player->isDying())
		player->update(deltaTime);
	if (enemy->isAlive() || enemy->isDying())
		enemy->update(deltaTime, player->getPosition());

	// --- Player attack vs Enemy ---
	if (player->isAttacking() && enemy->isAlive() && !attackHitThisSwing)
	{
		glm::vec4 atkBox = player->getAttackHitbox();
		glm::vec4 enemyBox = enemy->getHitbox();
		if (atkBox.x < enemyBox.x + enemyBox.z &&
			atkBox.x + atkBox.z > enemyBox.x &&
			atkBox.y < enemyBox.y + enemyBox.w &&
			atkBox.y + atkBox.w > enemyBox.y)
		{
			int knockDir = (enemy->getPosition().x >= player->getPosition().x) ? 1 : -1;
			enemy->takeDamage(knockDir);
			attackHitThisSwing = true;
		}
	}
	if (player->isAttacking() && Game::instance().hasSword) {
		enemy->destroyArrowsInHitbox(player->getAttackHitbox());
	}
	if (!player->isAttacking())
		attackHitThisSwing = false;

	// --- Enemy1 arrows hit player (or reflect if protecting) ---
	if (player->isAlive() && enemy->isAlive())
	{
		if (player->isProtecting())
			enemy->reflectArrowHit(player->getPosition(), glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), player->isFacingLeft());
		else if (!player->isInvincible())
			if (enemy->checkArrowHit(player->getPosition(), glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT)))
				player->takeDamage();
	}

	// --- Enemy1 reflected arrows vs all enemies ---
	{
		int kd;
		if (enemy->isAlive()  && enemy->checkReflectedArrowHit(enemy->getHitbox(),  kd)) enemy->takeDamage(kd);
		if (enemy2->isAlive() && enemy->checkReflectedArrowHit(enemy2->getHitbox(), kd)) enemy2->takeDamage(kd);
		if (enemy3->isAlive() && enemy->checkReflectedArrowHit(enemy3->getHitbox(), kd)) enemy3->takeDamage(kd);
	}

	// --- Enemy2 update ---
	if (enemy2->isAlive() || enemy2->isDying())
		enemy2->update(deltaTime, player->getPosition());

	// --- Player attack vs Enemy2 ---
	if (player->isAttacking() && enemy2->isAlive() && !attackHitEnemy2ThisSwing)
	{
		glm::vec4 atkBox = player->getAttackHitbox();
		glm::vec4 e2Box  = enemy2->getHitbox();
		if (atkBox.x < e2Box.x + e2Box.z &&
			atkBox.x + atkBox.z > e2Box.x &&
			atkBox.y < e2Box.y + e2Box.w  &&
			atkBox.y + atkBox.w > e2Box.y)
		{
			int knockDir = (enemy2->getPosition().x >= player->getPosition().x) ? 1 : -1;
			enemy2->takeDamage(knockDir);
			attackHitEnemy2ThisSwing = true;
		}
	}
	if (!player->isAttacking())
		attackHitEnemy2ThisSwing = false;

	// --- Enemy2 melee hits player ---
	if (enemy2->isMeleeAttacking() && !enemy2HitPlayerThisSwing)
	{
		if (player->isAlive() && !player->isInvincible())
		{
			glm::vec4 m = enemy2->getMeleeHitbox();
			glm::vec2 pPos = player->getPosition();
			if (m.x < pPos.x + Player::HITBOX_WIDTH  && m.x + m.z > pPos.x &&
				m.y < pPos.y + Player::HITBOX_HEIGHT && m.y + m.w > pPos.y)
			{
				player->takeDamage();
				enemy2HitPlayerThisSwing = true;
			}
		}
	}
	if (!enemy2->isMeleeAttacking()) enemy2HitPlayerThisSwing = false;

	// --- Enemy3 update ---
	if (enemy3->isAlive() || enemy3->isDying())
		enemy3->update(deltaTime, player->getPosition());

	// --- Player attack vs Enemy3 ---
	if (player->isAttacking() && enemy3->isAlive() && !attackHitEnemy3ThisSwing)
	{
		glm::vec4 atkBox = player->getAttackHitbox();
		glm::vec4 e3Box  = enemy3->getHitbox();
		if (atkBox.x < e3Box.x + e3Box.z &&
			atkBox.x + atkBox.z > e3Box.x &&
			atkBox.y < e3Box.y + e3Box.w  &&
			atkBox.y + atkBox.w > e3Box.y)
		{
			int knockDir = (enemy3->getPosition().x >= player->getPosition().x) ? 1 : -1;
			enemy3->takeDamage(knockDir);
			attackHitEnemy3ThisSwing = true;
		}
	}
	if (!player->isAttacking())
		attackHitEnemy3ThisSwing = false;

	// --- Enemy3 fireballs hit player (or reflect if protecting) ---
	if (player->isAlive() && enemy3->isAlive())
	{
		if (player->isProtecting())
			enemy3->reflectFireballHit(player->getPosition(), glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT), player->isFacingLeft());
		else if (!player->isInvincible())
			if (enemy3->checkFireballHit(player->getPosition(), glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT)))
				player->takeDamage();
	}

	// --- Enemy3 reflected fireballs vs all enemies ---
	{
		int kd;
		if (enemy->isAlive()  && enemy3->checkReflectedFireballHit(enemy->getHitbox(),  kd)) enemy->takeDamage(kd);
		if (enemy2->isAlive() && enemy3->checkReflectedFireballHit(enemy2->getHitbox(), kd)) enemy2->takeDamage(kd);
		if (enemy3->isAlive() && enemy3->checkReflectedFireballHit(enemy3->getHitbox(), kd)) enemy3->takeDamage(kd);
	}

	// --- Enemy3 melee (Attack2) hits player ---
	if (enemy3->isMeleeAttacking() && !enemy3HitPlayerThisSwing)
	{
		if (player->isAlive() && !player->isInvincible())
		{
			glm::vec4 m = enemy3->getMeleeHitbox();
			glm::vec2 pPos = player->getPosition();
			if (m.x < pPos.x + Player::HITBOX_WIDTH  && m.x + m.z > pPos.x &&
				m.y < pPos.y + Player::HITBOX_HEIGHT && m.y + m.w > pPos.y)
			{
				player->takeDamage();
				enemy3HitPlayerThisSwing = true;
			}
		}
	}
	if (!enemy3->isMeleeAttacking()) enemy3HitPlayerThisSwing = false;
	updateCamera();

    glm::vec2 pPos = player->getPosition();
    pSize = glm::ivec2(Player::HITBOX_WIDTH, Player::HITBOX_HEIGHT);

	// Trigger spring animation on activation
	if (player->consumeSpringTrigger()) {
		for (Sprite* spring : springs) {
			if (checkAABB(pPos, pSize, spring->getPosition(), glm::ivec2(map->getTileSize(), map->getTileSize()))) {
				spring->changeAnimation(1);
				break;
			}
		}
	}
	for (Sprite* spring : springs) {
		spring->update(deltaTime);
		if (spring->animationFinished())
			spring->changeAnimation(0);
	}
	for (Sprite* dash : dashes) {
		dash->update(deltaTime);
	}

	// Fall out of bounds death check
	if (pPos.y > map->getMapSize().y * map->getTileSize() + pSize.y) {
		Game::instance().lives--;
		std::cout << "Player fell! Lives remaining: " << Game::instance().lives << std::endl;
		if (Game::instance().lives <= 0)
			return;
		player->setPosition(playerInitPos);
		updateCamera();
		pPos = player->getPosition();
	}

    // un bucle por tipo de item, como solo hay 5 no pasa nada pero si añadimos más habría que crear una clase Item o algo así para no repetir código
    for (int i = keys.size() - 1; i >= 0; i--) {
        if (checkAABB(pPos, pSize, keys[i]->getPosition(), glm::ivec2(map->getTileSize(), map->getTileSize()))) {
            
			Game::instance().collectKeyInCurrentRoom();     
            delete keys[i];                
            keys.erase(keys.begin() + i); 
            std::cout << "KEY PICKED UP" << std::endl;
        }
    }

	if (sword != nullptr && checkAABB(pPos, pSize, sword->getPosition(), glm::ivec2(map->getTileSize(), map->getTileSize()))) {
		
		Game::instance().hasSword = true; 
		Game::instance().collectSwordInCurrentRoom();
		delete sword;
		sword = nullptr;
		std::cout << "GOT THE SWORD" << std::endl;
	}

	for (int i = heals.size() - 1; i >= 0; i--) {
		if (checkAABB(pPos, pSize, heals[i]->getPosition(), glm::ivec2(map->getTileSize(), map->getTileSize()))) {
			if (Game::instance().lives < 9) {
				Game::instance().lives++;
			}
			Game::instance().collectHealInCurrentRoom();
			delete heals[i];
			heals.erase(heals.begin() + i);
			std::cout << "HEAL PICKED UP" << std::endl;
		}
	}

	for (int i = shields.size() - 1; i >= 0; i--) {
		if (checkAABB(pPos, pSize, shields[i]->getPosition(), glm::ivec2(map->getTileSize(), map->getTileSize()))) {
			Game::instance().hasShield = true; 
			Game::instance().collectShieldInCurrentRoom();
			delete shields[i];
			shields.erase(shields.begin() + i);
			std::cout << "SHIELD PICKED UP" << std::endl;
		}
	}

	for (int i = weights.size() - 1; i >= 0; i--) {
		glm::vec2 wPos = weights[i]->getPosition();
		int pushStep = 2;
		glm::ivec2 weightSize(map->getTileSize(), map->getTileSize());
		bool weightDestroyed = false;
		float &wVel = weightVelocities[i];
		int &springCooldownMs = weightSpringCooldownMs[i];
		int &dashCooldownMs = weightDashCooldownMs[i];
		float &dashVel = weightDashVelocities[i];
		float &dashVelStart = weightDashVelocityStarts[i];
		int &dashTimeLeftMs = weightDashTimeLeftMs[i];
		if (springCooldownMs > 0) springCooldownMs -= deltaTime;
		if (dashCooldownMs > 0) dashCooldownMs -= deltaTime;

		if (checkAABB(pPos, pSize, wPos, weightSize)) {
			int newX = static_cast<int>(wPos.x);
			bool pushed = false;
			CollisionDir pushDir = CollisionDir::RIGHT;

			if (Game::instance().getKey(GLFW_KEY_LEFT)) {
				newX -= pushStep;
				pushed = true;
				pushDir = CollisionDir::LEFT;
			}
			else if (Game::instance().getKey(GLFW_KEY_RIGHT)) {
				newX += pushStep;
				pushed = true;
				pushDir = CollisionDir::RIGHT;
			}

			if (pushed) {
				glm::ivec2 weightPos(newX, static_cast<int>(wPos.y));
				if (!map->checkCollision(weightPos, weightSize, pushDir)) {
					weights[i]->setPosition(glm::vec2(float(newX), wPos.y));
					wPos.x = float(newX);
				}
				// Sync the player horizontal speed to avoid clipping into the weight block visually
				if (pushDir == CollisionDir::LEFT) {
					pPos.x = wPos.x + weightSize.x;
				} else {
					pPos.x = wPos.x - pSize.x;
				}
				player->setPosition(pPos);
			}
		}

		glm::ivec2 weightPosI(static_cast<int>(wPos.x), static_cast<int>(wPos.y));
		if (springCooldownMs <= 0 && map->isOnSpring(weightPosI, weightSize)) {
			wVel = -WEIGHT_SPRING_JUMP_VELOCITY;
			springCooldownMs = WEIGHT_SPRING_COOLDOWN_MS;
		}
		bool dashLeft = false;
		if (dashCooldownMs <= 0 && map->isOnDash(weightPosI, weightSize, &dashLeft)) {
			int dir = dashLeft ? -1 : 1;
			dashTimeLeftMs = WEIGHT_DASH_DURATION_MS;
			dashVelStart = dir * (WEIGHT_DASH_DISTANCE_BASE * 3.0f) / (0.5f * float(WEIGHT_DASH_DURATION_MS));
			dashVel = dashVelStart;
			dashCooldownMs = WEIGHT_DASH_COOLDOWN_MS;
		}

		if (dashTimeLeftMs > 0 && dashVel != 0.0f)
		{
			float ratio = float(dashTimeLeftMs) / float(WEIGHT_DASH_DURATION_MS);
			dashVel = dashVelStart * ratio;
			float dashDelta = dashVel * float(deltaTime);
			int dashSteps = int(std::ceil(std::abs(dashDelta)));
			int dashStepDir = (dashDelta < 0.0f) ? -1 : 1;
			for (int step = 0; step < dashSteps; ++step)
			{
				wPos.x += float(dashStepDir);
				glm::ivec2 dashPos(static_cast<int>(wPos.x), static_cast<int>(wPos.y));
				if (map->checkCollision(dashPos, weightSize, dashStepDir < 0 ? CollisionDir::LEFT : CollisionDir::RIGHT, &dashPos.x))
				{
					wPos.x = float(dashPos.x);
					dashTimeLeftMs = 0;
					dashVel = 0.0f;
					break;
				}
			}
			weights[i]->setPosition(wPos);
		}

		if (dashTimeLeftMs > 0)
		{
			dashTimeLeftMs -= deltaTime;
			if (dashTimeLeftMs <= 0)
			{
				dashTimeLeftMs = 0;
				dashVel = 0.0f;
			}
		}

		// Gravity: weights should fall when nothing supports them below
		wVel += WEIGHT_GRAVITY * dt;
		if (wVel > WEIGHT_MAX_FALL_SPEED)
			wVel = WEIGHT_MAX_FALL_SPEED;
		glm::vec2 nextPos = wPos;
		float yDelta = wVel * dt;
		if (yDelta > 0.0f)
		{
			int ySteps = int(std::ceil(yDelta));
			for (int step = 0; step < ySteps; ++step)
			{
				nextPos.y += 1.0f;
				glm::ivec2 fallCheckPos(static_cast<int>(nextPos.x), static_cast<int>(nextPos.y));
				int correctedY = 0;
				if (map->checkCollision(fallCheckPos, weightSize, CollisionDir::DOWN, &correctedY, false)) {
					nextPos.y = float(correctedY);
					wVel = 0.0f;
					break;
				}
			}
		}
		else if (yDelta < 0.0f)
		{
			int ySteps = int(std::ceil(-yDelta));
			for (int step = 0; step < ySteps; ++step)
			{
				nextPos.y -= 1.0f;
				glm::ivec2 riseCheckPos(static_cast<int>(nextPos.x), static_cast<int>(nextPos.y));
				int correctedY = 0;
				if (map->checkCollision(riseCheckPos, weightSize, CollisionDir::UP, &correctedY, false)) {
					nextPos.y = float(correctedY);
					wVel = 0.0f;
					break;
				}
			}
		}
		weights[i]->setPosition(nextPos);
		wPos = nextPos;

		weights[i]->update(deltaTime);
			
		// Weight collision with enemies - reuse wPos and weightSize from above
		glm::vec4 enemyHitbox = enemy->getHitbox();
		if (enemy->isAlive() && checkAABB(wPos, weightSize, glm::vec2(enemyHitbox.x, enemyHitbox.y), glm::ivec2(enemyHitbox.z, enemyHitbox.w))) {
			enemy->takeDamage(0);
			enemy->takeDamage(0);
			enemy->takeDamage(0);
			weightDestroyed = true;
		}
		glm::vec4 enemy2Hitbox = enemy2->getHitbox();
		if (enemy2->isAlive() && checkAABB(wPos, weightSize, glm::vec2(enemy2Hitbox.x, enemy2Hitbox.y), glm::ivec2(enemy2Hitbox.z, enemy2Hitbox.w))) {
			enemy2->takeDamage(0);
			enemy2->takeDamage(0);
			enemy2->takeDamage(0);
			weightDestroyed = true;
		}
		glm::vec4 enemy3Hitbox = enemy3->getHitbox();
		if (enemy3->isAlive() && checkAABB(wPos, weightSize, glm::vec2(enemy3Hitbox.x, enemy3Hitbox.y), glm::ivec2(enemy3Hitbox.z, enemy3Hitbox.w))) {
			enemy3->takeDamage(0);
			enemy3->takeDamage(0);
			enemy3->takeDamage(0);
			weightDestroyed = true;
		}
		if (weightDestroyed) {
			delete weights[i];
			weights.erase(weights.begin() + i);
			weightVelocities.erase(weightVelocities.begin() + i);
			weightSpringCooldownMs.erase(weightSpringCooldownMs.begin() + i);
			weightDashCooldownMs.erase(weightDashCooldownMs.begin() + i);
			weightDashVelocities.erase(weightDashVelocities.begin() + i);
			weightDashVelocityStarts.erase(weightDashVelocityStarts.begin() + i);
			weightDashTimeLeftMs.erase(weightDashTimeLeftMs.begin() + i);
			continue;
		}
	}

    // --- CHECK EXIT DOORS ---
    for (Sprite* portal : portals) {
        // We use a small AABB to make sure Bugs is standing right in front of it
        if (checkAABB(pPos, pSize, portal->getPosition(), glm::ivec2(map->getTileSize(), 2 * map->getTileSize()))) {
            
            // Player pressed UP?
			if (upJustPressed && !transitionPending) {
				if (!Game::instance().canUsePortalsFromCurrentWorld()) {
					cout << "Portal is locked: collect all keys in this map and its connected rooms." << endl;
					continue;
				}
				
				glm::vec2 portPos = portal->getPosition();
				int currentX = Game::instance().currentRoomX;
				int currentY = Game::instance().currentRoomY;
				int targetX = currentX;
				int targetY = currentY;

				// Is the portal on the Right side of the screen? (e.g., X > 800)
				if (portPos.x > SCREEN_WIDTH * 0.8f) {
					targetX = currentX + 1; // Go Right
				}
				// Is it on the Left side?
				else if (portPos.x < SCREEN_WIDTH * 0.2f) {
					targetX = currentX - 1; // Go Left
				}
				// Is it at the Top?
				else if (portPos.y < SCREEN_HEIGHT * 0.2f) {
					targetY = currentY - 1; // Go Up
				}
				// Must be at the Bottom!
				else {
					targetY = currentY + 1; // Go Down
				}

				scheduleTransitionToWorld(targetX, targetY);
				cout << "Teleporting to room: map_" << targetX << "_" << targetY << endl;
			}
        }
		portal ->update(deltaTime); 
    }

    // --- CHECK ROOM DOORS ---
	for (int i = 0; i < doors.size(); i++) {
		Sprite* door = doors[i];

		if (checkAABB(pPos, pSize, door->getPosition(), glm::ivec2(map->getTileSize(), 2 * map->getTileSize()))) {
			if (upJustPressed && !transitionPending) {
				const std::string currentMap = Game::instance().getCurrentMapName();
				Game::DoorLink link;
				if (!Game::instance().getDoorLink(currentMap, i, link))
					continue;

				if (!link.targetMap.empty()) {
					door->changeAnimation(1); // Open door first, swap room after delay

					int targetX = 0;
					int targetY = 0;
					if (parseWorldMapCoords(link.targetMap, targetX, targetY)) {
						scheduleTransitionToWorld(targetX, targetY);
						pendingTargetDoorIndex = link.targetDoorIndex;
						cout << "Returning to world room: map_" << targetX << "_" << targetY << endl;
					}
					else {
						scheduleTransitionToMap(link.targetMap, true, link.targetDoorIndex);
						cout << "Entering side room: " << link.targetMap << endl;
					}
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
	modelview = glm::translate(glm::mat4(1.0f), glm::vec3(-cameraX, -cameraY, 0.f));
	texProgram.setUniformMatrix4f("modelview", modelview);
	texProgram.setUniform2f("texCoordDispl", 0.f, 0.f);
	map->render();
	Sprite::setGlobalRenderOffset(glm::vec2(-cameraX, -cameraY));

	for (Sprite* key : keys) { key->render(); }
	if (sword) sword->render();
	for (Sprite* heal : heals) { heal->render(); }
	for (Sprite* shield : shields) { shield->render(); }
	for (Sprite* weight : weights) { weight->render(); }
	for (Sprite* spring : springs) { spring->render(); }
	for (Sprite* dash : dashes) { dash->render(); }
	for (Sprite* door : doors) { door->render(); }
	for (Sprite* portal : portals) { portal->render(); }

	if (enemy->isAlive() || enemy->isDying())
		enemy->render();
	if (enemy2->isAlive() || enemy2->isDying())
		enemy2->render();
	if (enemy3->isAlive() || enemy3->isDying())
		enemy3->render();
	player->render();
	Sprite::setGlobalRenderOffset(glm::vec2(0.f, 0.f));

	if(hudReady)
	{
		Game &game = Game::instance();
		std::ostringstream line1;
		std::ostringstream line2;
		std::ostringstream line3;

		int collectedKeysInWorld = game.getCollectedKeysInCurrentWorld();
		line1 << "Lives: " << game.lives
		      << "  Keys: " << collectedKeysInWorld << "/" << game.totalKeysInLevel;
		line2 << "Shield: " << (game.hasShield ? "ON" : "OFF")
		      << "  Sword: " << (game.hasSword ? "ON" : "OFF");

		hudText.render(line1.str(), glm::vec2(16.f, 28.f), 24, glm::vec4(1.f, 1.f, 0.85f, 1.f));
		hudText.render(line2.str(), glm::vec2(16.f, 56.f), 22, glm::vec4(0.8f, 1.f, 0.9f, 1.f));

		if (game.godMode) {
			hudText.render("GOD MODE", glm::vec2(16.f, 84.f), 22, glm::vec4(1.f, 0.2f, 0.2f, 1.f));
		}
	}

}

void Scene::initShaders()
{
	Shader vShader, fShader;

	vShader.initFromFile(VERTEX_SHADER, "../shaders/texture.vert");
	if(!vShader.isCompiled())
	{
		cout << "Vertex Shader Error" << endl;
		cout << "" << vShader.log() << endl << endl;
	}
	fShader.initFromFile(FRAGMENT_SHADER, "../shaders/texture.frag");
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



void Scene::renderMenuScreen(int selection)
{
	glm::mat4 modelview = glm::mat4(1.0f);
	texProgram.use();
	texProgram.setUniformMatrix4f("projection", projection);
	texProgram.setUniformMatrix4f("modelview", modelview);
	texProgram.setUniform4f("color", 1.0f, 1.0f, 1.0f, 1.0f);
	
	if (hudReady) {
		glm::vec4 titleColor(1.0f, 0.9f, 0.2f, 1.0f);
		glm::vec4 normalColor(0.8f, 0.8f, 0.8f, 1.0f);
		glm::vec4 selectedColor(1.0f, 1.0f, 0.0f, 1.0f);
		
		hudText.render("2D PLATFORMER GAME", glm::vec2(300.f, 100.f), 36, titleColor);
		
		hudText.render("PLAY", glm::vec2(400.f, 220.f), 32, selection == 0 ? selectedColor : normalColor);
		hudText.render("INSTRUCTIONS", glm::vec2(340.f, 280.f), 32, selection == 1 ? selectedColor : normalColor);
		hudText.render("CREDITS", glm::vec2(380.f, 340.f), 32, selection == 2 ? selectedColor : normalColor);
		
		hudText.render("Use UP/DOWN to navigate, ENTER to select", glm::vec2(220.f, 420.f), 20, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
	}
}

void Scene::renderInstructionsScreen()
{
	glm::mat4 modelview = glm::mat4(1.0f);
	texProgram.use();
	texProgram.setUniformMatrix4f("projection", projection);
	texProgram.setUniformMatrix4f("modelview", modelview);
	texProgram.setUniform4f("color", 1.0f, 1.0f, 1.0f, 1.0f);
	
	if (hudReady) {
		glm::vec4 titleColor(1.0f, 0.9f, 0.2f, 1.0f);
		glm::vec4 textColor(0.9f, 0.9f, 0.9f, 1.0f);
		
		hudText.render("INSTRUCTIONS", glm::vec2(340.f, 60.f), 32, titleColor);
		
		hudText.render("LEFT / RIGHT - Move", glm::vec2(100.f, 140.f), 24, textColor);
		hudText.render("UP - Jump / Climb / Enter doors", glm::vec2(100.f, 180.f), 24, textColor);
		hudText.render("DOWN - Drop through one-way platforms", glm::vec2(100.f, 220.f), 24, textColor);
		hudText.render("SPACE - Attack (requires sword)", glm::vec2(100.f, 260.f), 24, textColor);
		hudText.render("SHIFT - Parry (requires sword)", glm::vec2(100.f, 300.f), 24, textColor);
		hudText.render("ESC - Pause/Menu", glm::vec2(100.f, 340.f), 24, textColor);
		
		hudText.render("Debug shortcuts (PLAY mode):", glm::vec2(100.f, 390.f), 22, glm::vec4(0.7f, 0.7f, 1.0f, 1.0f));
		hudText.render("G - God Mode, K - Get all keys, 1-9 - Jump to level", glm::vec2(120.f, 420.f), 20, textColor);
		
		hudText.render("Press ESC to return to menu", glm::vec2(260.f, 455.f), 20, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
	}
}

void Scene::renderCreditsScreen()
{
	glm::mat4 modelview = glm::mat4(1.0f);
	texProgram.use();
	texProgram.setUniformMatrix4f("projection", projection);
	texProgram.setUniformMatrix4f("modelview", modelview);
	texProgram.setUniform4f("color", 1.0f, 1.0f, 1.0f, 1.0f);
	
	if (hudReady) {
		glm::vec4 titleColor(1.0f, 0.9f, 0.2f, 1.0f);
		glm::vec4 textColor(0.9f, 0.9f, 0.9f, 1.0f);
		
		hudText.render("CREDITS", glm::vec2(400.f, 100.f), 32, titleColor);
		
		hudText.render("2D Platformer Game", glm::vec2(300.f, 200.f), 28, textColor);
		hudText.render("Developed for VJ Course", glm::vec2(260.f, 250.f), 24, textColor);
		hudText.render("2025-26 Q2", glm::vec2(380.f, 290.f), 24, textColor);
		
		hudText.render("Press ESC to return to menu", glm::vec2(260.f, 440.f), 20, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
	}
}
