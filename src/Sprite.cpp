#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Sprite.h"

glm::vec2 Sprite::globalRenderOffset = glm::vec2(0.f, 0.f);


Sprite *Sprite::createSprite(const glm::vec2 &quadSize, const glm::vec2 &sizeInSpritesheet, Texture *spritesheet, ShaderProgram *program)
{
	Sprite *quad = new Sprite(quadSize, sizeInSpritesheet, spritesheet, program);

	return quad;
}


Sprite::Sprite(const glm::vec2 &quadSize, const glm::vec2 &sizeInSpritesheet, Texture *spritesheet, ShaderProgram *program)
{
	this->quadSize = quadSize;
	float vertices[24] = {0.f, 0.f, 0.f, 0.f, 
												quadSize.x, 0.f, sizeInSpritesheet.x, 0.f, 
												quadSize.x, quadSize.y, sizeInSpritesheet.x, sizeInSpritesheet.y, 
												0.f, 0.f, 0.f, 0.f, 
												quadSize.x, quadSize.y, sizeInSpritesheet.x, sizeInSpritesheet.y, 
												0.f, quadSize.y, 0.f, sizeInSpritesheet.y};

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), vertices, GL_STATIC_DRAW);
	posLocation = program->bindVertexAttribute("position", 2, 4*sizeof(float), 0);
	texCoordLocation = program->bindVertexAttribute("texCoord", 2, 4*sizeof(float), (void *)(2*sizeof(float)));
	texture = spritesheet;
	shaderProgram = program;
	currentAnimation = -1;
	position = glm::vec2(0.f);
	flipHorizontal = false;
}

Sprite::~Sprite()
{
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
}

void Sprite::update(int deltaTime)
{
	if(currentAnimation >= 0)
	{
		if(animations[currentAnimation].keyframeDispl.empty())
			return;

		timeAnimation += deltaTime;
		while(timeAnimation > animations[currentAnimation].millisecsPerKeyframe)
		{
			if(animations[currentAnimation].loop)
			{
				timeAnimation -= animations[currentAnimation].millisecsPerKeyframe;
				currentKeyframe = (currentKeyframe + 1) % animations[currentAnimation].keyframeDispl.size();
			}
			else if(currentKeyframe + 1 < int(animations[currentAnimation].keyframeDispl.size()))
			{
				timeAnimation -= animations[currentAnimation].millisecsPerKeyframe;
				currentKeyframe += 1;
			}
			else
				break;
		}
		texCoordDispl = animations[currentAnimation].keyframeDispl[currentKeyframe];
	}
}

void Sprite::render() const
{
	glm::mat4 modelview = glm::translate(glm::mat4(1.0f), glm::vec3(position.x + globalRenderOffset.x, position.y + globalRenderOffset.y, 0.f));
	if(flipHorizontal)
	{
		modelview = glm::translate(modelview, glm::vec3(quadSize.x, 0.f, 0.f));
		modelview = glm::scale(modelview, glm::vec3(-1.f, 1.f, 1.f));
	}
	shaderProgram->setUniformMatrix4f("modelview", modelview);
	shaderProgram->setUniform2f("texCoordDispl", texCoordDispl.x, texCoordDispl.y);
	glEnable(GL_TEXTURE_2D);
	texture->use();
	glBindVertexArray(vao);
	glEnableVertexAttribArray(posLocation);
	glEnableVertexAttribArray(texCoordLocation);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisable(GL_TEXTURE_2D);
}

void Sprite::free()
{
	glDeleteBuffers(1, &vbo);
}

void Sprite::setNumberAnimations(int nAnimations)
{
	animations.clear();
	animations.resize(nAnimations);
}

void Sprite::setAnimationSpeed(int animId, int keyframesPerSec)
{
	if(animId < int(animations.size()))
		animations[animId].millisecsPerKeyframe = 1000.f / keyframesPerSec;
}

void Sprite::setAnimationLoop(int animId, bool loop)
{
	if(animId < int(animations.size()))
		animations[animId].loop = loop;
}

void Sprite::addKeyframe(int animId, const glm::vec2 &displacement)
{
	if(animId < int(animations.size()))
		animations[animId].keyframeDispl.push_back(displacement);
}

void Sprite::changeAnimation(int animId)
{
	if(animId < int(animations.size()))
	{
		currentAnimation = animId;
		currentKeyframe = 0;
		timeAnimation = 0.f;
		texCoordDispl = animations[animId].keyframeDispl[0];
	}
}

int Sprite::animation() const
{
	return currentAnimation;
}

bool Sprite::animationFinished() const
{
	if(currentAnimation < 0)
		return true;
	if(animations[currentAnimation].loop)
		return false;
	// Only finished after the last keyframe has been displayed for 2x its normal duration
	return currentKeyframe >= int(animations[currentAnimation].keyframeDispl.size()) - 1
	       && timeAnimation >= animations[currentAnimation].millisecsPerKeyframe * 2.f;
}

void Sprite::setFlipHorizontal(bool flip)
{
	flipHorizontal = flip;
}

void Sprite::setPosition(const glm::vec2 &pos)
{
	position = pos;
}

void Sprite::setGlobalRenderOffset(const glm::vec2 &offset)
{
	globalRenderOffset = offset;
}



