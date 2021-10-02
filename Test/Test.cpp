#include "Test.h"
#include "LoadTest.h"

int main(void)
{

	LoadTest* loadTest = new LoadTest();
	loadTest->Execute();
	delete loadTest;

}