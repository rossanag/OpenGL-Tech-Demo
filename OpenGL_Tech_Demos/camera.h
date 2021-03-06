#ifndef CAMERA_H
#define CAMERA_H

#include <math.h>
#include "Math\vector3.h"
#include "Math\matrix4.h"
#include "Math\MathHelper.h"
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

using namespace std;

/*
	This Camera does not allow pich value higher than 90 degress.
	Will be good enough for a simple camera to look around in scene.
*/
class Camera
{
public:
	Camera(vector3 &position, vector3 &worldUp = vector3(0.0f, 1.0f, 0.0f), GLfloat yaw = -90.0f,
		GLfloat pitch = 0.0f, GLfloat speed = 2.0f, GLfloat sensitivity = 0.1f, GLfloat zoom = 45.0f);

	vector3 position;
	Matrix4 view;
	GLfloat deltaTime;
	GLfloat cameraSpeed, movementSpeed;
	GLfloat zoom;

	// Returns the view martrix calculated using Euler Angles and the LookAt matrix
	Matrix4 &GetViewMatrix();

	void KeyboardMovement(bool keys[], GLfloat deltaTime);
	void ControllerMovement();
	void MouseMovement(GLfloat xOffset, GLfloat yOffset);

private:
	vector3 frontVec;
	vector3 upVec;
	vector3 rightVec;
	vector3 worldUp;

	GLfloat yaw;
	GLfloat pitch;

	GLfloat mouseSensitivity;

	void Roll(GLfloat angle);
	void UpdateCameraVectors();
};

#endif