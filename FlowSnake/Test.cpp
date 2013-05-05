#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <math.h>
#include "Types.h"
#include "Test.h"

const uint g_numVerts = 16000;
extern Node g_nodes[g_numVerts];

extern float frand();
extern HRESULT Update(double deltaTime);

LARGE_INTEGER freqTime;

Counter nearestNeighborCounter;
Counter binningCounter;
Counter positionUpdate;
Counter updateTime;

inline void BeginCounter(Counter* counter)
{
	QueryPerformanceCounter(&counter->start);
}

inline void EndCounter(Counter* counter)
{
	QueryPerformanceCounter(&counter->end);
}

double GetCounter(Counter& counter)
{
	return double(counter.end.QuadPart - counter.start.QuadPart) / freqTime.QuadPart;
}

// Let's set up a reproduceable test environment...
int testMain (int argc, char* argv[])
{
	double aveDeltaTime = 0;
	double aveBinning = 0;
	double aveNN = 0;
	double avePos = 0;

	uint numUpdateLoops = 100;

    QueryPerformanceFrequency(&freqTime);

	// Each run starts with a new set of initial random positions
	// But each test pass will have the same set of initial position sets
	for (uint i = 0; i < numUpdateLoops; i++)
	{
		BeginCounter(&updateTime);
		Update(0.016);
		EndCounter(&updateTime);

		if (i >= 10) // Skip the first c iterations to warm it up a bit
		{
			aveDeltaTime += GetCounter(updateTime);
			aveBinning += GetCounter(binningCounter);
			aveNN += GetCounter(nearestNeighborCounter);
			avePos += GetCounter(positionUpdate);
		}
		
		// Reset the sim
		memset(g_nodes, 0, sizeof(g_nodes));
		for (uint i = 0; i < g_numVerts; i++)
		{
			g_nodes[i].position.setX(frand()*2 - 1);
			g_nodes[i].position.setY(frand()*2 - 1);
		}
	}
	
	printf("Average Update duration = %.3f ms\n", aveDeltaTime/numUpdateLoops * 1000.0f); 
	printf("Average Binning duration = %.3f ms\n", aveBinning/numUpdateLoops * 1000.0f); 
	printf("Average Nearest Neighbor duration = %.3f ms\n", aveNN/numUpdateLoops * 1000.0f); 
	printf("Average Position Update duration = %.3f ms\n", avePos/numUpdateLoops * 1000.0f); 

	return 0;
}
