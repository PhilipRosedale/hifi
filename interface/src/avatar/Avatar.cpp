//
//  Avatar.cpp
//  interface
//
//  Created by Philip Rosedale on 9/11/12.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <NodeList.h>
#include <NodeTypes.h>
#include <PacketHeaders.h>
#include <SharedUtil.h>

#include <GeometryUtil.h>

#include "Application.h"
#include "Avatar.h"
#include "DataServerClient.h"
#include "Hand.h"
#include "Head.h"
#include "Menu.h"
#include "Physics.h"
#include "world.h"
#include "devices/OculusManager.h"
#include "ui/TextRenderer.h"

using namespace std;

const bool BALLS_ON = false;
const glm::vec3 DEFAULT_UP_DIRECTION(0.0f, 1.0f, 0.0f);
const float YAW_MAG = 500.0;
const float MY_HAND_HOLDING_PULL = 0.2;
const float YOUR_HAND_HOLDING_PULL = 1.0;
const float BODY_SPRING_DEFAULT_TIGHTNESS = 1000.0f;
const float BODY_SPRING_FORCE = 300.0f;
const float BODY_SPRING_DECAY = 16.0f;
const float COLLISION_RADIUS_SCALAR = 1.2; // pertains to avatar-to-avatar collisions
const float COLLISION_BALL_FORCE = 200.0; // pertains to avatar-to-avatar collisions
const float COLLISION_BODY_FORCE = 30.0; // pertains to avatar-to-avatar collisions
const float HEAD_ROTATION_SCALE = 0.70;
const float HEAD_ROLL_SCALE = 0.40;
const float HEAD_MAX_PITCH = 45;
const float HEAD_MIN_PITCH = -45;
const float HEAD_MAX_YAW = 85;
const float HEAD_MIN_YAW = -85;
const float AVATAR_BRAKING_STRENGTH = 40.0f;
const float MOUSE_RAY_TOUCH_RANGE = 0.01f;
const float FLOATING_HEIGHT = 0.13f;
const bool  USING_HEAD_LEAN = false;
const float LEAN_SENSITIVITY = 0.15;
const float LEAN_MAX = 0.45;
const float LEAN_AVERAGING = 10.0;
const float HEAD_RATE_MAX = 50.f;
const float SKIN_COLOR[] = {1.0, 0.84, 0.66};
const float DARK_SKIN_COLOR[] = {0.9, 0.78, 0.63};
const int   NUM_BODY_CONE_SIDES = 9;
const float CHAT_MESSAGE_SCALE = 0.0015;
const float CHAT_MESSAGE_HEIGHT = 0.1f;

void Avatar::sendAvatarURLsMessage(const QUrl& voxelURL) {
    QByteArray message;
    
    char packetHeader[MAX_PACKET_HEADER_BYTES];
    int numBytesPacketHeader = populateTypeAndVersion((unsigned char*) packetHeader, PACKET_TYPE_AVATAR_URLS);
    
    message.append(packetHeader, numBytesPacketHeader);
    message.append(NodeList::getInstance()->getOwnerUUID().toRfc4122());
    
    QDataStream out(&message, QIODevice::WriteOnly | QIODevice::Append);
    out << voxelURL;
    
    Application::controlledBroadcastToNodes((unsigned char*)message.data(), message.size(), &NODE_TYPE_AVATAR_MIXER, 1);
}

Avatar::Avatar(Node* owningNode) :
    AvatarData(owningNode),
    _head(this),
    _hand(this),
    _skeletonModel(this),
    _ballSpringsInitialized(false),
    _bodyYawDelta(0.0f),
    _mode(AVATAR_MODE_STANDING),
    _velocity(0.0f, 0.0f, 0.0f),
    _thrust(0.0f, 0.0f, 0.0f),
    _speed(0.0f),
    _leanScale(0.5f),
    _pelvisFloatingHeight(0.0f),
    _scale(1.0f),
    _worldUpDirection(DEFAULT_UP_DIRECTION),
    _mouseRayOrigin(0.0f, 0.0f, 0.0f),
    _mouseRayDirection(0.0f, 0.0f, 0.0f),
    _isCollisionsOn(true),
    _leadingAvatar(NULL),
    _moving(false),
    _initialized(false),
    _handHoldingPosition(0.0f, 0.0f, 0.0f),
    _maxArmLength(0.0f),
    _pelvisStandingHeight(0.0f)
{
    // we may have been created in the network thread, but we live in the main thread
    moveToThread(Application::getInstance()->thread());
    
    // give the pointer to our head to inherited _headData variable from AvatarData
    _headData = &_head;
    _handData = &_hand;
    
    _skeleton.initialize();
    
    _height = _skeleton.getHeight();

    _maxArmLength = _skeleton.getArmLength();
    _pelvisStandingHeight = _skeleton.getPelvisStandingHeight();
    _pelvisFloatingHeight = _skeleton.getPelvisFloatingHeight();
    _pelvisToHeadLength = _skeleton.getPelvisToHeadLength();
    
}


Avatar::~Avatar() {
    _headData = NULL;
    _handData = NULL;
}

void Avatar::deleteOrDeleteLater() {
    this->deleteLater();
}


void Avatar::init() {
    _head.init();
    _hand.init();
    _skeletonModel.init();
    _initialized = true;
}

glm::quat Avatar::getOrientation() const {
    return glm::quat(glm::radians(glm::vec3(_bodyPitch, _bodyYaw, _bodyRoll)));
}

glm::quat Avatar::getWorldAlignedOrientation () const {
    return computeRotationFromBodyToWorldUp() * getOrientation();
}

void Avatar::follow(Avatar* leadingAvatar) {
    const float MAX_STRING_LENGTH = 2;

    _leadingAvatar = leadingAvatar;
    if (_leadingAvatar != NULL) {
        _leaderUUID = leadingAvatar->getOwningNode()->getUUID();
        _stringLength = glm::length(_position - _leadingAvatar->getPosition()) / _scale;
        if (_stringLength > MAX_STRING_LENGTH) {
            _stringLength = MAX_STRING_LENGTH;
        }
    } else {
        _leaderUUID = QUuid();
    }
}

void Avatar::simulate(float deltaTime, Transmitter* transmitter) {
    
    if (_leadingAvatar && !_leadingAvatar->getOwningNode()->isAlive()) {
        follow(NULL);
    }
    
    if (_scale != _newScale) {
        setScale(_newScale);
    }
    
    // copy velocity so we can use it later for acceleration
    glm::vec3 oldVelocity = getVelocity();
    
    // update torso rotation based on head lean
    _skeleton.joint[AVATAR_JOINT_TORSO].rotation = glm::quat(glm::radians(glm::vec3(
                                                                                    _head.getLeanForward(), 0.0f, _head.getLeanSideways())));
    
    // apply joint data (if any) to skeleton
    bool enableHandMovement = true;
    for (vector<JointData>::iterator it = _joints.begin(); it != _joints.end(); it++) {
        _skeleton.joint[it->jointID].rotation = it->rotation;
        
        // disable hand movement if we have joint info for the right wrist
        enableHandMovement &= (it->jointID != AVATAR_JOINT_RIGHT_WRIST);
    }
    
    // update avatar skeleton
    _skeleton.update(deltaTime, getOrientation(), _position);
    

    // if this is not my avatar, then hand position comes from transmitted data
    _skeleton.joint[ AVATAR_JOINT_RIGHT_FINGERTIPS ].position = _handPosition;
    
    _hand.simulate(deltaTime, false);
    _skeletonModel.simulate(deltaTime);
    _head.setBodyRotation(glm::vec3(_bodyPitch, _bodyYaw, _bodyRoll));
    glm::vec3 headPosition;
    _skeletonModel.getHeadPosition(headPosition);
    _head.setPosition(headPosition);
    _head.setScale(_scale);
    _head.setSkinColor(glm::vec3(SKIN_COLOR[0], SKIN_COLOR[1], SKIN_COLOR[2]));
    _head.simulate(deltaTime, false);
    
    // use speed and angular velocity to determine walking vs. standing
    if (_speed + fabs(_bodyYawDelta) > 0.2) {
        _mode = AVATAR_MODE_WALKING;
    } else {
        _mode = AVATAR_MODE_INTERACTING;
    }
    
    // update position by velocity, and subtract the change added earlier for gravity
    _position += _velocity * deltaTime;
    
    // Zero thrust out now that we've added it to velocity in this frame
    _thrust = glm::vec3(0, 0, 0);
    
}

void Avatar::setMouseRay(const glm::vec3 &origin, const glm::vec3 &direction) {
    _mouseRayOrigin = origin;
    _mouseRayDirection = direction;
}

static TextRenderer* textRenderer() {
    static TextRenderer* renderer = new TextRenderer(SANS_FONT_FAMILY, 24, -1, false, TextRenderer::SHADOW_EFFECT);
    return renderer;
}

void Avatar::render(bool forceRenderHead) {
    
    {
        // glow when moving in the distance
        glm::vec3 toTarget = _position - Application::getInstance()->getAvatar()->getPosition();
        const float GLOW_DISTANCE = 5.0f;
        Glower glower(_moving && glm::length(toTarget) > GLOW_DISTANCE ? 1.0f : 0.0f);
        
        // render body
        renderBody(forceRenderHead);
    
        // render sphere when far away
        const float MAX_ANGLE = 10.f;
        glm::vec3 delta = _height * (_head.getCameraOrientation() * IDENTITY_UP) / 2.f;
        float angle = abs(angleBetween(toTarget + delta, toTarget - delta));

        if (angle < MAX_ANGLE) {
            glColor4f(0.5f, 0.8f, 0.8f, 1.f - angle / MAX_ANGLE);
            glPushMatrix();
            glTranslatef(_position.x, _position.y, _position.z);
            glScalef(_height / 2.f, _height / 2.f, _height / 2.f);
            glutSolidSphere(1.2f + _head.getAverageLoudness() * .0005f, 20, 20);
            glPopMatrix();
        }
    }
    
   
    if (!_chatMessage.empty()) {
        int width = 0;
        int lastWidth = 0;
        for (string::iterator it = _chatMessage.begin(); it != _chatMessage.end(); it++) {
            width += (lastWidth = textRenderer()->computeWidth(*it));
        }
        glPushMatrix();
        
        glm::vec3 chatPosition = getHead().getEyePosition() + getBodyUpDirection() * CHAT_MESSAGE_HEIGHT * _scale;
        glTranslatef(chatPosition.x, chatPosition.y, chatPosition.z);
        glm::quat chatRotation = Application::getInstance()->getCamera()->getRotation();
        glm::vec3 chatAxis = glm::axis(chatRotation);
        glRotatef(glm::angle(chatRotation), chatAxis.x, chatAxis.y, chatAxis.z);
        
        
        glColor3f(0, 0.8, 0);
        glRotatef(180, 0, 1, 0);
        glRotatef(180, 0, 0, 1);
        glScalef(_scale * CHAT_MESSAGE_SCALE, _scale * CHAT_MESSAGE_SCALE, 1.0f);
        
        glDisable(GL_LIGHTING);
        glDepthMask(false);
        if (_keyState == NO_KEY_DOWN) {
            textRenderer()->draw(-width / 2.0f, 0, _chatMessage.c_str());
            
        } else {
            // rather than using substr and allocating a new string, just replace the last
            // character with a null, then restore it
            int lastIndex = _chatMessage.size() - 1;
            char lastChar = _chatMessage[lastIndex];
            _chatMessage[lastIndex] = '\0';
            textRenderer()->draw(-width / 2.0f, 0, _chatMessage.c_str());
            _chatMessage[lastIndex] = lastChar;
            glColor3f(0, 1, 0);
            textRenderer()->draw(width / 2.0f - lastWidth, 0, _chatMessage.c_str() + lastIndex);
        }
        glEnable(GL_LIGHTING);
        glDepthMask(true);
        
        glPopMatrix();
    }
}

glm::quat Avatar::computeRotationFromBodyToWorldUp(float proportion) const {
    glm::quat orientation = getOrientation();
    glm::vec3 currentUp = orientation * IDENTITY_UP;
    float angle = glm::degrees(acosf(glm::clamp(glm::dot(currentUp, _worldUpDirection), -1.0f, 1.0f)));
    if (angle < EPSILON) {
        return glm::quat();
    }
    glm::vec3 axis;
    if (angle > 179.99f) { // 180 degree rotation; must use another axis
        axis = orientation * IDENTITY_RIGHT;
    } else {
        axis = glm::normalize(glm::cross(currentUp, _worldUpDirection));
    }
    return glm::angleAxis(angle * proportion, axis);
}

void Avatar::renderBody(bool forceRenderHead) {

    if (_head.getVideoFace().isFullFrame()) {
        //  Render the full-frame video
        _head.getVideoFace().render(1.0f);
    } else {
        //  Render the body's voxels and head
        glm::vec3 pos = getPosition();
        //printf("Render other at %.3f, %.2f, %.2f\n", pos.x, pos.y, pos.z);
        _skeletonModel.render(1.0f);
        _head.render(1.0f);
    }
    _hand.render(false);
}

void Avatar::getSkinColors(glm::vec3& lighter, glm::vec3& darker) {
    lighter = glm::vec3(SKIN_COLOR[0], SKIN_COLOR[1], SKIN_COLOR[2]);
    darker = glm::vec3(DARK_SKIN_COLOR[0], DARK_SKIN_COLOR[1], DARK_SKIN_COLOR[2]);
    if (_head.getFaceModel().isActive()) {
        lighter = glm::vec3(_head.getFaceModel().computeAverageColor());
        const float SKIN_DARKENING = 0.9f;
        darker = lighter * SKIN_DARKENING;
    }
}

bool Avatar::findSpherePenetration(const glm::vec3& penetratorCenter, float penetratorRadius,
        glm::vec3& penetration, int skeletonSkipIndex) {
    bool didPenetrate = false;
    glm::vec3 totalPenetration;
    glm::vec3 skeletonPenetration;
    if (_skeletonModel.findSpherePenetration(penetratorCenter, penetratorRadius,
            skeletonPenetration, 1.0f, skeletonSkipIndex)) {
        totalPenetration = addPenetrations(totalPenetration, skeletonPenetration);
        didPenetrate = true; 
    }
    glm::vec3 facePenetration;
    if (_head.getFaceModel().findSpherePenetration(penetratorCenter, penetratorRadius, facePenetration)) {
        totalPenetration = addPenetrations(totalPenetration, facePenetration);
        didPenetrate = true; 
    }
    if (didPenetrate) {
        penetration = totalPenetration;
        return true;
    }
    return false;
}

int Avatar::parseData(unsigned char* sourceBuffer, int numBytes) {
    // change in position implies movement
    glm::vec3 oldPosition = _position;
    int bytesRead = AvatarData::parseData(sourceBuffer, numBytes);
    const float MOVE_DISTANCE_THRESHOLD = 0.001f;
    _moving = glm::distance(oldPosition, _position) > MOVE_DISTANCE_THRESHOLD;
    return bytesRead;
}

// render a makeshift cone section that serves as a body part connecting joint spheres
void Avatar::renderJointConnectingCone(glm::vec3 position1, glm::vec3 position2, float radius1, float radius2) {
    
    glBegin(GL_TRIANGLES);
    
    glm::vec3 axis = position2 - position1;
    float length = glm::length(axis);
    
    if (length > 0.0f) {
        
        axis /= length;
        
        glm::vec3 perpSin = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 perpCos = glm::normalize(glm::cross(axis, perpSin));
        perpSin = glm::cross(perpCos, axis);
        
        float anglea = 0.0;
        float angleb = 0.0;
        
        for (int i = 0; i < NUM_BODY_CONE_SIDES; i ++) {
            
            // the rectangles that comprise the sides of the cone section are
            // referenced by "a" and "b" in one dimension, and "1", and "2" in the other dimension.
            anglea = angleb;
            angleb = ((float)(i+1) / (float)NUM_BODY_CONE_SIDES) * PIf * 2.0f;
            
            float sa = sinf(anglea);
            float sb = sinf(angleb);
            float ca = cosf(anglea);
            float cb = cosf(angleb);
            
            glm::vec3 p1a = position1 + perpSin * sa * radius1 + perpCos * ca * radius1;
            glm::vec3 p1b = position1 + perpSin * sb * radius1 + perpCos * cb * radius1; 
            glm::vec3 p2a = position2 + perpSin * sa * radius2 + perpCos * ca * radius2;   
            glm::vec3 p2b = position2 + perpSin * sb * radius2 + perpCos * cb * radius2;  
            
            glVertex3f(p1a.x, p1a.y, p1a.z); 
            glVertex3f(p1b.x, p1b.y, p1b.z); 
            glVertex3f(p2a.x, p2a.y, p2a.z); 
            glVertex3f(p1b.x, p1b.y, p1b.z); 
            glVertex3f(p2a.x, p2a.y, p2a.z); 
            glVertex3f(p2b.x, p2b.y, p2b.z); 
        }
    }
    
    glEnd();
}

void Avatar::goHome() {
    qDebug("Going Home!\n");
    setPosition(START_LOCATION);
}

void Avatar::increaseSize() {
    if ((1.f + SCALING_RATIO) * _newScale < MAX_SCALE) {
        _newScale *= (1.f + SCALING_RATIO);
        qDebug("Changed scale to %f\n", _newScale);
    }
}

void Avatar::decreaseSize() {
    if (MIN_SCALE < (1.f - SCALING_RATIO) * _newScale) {
        _newScale *= (1.f - SCALING_RATIO);
        qDebug("Changed scale to %f\n", _newScale);
    }
}

void Avatar::resetSize() {
    _newScale = 1.0f;
    qDebug("Reseted scale to %f\n", _newScale);
}

void Avatar::setScale(const float scale) {
    _scale = scale;

    if (_newScale * (1.f - RESCALING_TOLERANCE) < _scale &&
            _scale < _newScale * (1.f + RESCALING_TOLERANCE)) {
        _scale = _newScale;
    }
    
    _skeleton.setScale(_scale);
    
    _height = _skeleton.getHeight();
    
    _maxArmLength = _skeleton.getArmLength();
    _pelvisStandingHeight = _skeleton.getPelvisStandingHeight();
    _pelvisFloatingHeight = _skeleton.getPelvisFloatingHeight();
    _pelvisToHeadLength = _skeleton.getPelvisToHeadLength();
}

