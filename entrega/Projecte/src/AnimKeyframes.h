#ifndef _ANIMATION_KEYFRAMES
#define _ANIMATION_KEYFRAMES


#include <vector>


using namespace std;


// Datos de una animacion: velocidad y coordenadas UV de cada frame.


struct AnimKeyframes
{
	float millisecsPerKeyframe;
	bool loop = true;
	vector<glm::vec2> keyframeDispl;
};


#endif // _ANIMATION_KEYFRAMES


