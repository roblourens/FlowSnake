#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL\GL.h>
#include <math.h>  // sqrt
#include <stdio.h> // _vsnwprintf_s. Can disable for Release
#include "glext.h" // glGenBuffers, glBindBuffers, ...


/********** Defines *******************************/
#define countof(x) (sizeof(x)/sizeof(x[0])) // Defined in stdlib, but define here to avoid the header cost
#ifdef _DEBUG
#	define IFC(x) { if (FAILED(hr = x)) { char buf[256]; sprintf_s(buf, "IFC Failed at Line %u\n", __LINE__); OutputDebugString(buf); goto Cleanup; } }
#	define ASSERT(x) if (!(x)) { OutputDebugString("Assert Failed!\n"); DebugBreak(); }
#else
#	define IFC(x) {if (FAILED(hr = x)) { goto Cleanup; }}
#	define ASSERT(x)
#endif

#define uint UINT

#define E_NOTARGETS 0x8000000f

/********** Function Declarations *****************/
LRESULT WINAPI MsgHandler(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam);
void Error(const char* pStr, ...);
float rand();

PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLDELETEPROGRAMPROC glDeleteProgram;
PFNGLUSEPROGRAMPROC	   glUseProgram;  
PFNGLATTACHSHADERPROC  glAttachShader;  
PFNGLLINKPROGRAMPROC   glLinkProgram;   
PFNGLGETPROGRAMIVPROC  glGetProgramiv;  
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATESHADERPROC glCreateShader; 
PFNGLDELETESHADERPROC glDeleteShader; 
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;

/********** Structure Definitions******************/
struct float2
{
	float x;
	float y;

	float getLength()
	{
		if (x == 0 && y == 0)
			return 0;
		else
			return sqrt(x*x + y*y);
	}

	float2 operator- (float2 a)
	{
		float2 ret = {x - a.x, y - a.y};
		return ret;
	}

	float2 operator+ (float2 a)
	{
		float2 ret = {x + a.x, y + a.y};
		return ret;
	}

	float2 operator/ (float a)
	{
		float2 ret = {x/a, y/a};
		return ret;
	}
	
	float2 operator* (float a)
	{
		float2 ret = {x*a, y*a};
		return ret;
	}
};

/********** Global Constants***********************/
const uint g_numVerts = 500;
const float g_tailDist = 0.001f;
const float g_speed = 0.1f; // in Screens per second

/********** Globals Variables *********************/
// Use SOA instead of AOS from optimal cache usage
// We'll traverse each array linearly in each stage of the algorithm
float2 g_positions[g_numVerts] = {};
float2 g_vectors[g_numVerts] = {}; // vectors from a node to its target
short  g_targets[g_numVerts] = {};
short  g_tails[g_numVerts] = {};
bool   g_hasParent[g_numVerts] = {};
bool   g_hasChild[g_numVerts] = {};

GLuint g_vboPos;

bool g_endgame = false;

/**************************************************/

inline bool IsValidTarget(short target, short current)
{
	bool validTarget = 
		 (g_hasChild[target] == false) &&	// It can't already have a child 
		 (g_tails[target] != g_tails[current]);	// It can't be part of ourself

	return validTarget;
}

float SmoothStep(float a, float b, float t)
{
	return a + (pow(t,2)*(3-2*t))*(b - a);
}

HRESULT EndgameUpdate(uint deltaTime)
{
	static uint absoluteTime = 0;
	const float timeLimit = 5000000.0f; // 5 seconds

	absoluteTime += deltaTime;

	for (uint i = 0; i < g_numVerts; i++)
	{	
		if (g_positions[i].x > 1.0f || g_positions[i].x < 0.0f)
			g_vectors[i].x = -g_vectors[i].x;
		if (g_positions[i].y > 1.0f || g_positions[i].y < 0.0f)
			g_vectors[i].y = -g_vectors[i].y;

		float velx = SmoothStep(g_vectors[i].x, 0.0f, float(absoluteTime)/timeLimit);
		float vely = SmoothStep(g_vectors[i].y, 0.0f, float(absoluteTime)/timeLimit);

		g_positions[i].x += + velx * float(deltaTime)/1000000.0f;
		g_positions[i].y += + vely * float(deltaTime)/1000000.0f;
	}

	if (absoluteTime > timeLimit)
	{
		g_endgame = false;
		absoluteTime = 0;

		for (uint i = 0; i < g_numVerts; i++)
		{
			g_tails[i] = i;
			g_hasParent[i] = false;
			g_hasChild[i] = false;
		}
	}

	return S_OK;
}

HRESULT Endgame()
{
	g_endgame = true;

	// TODO: Add "shaking" before we explode. The snake should continue
	//		 to swim along, then start vibrating, then EXPLODE.

	for (uint i = 0; i < g_numVerts; i++)
	{
		float2 velocity;
		float maxVelocity = 0.5f; // screens per second
		g_vectors[i].x = (rand()*2.0f - 1.0f) * maxVelocity;
		g_vectors[i].y = (rand()*2.0f - 1.0f) * maxVelocity;
	}

	return S_OK;
}

HRESULT FindNearestNeighbor(short i)
{
	if (g_hasParent[i] == false) // Find the nearest potential parent
	{
		short nearest = -1;
		float minDist = 1.0f;

		for (uint j = 0; j < g_numVerts; j++)
		{
			if (IsValidTarget(j, i))
			{
				float dist = (g_positions[j] - g_positions[i]).getLength();
				if (dist < minDist)
				{
					minDist = dist;
					nearest = j;
				}
			}
		}

		if (nearest == -1)
			return E_NOTARGETS;

		g_targets[i] = nearest;
	}

	return S_OK;
}

HRESULT Chomp(short chomper)
{
	short target = g_targets[chomper];
	
	if (IsValidTarget(target, chomper))
	{
		g_hasParent[chomper] = true;
		g_hasChild[target] = true;
		g_tails[target] = g_tails[chomper];

		// Update the whole chain's tail
		short index = target;
		while (g_hasParent[index])
		{
			index = g_targets[index];
			g_tails[index] = g_tails[chomper];
		}
	}

	return S_OK;
}

HRESULT Update(uint deltaTime)
{
	HRESULT hr = S_OK;

	// TODO: Optimize for data-drive design

	// Sort into buckets

	// Determine nearest neighbors
	for (uint i = 0; i < g_numVerts; i++)
	{
		IFC( FindNearestNeighbor(i) );
	}

	// Determine target vectors
	for (uint i = 0; i < g_numVerts; i++)
	{
		short target = g_targets[i];
		g_vectors[i] = g_positions[target] - g_positions[i];
	}

	// Determine positions
	for (uint i = 0; i < g_numVerts; i++)
	{
		float2 dir = g_vectors[i];
		float dist = dir.getLength();
		dir.x = (dist != dist || dist == 0.0f ? 0.0f : dir.x/dist);
		dir.y = (dist != dist || dist == 0.0f ? 0.0f : dir.y/dist);
		
		float2 offset;

		if (g_hasParent[i])
		{
			// This controls wigglyness. Perhaps it should be a function of velocity? (static is more wiggly)
			float distanceToParent = g_tailDist;// + (rand() * 2 - 1)*g_tailDist*0.3f;
			offset = g_vectors[i] * (1 - distanceToParent / g_vectors[i].getLength());
		}
		else
			offset = dir * g_speed * float(deltaTime)/1000000.0f;
		
		g_positions[i] = g_positions[i] + offset;
	}
	
	// Check for chomps
	for (uint i = 0; i < g_numVerts; i++)
	{
		float dist = g_vectors[i].getLength();
		if (dist <= g_tailDist)
			Chomp(i);
	}

Cleanup:
	if (hr == E_NOTARGETS)
		return Endgame();

	return hr;
}

HRESULT Render()
{
	glClearColor(0.1f, 0.1f, 0.2f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindBuffer(GL_ARRAY_BUFFER, g_vboPos);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_positions), g_positions, GL_STREAM_DRAW); 

	glDrawArrays(GL_POINTS, 0, g_numVerts);
	return S_OK;
}

HRESULT CreateProgram(GLuint* program)
{
	HRESULT hr = S_OK;

	ASSERT(program != nullptr);

	const char* vertexShaderString = "\
		#version 330\n \
		layout(location = 0) in vec4 position; \
		void main() \
		{ \
			gl_Position = position * 2.0f - 1.0f; \
		}";

	const char* pixelShaderString = "\
		#version 330\n \
		out vec4 outputColor; \
		void main() \
		{ \
			outputColor = 1.0f; \
		} ";

	GLint success;
	GLchar errorsBuf[256];
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint prgm = glCreateProgram();

	glShaderSource(vs, 1, &vertexShaderString, NULL);
	glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success)
	{
		glGetShaderInfoLog(vs, sizeof(errorsBuf), 0, errorsBuf);
		Error("Vertex Shader Errors:\n%s", errorsBuf);
		hr = E_FAIL;
	}

	glShaderSource(ps, 1, &pixelShaderString, NULL);
	glCompileShader(ps);
    glGetShaderiv(ps, GL_COMPILE_STATUS, &success);
    if (!success)
	{
		glGetShaderInfoLog(ps, sizeof(errorsBuf), 0, errorsBuf);
		Error("Pixel Shader Errors:\n%s", errorsBuf);
		hr = E_FAIL;
	}
	
	if (FAILED(hr)) return hr;

	glAttachShader(prgm, vs);
	glAttachShader(prgm, ps);
	glLinkProgram(prgm);
	glGetProgramiv(prgm, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(prgm, sizeof(errorsBuf), 0, errorsBuf);
		Error("Program Link Errors:\n%s", errorsBuf);
		hr = E_FAIL;
	}

	*program = prgm;

	return hr;
}

HRESULT Init()
{
	HRESULT hr = S_OK;

	// Get OpenGL functions
	glGenBuffers = (PFNGLGENBUFFERSPROC)wglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)wglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)wglGetProcAddress("glBufferData");
	glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)wglGetProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)wglGetProcAddress("glVertexAttribPointer");
	glCreateProgram = (PFNGLCREATEPROGRAMPROC)wglGetProcAddress("glCreateProgram");
	glDeleteProgram = (PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram");
	glUseProgram =	  (PFNGLUSEPROGRAMPROC)wglGetProcAddress("glUseProgram");
	glAttachShader =  (PFNGLATTACHSHADERPROC)wglGetProcAddress("glAttachShader");
	glLinkProgram =   (PFNGLLINKPROGRAMPROC)wglGetProcAddress("glLinkProgram");
	glGetProgramiv =  (PFNGLGETPROGRAMIVPROC)wglGetProcAddress("glGetProgramiv");
	glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)wglGetProcAddress("glGetProgramInfoLog");
	glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)wglGetProcAddress("glGetShaderInfoLog");
	glCreateShader = (PFNGLCREATESHADERPROC)wglGetProcAddress("glCreateShader");
	glDeleteShader = (PFNGLDELETESHADERPROC)wglGetProcAddress("glDeleteShader");
	glShaderSource = (PFNGLSHADERSOURCEPROC)wglGetProcAddress("glShaderSource");
	glCompileShader = (PFNGLCOMPILESHADERPROC)wglGetProcAddress("glCompileShader");
	glGetShaderiv = (PFNGLGETSHADERIVPROC)wglGetProcAddress("glGetShaderiv");

	GLuint program;
	IFC( CreateProgram(&program) );
	glUseProgram(program);

	// Calculate random starting positions
	for (uint i = 0; i < g_numVerts; i++)
	{
		g_positions[i].x = rand();
		g_positions[i].y = rand();
	}

	// Initilialize our node attributes
	for (uint i = 0; i < g_numVerts; i++)
	{
		// Every node is its own tail initially
		g_tails[i] = i;
	}

	// Initialize buffers
	uint positionSlot = 0;
    GLsizei stride = sizeof(g_positions[0]);
	GLsizei totalSize = sizeof(g_positions);
    glGenBuffers(1, &g_vboPos);
    glBindBuffer(GL_ARRAY_BUFFER, g_vboPos);
    glBufferData(GL_ARRAY_BUFFER, totalSize, g_positions, GL_STREAM_DRAW);
    glEnableVertexAttribArray(positionSlot);
    glVertexAttribPointer(positionSlot, 2, GL_FLOAT, GL_FALSE, stride, 0);

Cleanup:
	return hr;
}

HRESULT InitWindow(HWND& hWnd, int width, int height)
{
    LPCSTR wndName = "Flow Snake";

	// Create our window
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_CLASSDC;
	wndClass.lpfnWndProc = MsgHandler;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = GetModuleHandle(0);
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(0, IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = wndName;
	wndClass.hIconSm = NULL;
    RegisterClassEx(&wndClass);
	
    DWORD wndStyle = WS_SYSMENU | WS_CAPTION | WS_VISIBLE | WS_POPUP ;
    DWORD wndExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	RECT wndRect;
	SetRect(&wndRect, 0, 0, width, height);
    AdjustWindowRectEx(&wndRect, wndStyle, FALSE, wndExStyle);
    int wndWidth = wndRect.right - wndRect.left;
    int wndHeight = wndRect.bottom - wndRect.top;
    int wndTop = GetSystemMetrics(SM_CYSCREEN) / 2 - wndHeight / 2;
    int wndLeft = GetSystemMetrics(SM_XVIRTUALSCREEN) + 
				  GetSystemMetrics(SM_CXSCREEN) / 2 - wndWidth / 2;

	hWnd = CreateWindowEx(0, wndName, wndName, wndStyle, wndLeft, wndTop, 
						  wndWidth, wndHeight, NULL, NULL, NULL, NULL);
	if (hWnd == NULL)
	{
		Error("Window creation failed with error: %x\n", GetLastError());
		return E_FAIL;
	}

	return S_OK;
}

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE ignoreMe0, LPSTR ignoreMe1, INT ignoreMe2)
{
	HRESULT hr = S_OK;
	HWND hWnd = NULL;
	HDC hDC = NULL;
	HGLRC hRC = NULL;
	MSG msg = {};
	PIXELFORMATDESCRIPTOR pfd;
	
    LARGE_INTEGER previousTime;
    LARGE_INTEGER freqTime;
	double aveDeltaTime = 0.0;

	IFC( InitWindow(hWnd, 1024, 768) );
    hDC = GetDC(hWnd);

    // Create the GL context.
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pixelFormat = ChoosePixelFormat(hDC, &pfd);
	SetPixelFormat(hDC, pixelFormat, &pfd);
    
	hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);

	IFC( Init() );

    QueryPerformanceFrequency(&freqTime);
    QueryPerformanceCounter(&previousTime);
	
    // -------------------
    // Start the Game Loop
    // -------------------
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg); 
            DispatchMessage(&msg);
        }
        else
        {
            LARGE_INTEGER currentTime;
            __int64 elapsed;
            double deltaTime;

            QueryPerformanceCounter(&currentTime);
            elapsed = currentTime.QuadPart - previousTime.QuadPart;
            deltaTime = elapsed * 1000000.0 / freqTime.QuadPart;
			aveDeltaTime = aveDeltaTime * 0.9 + 0.1 * deltaTime;
            previousTime = currentTime;

			if (g_endgame == false)
			{
				IFC( Update((uint) deltaTime));
			}
			else
			{
				IFC( EndgameUpdate((uint) deltaTime));
			}

			Render();
            SwapBuffers(hDC);
            if (glGetError() != GL_NO_ERROR)
            {
                Error("OpenGL error.\n");
            }
        }
    }

Cleanup:
	if(hRC) wglDeleteContext(hRC);
    //UnregisterClassA(szName, wc.hInstance);

	char strBuf[256];
	sprintf_s(strBuf, "Average frame duration = %.3f ms\n", aveDeltaTime/1000.0f); 
	OutputDebugString(strBuf);

    return FAILED(hr);
}

LRESULT WINAPI MsgHandler(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
	case WM_CLOSE:
		PostQuitMessage(0);
		break;

    case WM_KEYDOWN:
        switch (wParam)
        {
            case VK_ESCAPE:
                PostQuitMessage(0);
                break;
        }
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void Error(const char* pStr, ...)
{
    char msg[1024] = {0};
	va_list args;
	va_start(args, pStr);
    _vsnprintf_s(msg, countof(msg), _TRUNCATE, pStr, args);
    OutputDebugString(msg);
}

float rand()
{
	#define RAND_MAX 32767.0f
	static uint seed = 123456789;

	seed = (214013*seed+2531011); 
	return float(((seed>>16)&0x7FFF)/RAND_MAX); 
}