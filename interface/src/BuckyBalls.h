//
//  BuckyBalls.h
//  hifi
//
//  Created by Philip Rosedale on 1/2/14.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

#ifndef __hifi__BuckyBalls__
#define __hifi__BuckyBalls__

#include <iostream>

#include <glm/glm.hpp>

#include <HandData.h>
#include <SharedUtil.h>

#include "GeometryUtil.h"
#include "InterfaceConfig.h"
#include "Util.h"


const int NUM_BBALLS = 200;

class BuckyBalls {
public:
    BuckyBalls();
    void grab(PalmData& palm, const glm::vec3& fingerTipPosition, glm::quat avatarOrientation, float deltaTime);
    void simulate(float deltaTime);
    void render();

    
private:
 
    glm::vec3 _bballPosition[NUM_BBALLS];
    glm::vec3 _bballVelocity[NUM_BBALLS];
    glm::vec3 _bballColor[NUM_BBALLS];
    float _bballRadius[NUM_BBALLS];
    float _bballColliding[NUM_BBALLS];
    int _bballElement[NUM_BBALLS];
    int _bballIsGrabbed[2];

    
};

#endif /* defined(__hifi__BuckyBalls__) */
