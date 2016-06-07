#include "HDR_Demo.h"

HDR_DEMO::HDR_DEMO()
{

}

HDR_DEMO::~HDR_DEMO()
{

}

/*
	- Scale up room.
		-Don't know about this.
	- Fix saturation on the room walls.
*/

void HDR_DEMO::InitalizeScene(GLsizei screenWidth, GLsizei screenHeight)
{
	//glEnable(GL_DEPTH_TEST);

	// Light sources
	// - Positions
	lightPositions.push_back(vector3(0.0f, 0.0f, 49.5f));
	lightPositions.push_back(vector3(-1.4f, -1.9f, 19.0f));
	lightPositions.push_back(vector3(0.0f, -1.8f, 30.0f));
	lightPositions.push_back(vector3(0.8f, -1.7f, 8.0f));
	// - Colors
	lightColors.push_back(glm::vec3(200.0f, 200.0f, 200.0f));
	lightColors.push_back(glm::vec3(0.0f, 0.0f, 50.0f));
	lightColors.push_back(glm::vec3(50.0f, 0.0f, 0.0f));
	lightColors.push_back(glm::vec3(0.0f, 50.0f, 0.0f));

	// Load textures
	woodTexture = LoadTexture("Resources/brickwall.jpg");
	endTexture = LoadTexture("Resources/theWitness.jpg");

	SetupBuffers(screenWidth, screenHeight);

	// Shaders
	shaderBlinnPhong.InitShader("Shaders/HDR_Demo/BlinnPhong.vert", "bloom.frag");
	shaderLight.InitShader("bloom.vert", "light_box.frag");
	shaderBlur.InitShader("blur.vert", "blur.frag");
	shaderHDR.InitShader("Shaders/HDR_Demo/HDR.vert", "Shaders/HDR_Demo/HDR.frag");

	// Set up bloom shader samplers.
	shaderHDR.Use();
	// Number corresponds to the texture unit.
	glUniform1i(glGetUniformLocation(shaderHDR.Program, "scene"), 0);
	glUniform1i(glGetUniformLocation(shaderHDR.Program, "bloomBlur"), 1);
}

// NOTE(Darren): Should i have a set up buffers for all demos, really need to
// organise my demos.
// TODO(Darren): Create the final colors buffers and depth buffers for both hdr and bloom.
void HDR_DEMO::SetupBuffers(GLsizei screenWidth, GLsizei screenHeight)
{
	// Set up floating point framebuffer to render scene to
	glGenFramebuffers(1, &hdrFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	// TODO(Darren): Consistency with comments.
	// - Create 2 floating point color buffers (1 for normal rendering, other for brightness treshold values)
	glGenTextures(2	, colorBuffers);
	for (GLuint i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screenWidth, screenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// Clamp to the edge as the blur filter would otherwise sample repeated texture values.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// Attach texture to framebuffer.
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
	}
	// - Create depth buffer (renderbuffer)
	glGenRenderbuffers(1, &rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

	// - Tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
	GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);
	// - Finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "Framebuffer not complete!" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Ping pong framebuffer for blurring
	glGenFramebuffers(2, pingpongFBO);
	glGenTextures(2, pingpongColorbuffers);
	for (GLuint i = 0; i < 2; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
		glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
		// Also check if framebuffers are complete (no need for depth buffer)
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "Framebuffer not complete!" << std::endl;
	}
}

void HDR_DEMO::UpdateScene(Camera &camera, GLsizei screenWidth, GLsizei screenHeight, bool resized)
{
	camera.ControllerMovement();

	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

	if (resized)
		SetupBuffers(screenWidth, screenHeight);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Camera collision with in the tunnel.
	// ---

	// 1. Render scene into floating point framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glm::mat4 projection = glm::perspective(camera.zoom, (GLfloat)screenWidth / (GLfloat)screenHeight, 0.1f, 500.0f);
	Matrix4 _projection;
	_projection = _projection.perspectiveProjection(camera.zoom, (GLfloat)screenWidth / (GLfloat)screenHeight, 0.1f, 500.0f);
	Matrix4 view = camera.GetViewMatrix();
	Matrix4 model;
	shaderBlinnPhong.Use();
	glUniformMatrix4fv(glGetUniformLocation(shaderBlinnPhong.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
	glUniformMatrix4fv(glGetUniformLocation(shaderBlinnPhong.Program, "view"), 1, GL_FALSE, &view.data[0]);

	// - set lighting uniforms
	for (GLuint i = 0; i < lightPositions.size(); i++)
	{
		float lightPosData[] = {lightPositions[i].x, lightPositions[i].y, lightPositions[i].z};
		glUniform3fv(glGetUniformLocation(shaderBlinnPhong.Program, ("lights[" + std::to_string(i) + "].Position").c_str()), 1, &lightPosData[0]);
		glUniform3fv(glGetUniformLocation(shaderBlinnPhong.Program, ("lights[" + std::to_string(i) + "].Color").c_str()), 1, &lightColors[i][0]);
	}
	float data[] = { camera.position.x, camera.position.y, camera.position.z };
	glUniform3fv(glGetUniformLocation(shaderBlinnPhong.Program, "viewPos"), 1, &data[0]);

	// - render tunnel
	model = Matrix4();
	model = model.translate(vector3(0.0f, 0.0f, 25.0f));
	model = model.scale(vector3(5.0f, 5.0f, 55.0f));
	glUniformMatrix4fv(glGetUniformLocation(shaderBlinnPhong.Program, "model"), 1, GL_FALSE, &model.data[0]);
	glUniform1i(glGetUniformLocation(shaderBlinnPhong.Program, "inverse_normals"), GL_TRUE);
	RenderCube(5.0f, 5.0f, 55.0f);

	// Show all the light sources as bright cubes
	if (renderLights)
	{
		shaderLight.Use();
		glUniformMatrix4fv(glGetUniformLocation(shaderLight.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		glUniformMatrix4fv(glGetUniformLocation(shaderLight.Program, "view"), 1, GL_FALSE, &view.data[0]);
		for (GLuint i = 0; i < lightPositions.size(); i++)
		{
			model = Matrix4();
			model = model.translate(vector3(lightPositions[i]));
			model = model.scale(vector3(0.5f, 0.5f, 0.5f));
			glUniformMatrix4fv(glGetUniformLocation(shaderLight.Program, "model"), 1, GL_FALSE, &model.data[0]);
			glUniform3fv(glGetUniformLocation(shaderLight.Program, "lightColor"), 1, &lightColors[i][0]);
			RenderCube(0.5f, 0.5f, 0.5f);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Blur bright fragments with two-pass Gaussian Blur 
	GLboolean horizontal = true, first_iteration = true;
	GLuint amount = 10;
	shaderBlur.Use();
	for (GLuint i = 0; i < amount; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
		glUniform1i(glGetUniformLocation(shaderBlur.Program, "horizontal"), horizontal);
		// Bind texture of other framebuffer (or scene if first iteration).
		glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
		RenderQuad();
		horizontal = !horizontal;
		if (first_iteration)
			first_iteration = false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// 2. Now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	shaderHDR.Use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
	// TODO(Darren): May create a bool here.
	glUniform1i(glGetUniformLocation(shaderHDR.Program, "hdr"), true);
	glUniform1i(glGetUniformLocation(shaderHDR.Program, "bloom"), true);
	glUniform1f(glGetUniformLocation(shaderHDR.Program, "exposure"), exposure);
	RenderQuad();
}

GLuint HDR_DEMO::LoadTexture(GLchar *path)
{
	// Generate a texture ID and load texture data
	GLuint textureID;
	glGenTextures(1, &textureID);
	int width, height;
	unsigned char* image = SOIL_load_image(path, &width, &height, 0, SOIL_LOAD_RGB);
	if (!image)
		std::cout << "ERROR:: Image was not loaded!" << std::endl;
	// Assign texture to ID
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);

	// Parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	SOIL_free_image_data(image);

	return textureID;
}

// RenderQuad() Renders a 1x1 quad in NDC, best used for framebuffer color targets
// and post-processing effects.
void HDR_DEMO::RenderQuad()
{
	if (quadVAO == 0)
	{
		GLfloat quadVertices[] = {
			// Positions        // Texture Coords
			-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};

		// Setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
	}
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}


// RenderCube() Renders a 1x1 3D cube in NDC. (Not anymore) MWahahaha.
void HDR_DEMO::RenderCube(GLfloat xScale, GLfloat yScale, GLfloat zScale)
{
	// Initialize (if necessary)
	if (cubeVAO == 0)
	{
		GLfloat xyRatio = xScale / yScale;
		GLfloat zyRatio = zScale / yScale;

		GLfloat vertices[] = {
			// Back face
			-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, // Bottom-left
			0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, // top-right
			0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,  // top-right
			-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,  // bottom-left
			-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,// top-left
			// Front face
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom-left
			0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // bottom-right
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // top-right
			0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // top-right
			-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // top-left
			-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  // bottom-left
			// Left face
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-right
			-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, zyRatio, // top-left
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, zyRatio,  // bottom-left
			-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, zyRatio, // bottom-left
			-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  // bottom-right
			-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-right
			// Right face
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, // top-left
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, zyRatio, // bottom-right
			0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, zyRatio, // top-right         
			0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, zyRatio,  // bottom-right
			0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,  // top-left
			0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // bottom-left     
			// Bottom face
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, zyRatio, // top-right
			0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, zyRatio, // top-left
			0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,// bottom-left
			0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, // bottom-left
			-0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, // bottom-right
			-0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, zyRatio, // top-right
			// Top face
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, zyRatio,// top-left
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
			0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, zyRatio, // top-right     
			0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom-right
			-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, zyRatio,// top-left
			-0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// Fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// Link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(6 * sizeof(GLfloat)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// Render Cube
	glBindVertexArray(cubeVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, woodTexture);
	glDrawArrays(GL_TRIANGLES, 0, 24);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, endTexture);
	glDrawArrays(GL_TRIANGLES, 6, 12);
	glBindTexture(GL_TEXTURE_2D, 0);
	// TODO(Darren): Prevent stretching of wood textures.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, woodTexture);
	glDrawArrays(GL_TRIANGLES, 12, 36);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindVertexArray(0);
}