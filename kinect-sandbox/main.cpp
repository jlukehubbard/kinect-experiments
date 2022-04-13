#include <iostream>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl_core_3_3.h"
#include <GL/freeglut.h>
#include "util.hpp"

// Kinect required headers
#include <Windows.h>
#include <Ole2.h>
#include <NuiApi.h>
#include <NuiImageCamera.h>
#include <NuiSensor.h>

using namespace std;
using namespace glm;

// Global state
GLuint shader;			// Shader program
GLuint uniXform;		// Matrix location in shader
GLuint vao;				// Vertex array object
GLuint vbuf;			// Vertex buffer
GLsizei vcount;			// Number of vertices

// Texture data
GLuint colorTex;			// OpenGL texture holding color data
GLuint depthTex;			// OpenGL texture holding depth data
const GLint texWidth = 640;		// Kinect image dimensions
const GLint texHeight = 480;
vector<GLubyte> colorData;	// Color data from kinect
vector<GLubyte> depthData;	// Depth data from kinect
int viewmode;				// 0 = show color, 1 = show depth

// Kinect state
INuiSensor* kinect = NULL;
HANDLE colorStream = INVALID_HANDLE_VALUE;
HANDLE depthStream = INVALID_HANDLE_VALUE;


// Kinect / projector correspondence - manually derived
// These are the texture coordinates of the projected rectangle boundaries
// Use them to fit the projection to real space
float kpLeft = -0.00800092;
float kpRight = 0.932997f;
float kpTop = 0.968996;
float kpBottom = 0.0950001;

// Change this to match your display configuration
int winXOffset = 1920;
const int winWidth = 1920;
const int winHeight = 1080;

// Initialization functions
void initState();
void initGLUT(int* argc, char** argv);
void initOpenGL();
void initQuad();
void initTextures();
void initKinect();

// Kinect functions
void kinectGetColor();
void kinectGetDepth();

// Callback functions
void display();
void reshape(GLint width, GLint height);
void keyPress(unsigned char key, int x, int y);
void keyRelease(unsigned char key, int x, int y);
void mouseBtn(int button, int state, int x, int y);
void mouseMove(int x, int y);
void idle();
void cleanup();

mat4 aspectMtx();
ivec2 winToTex(ivec2 winPos);


int main(int argc, char** argv) {
	try {
		// Initialize
		initState();
		initGLUT(&argc, argv);
		initOpenGL();
		initQuad();
		initTextures();
		initKinect();

	} catch (const exception& e) {
		// Handle any errors
		cerr << "Fatal error: " << e.what() << endl;
		cleanup();
		return -1;
	}

	// Execute main loop
	glutMainLoop();

	return 0;
}

void initState() {
	// Initialize global state
	shader = 0;
	uniXform = 0;
	vao = 0;
	vbuf = 0;
	vcount = 0;
	colorTex = 0;
	depthTex = 0;
	viewmode = 1;	// Draw depth
}

void initGLUT(int* argc, char** argv) {
	// Set window and context settings
	glutInit(argc, argv);
	glutInitWindowSize(winWidth, winHeight);
	glutInitWindowPosition(winXOffset, 0);
	glutInitContextVersion(3, 3);
	glutInitContextFlags(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE | GLUT_BORDERLESS | GLUT_CAPTIONLESS);
	// Create the window
	glutCreateWindow("Kinect demo: press space to switch view modes");

	// GLUT callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyPress);
	glutKeyboardUpFunc(keyRelease);
	glutMouseFunc(mouseBtn);
	glutMotionFunc(mouseMove);
	glutIdleFunc(idle);
	glutCloseFunc(cleanup);
}

void initOpenGL() {
	// Set clear color and depth
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	// Set viewport
	glViewport(0, 0, winWidth, winHeight);

	// Compile and link shader program
	vector<GLuint> shaders;
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f.glsl"));
	shader = linkProgram(shaders);
	// Release shader sources
	for (auto s = shaders.begin(); s != shaders.end(); ++s)
		glDeleteShader(*s);
	shaders.clear();
	// Locate uniforms
	uniXform = glGetUniformLocation(shader, "xform");

	// Bind texture image unit
	glUseProgram(shader);
	GLuint texLoc = glGetUniformLocation(shader, "tex");
	glUniform1i(texLoc, 0);
	glUseProgram(0);
}

void initQuad() {
	// Create a textured quad
	struct vert {
		vert(vec2 p, vec2 u) : pos(p), uv(u) {}
		vec2 pos;	// Vertex position
		vec2 uv;	// Texture coordinates
	};
	vector<vert> verts;
	verts.push_back(vert(vec2(-1.0f, -1.0f), vec2(0.0, 0.0)));
	verts.push_back(vert(vec2( 1.0f, -1.0f), vec2(1.0, 0.0)));
	verts.push_back(vert(vec2( 1.0f,  1.0f), vec2(1.0, 1.0)));
	verts.push_back(vert(vec2( 1.0f,  1.0f), vec2(1.0, 1.0)));
	verts.push_back(vert(vec2(-1.0f,  1.0f), vec2(0.0, 1.0)));
	verts.push_back(vert(vec2(-1.0f, -1.0f), vec2(0.0, 0.0)));
	vcount = verts.size();

	// Create vertex array object
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Create vertex buffer
	glGenBuffers(1, &vbuf);
	glBindBuffer(GL_ARRAY_BUFFER, vbuf);
	glBufferData(GL_ARRAY_BUFFER, vcount * sizeof(vert), verts.data(), GL_STATIC_DRAW);
	// Specify vertex attributes
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vert), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vert), (GLvoid*)sizeof(vec2));

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void initTextures() {
	// Create textures to hold kinect data
	colorData = vector<GLubyte>(texWidth * texHeight * 4, 255);
	depthData = vector<GLubyte>(texWidth * texHeight, 255);

	glGenTextures(1, &colorTex);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, colorData.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &depthTex);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, GL_RED, GL_UNSIGNED_BYTE, depthData.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void initKinect() {
	// Count the Kinects connected to the PC
	HRESULT hr;
	int sensorCount = 0;
	hr = NuiGetSensorCount(&sensorCount);
	if (FAILED(hr)) throw runtime_error("No connected Kinects!");

	// Check all the Kinects connected to the PC
	for (int i = 0; i < sensorCount; i++) {
		hr = NuiCreateSensorByIndex(i, &kinect);
		if (FAILED(hr)) continue;

		// Check if we have a good connection
		hr = kinect->NuiStatus();
		if (hr == S_OK) break;	// No need to check further Kinects

		// Release the Kinect
		kinect->Release();
	}

	// Fail if no Kinect was good
	if (kinect == NULL) throw runtime_error("Could not connect to Kinect!");

	// Set the initialization flags for depth and color images
	hr = kinect->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH | NUI_INITIALIZE_FLAG_USES_COLOR);
	if (!SUCCEEDED(hr)) throw runtime_error("Failed to open Kinect streams!");

	// Create data streams for depth and color
	kinect->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,
		NUI_IMAGE_RESOLUTION_640x480,
		0, 2, NULL, &colorStream);
	kinect->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_DEPTH,
		NUI_IMAGE_RESOLUTION_640x480,
		0, 2, NULL, &depthStream);
}

// Get a color image from the Kinect
void kinectGetColor() {
	NUI_IMAGE_FRAME imageFrame;
	NUI_LOCKED_RECT lockedRect;

	if (kinect->NuiImageStreamGetNextFrame(colorStream, 0, &imageFrame) < 0) return;

	INuiFrameTexture* texture = imageFrame.pFrameTexture;
	texture->LockRect(0, &lockedRect, NULL, 0);

	if (lockedRect.Pitch != 0) {
		const BYTE* curr = (const BYTE*) lockedRect.pBits;
		const BYTE* dataEnd = curr + (texWidth * texHeight * 4);

		GLubyte* dest = colorData.data();
		while (curr < dataEnd) {
			*dest++ = *curr++;
		}
	}

	texture->UnlockRect(0);
	kinect->NuiImageStreamReleaseFrame(colorStream, &imageFrame);

	// Upload texture to OpenGL
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, colorData.data());
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Get a depth image from the Kinect
void kinectGetDepth() {
	HRESULT hr;
	NUI_IMAGE_FRAME imageFrame;

	// Attempt to get the depth frame
	hr = kinect->NuiImageStreamGetNextFrame(depthStream, 0, &imageFrame);
	if (FAILED(hr)) return;

	BOOL nearMode;
	INuiFrameTexture* pTexture;

	// Get the depth image pixel texture
	hr = kinect->NuiImageFrameGetDepthImagePixelFrameTexture(depthStream, &imageFrame, &nearMode, &pTexture);
	if (FAILED(hr)) {
		// Release the frame
		kinect->NuiImageStreamReleaseFrame(depthStream, &imageFrame);
		return;
	}

	NUI_LOCKED_RECT lockedRect;

	// Lock the frame data
	pTexture->LockRect(0, &lockedRect, NULL, 0);

	// Make sure we're receiving valid data
	if (lockedRect.Pitch != 0) {
		// Get the min and max reliable depth for the current frame
		int minDepth = (nearMode ? NUI_IMAGE_DEPTH_MINIMUM_NEAR_MODE : NUI_IMAGE_DEPTH_MINIMUM) >> NUI_IMAGE_PLAYER_INDEX_SHIFT;
		int maxDepth = (nearMode ? NUI_IMAGE_DEPTH_MAXIMUM_NEAR_MODE : NUI_IMAGE_DEPTH_MAXIMUM) >> NUI_IMAGE_PLAYER_INDEX_SHIFT;

		const NUI_DEPTH_IMAGE_PIXEL* pBufferRun = reinterpret_cast<const NUI_DEPTH_IMAGE_PIXEL*>(lockedRect.pBits);
		const NUI_DEPTH_IMAGE_PIXEL* pBufferEnd = pBufferRun + (texWidth * texHeight);

		int i = 0;
		while (pBufferRun < pBufferEnd) {
			USHORT depth = pBufferRun->depth;
			// Convert the depth from a short to a byte by discarding the 8 most significant bits.
			// This preserves precision but results in intensity "wrapping" when outside the range
			depthData[i++] = (GLubyte)(depth >= minDepth && depth <= maxDepth ? (depth - 32) % 256 : 0);
			pBufferRun++;
		}
	}

	// Unlock the rect
	pTexture->UnlockRect(0);
	pTexture->Release();

	// Release the frame
	kinect->NuiImageStreamReleaseFrame(depthStream, &imageFrame);

	// Upload texture data to OpenGL
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, texWidth, texHeight, 0, GL_RED, GL_UNSIGNED_BYTE, depthData.data());
	glBindTexture(GL_TEXTURE_2D, 0);
}

void display() {
	try {
		// Get image from Kinect
		if (viewmode == 0)
			kinectGetColor();
		else
			kinectGetDepth();

		// Clear the back buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Get ready to draw
		glUseProgram(shader);
		glBindVertexArray(vao);

		// Activate texture
		glActiveTexture(GL_TEXTURE0 + 0);
		if (viewmode == 0)
			glBindTexture(GL_TEXTURE_2D, colorTex);
		else
			glBindTexture(GL_TEXTURE_2D, depthTex);

		// Transform texture coordinates to fit projection to sandbox
		mat4 xform(1.0f);
		xform [0][0] = kpRight - kpLeft;
		xform [3][0] = kpLeft;
		xform [1][1] = kpTop - kpBottom;
		xform [3][1] = kpBottom;
		// Send transformation matrix to shader
		glUniformMatrix4fv(uniXform, 1, GL_FALSE, value_ptr(xform));
		GLuint viewmodeLoc = glGetUniformLocation(shader, "viewmode");
		glUniform1i(viewmodeLoc, viewmode);

		// Draw the textured quad
		glDrawArrays(GL_TRIANGLES, 0, vcount);

		// Revert context state
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);
		glUseProgram(0);

		// Display the back buffer
		glutSwapBuffers();

	} catch (const exception& e) {
		cerr << "Fatal error: " << e.what() << endl;
		glutLeaveMainLoop();
	}
}

void reshape(GLint width, GLint height) {
	// Prevent resizing
	glutReshapeWindow(winWidth, winHeight);
}

void keyPress(unsigned char key, int x, int y) {
	float deltaCoarse = 0.05f;
	float deltaFine = 0.001f;
	static bool fine = false;
	float delta = fine ? deltaFine : deltaCoarse;
	switch (key) {
	case 'q':
	case 'Q':
		fine = !fine;
		break;
	case 'a':
		kpLeft += delta;
		cout << "kpLeft: " << kpLeft << endl;
		glutPostRedisplay();
		break;
	case 'A':
		kpLeft -= delta;
		cout << "kpLeft: " << kpLeft << endl;
		glutPostRedisplay();
		break;
	case 's':
		kpBottom += delta;
		cout << "kpBottom: " << kpBottom << endl;
		glutPostRedisplay();
		break;
	case 'S':
		kpBottom -= delta;
		cout << "kpBottom: " << kpBottom << endl;
		glutPostRedisplay();
		break;
	case 'd':
		kpRight += delta;
		cout << "kpRight: " << kpRight << endl;
		glutPostRedisplay();
		break;
	case 'D':
		kpRight -= delta;
		cout << "kpRight: " << kpRight << endl;
		glutPostRedisplay();
		break;
	case 'w':
		kpTop += delta;
		cout << "kpTop: " << kpTop << endl;
		glutPostRedisplay();
		break;
	case 'W':
		kpTop -= delta;
		cout << "kpTop: " << kpTop << endl;
		glutPostRedisplay();
		break;
	}
}

void keyRelease(unsigned char key, int x, int y) {
	switch (key) {
	case 27:	// Escape key - quit program
		glutLeaveMainLoop();
		break;
	case ' ':
		viewmode = !viewmode;
		break;
	}
}

void mouseBtn(int button, int state, int x, int y) {}
void mouseMove(int x, int y) {}

void idle() {
	glutPostRedisplay();
}

void cleanup() {
	// Release all resources
	if (shader) { glDeleteProgram(shader); shader = 0; }
	uniXform = 0;
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbuf) { glDeleteBuffers(1, &vbuf); vbuf = 0; }
	vcount = 0;
	if (colorTex) { glDeleteTextures(1, &colorTex); colorTex = 0; }
	if (depthTex) { glDeleteTextures(1, &depthTex); depthTex = 0; }
	colorData.clear();
	depthData.clear();
}

mat4 aspectMtx() {
	// Returns a matrix that preserves texture aspect ratio regardless of window size
	mat4 xform(1.0f);
	float winAspect = (float)winWidth / (float)winHeight;
	float texAspect = (float)texWidth / (float)texHeight;
	xform [0] [0] = std::min(texAspect / winAspect, 1.0f);
	xform [1] [1] = std::min(winAspect / texAspect, 1.0f);
	return xform;
}

// Converts window pixel coordinates into texture pixel coordinates
ivec2 winToTex(ivec2 winPos) {
	// Get aspect correction matrix
	mat4 xform = aspectMtx();

	// Transform window pixel coordinates into clip-space coordinates
	vec2 clipPos(2.0f * winPos.x / (float)winWidth - 1.0f, 2.0f * (winHeight - winPos.y) / (float)winHeight - 1.0f);
	// Invert aspect correction to get quad coordinates
	vec2 quadPos = vec2(inverse(xform) * vec4(clipPos, 0.0f, 1.0f));
	// Convert quad coordinates into texture pixel coordinates
	ivec2 texPos = ivec2(((quadPos + vec2(1.0f)) / vec2(2.0f)) * vec2(texWidth, texHeight));

	return texPos;
}
