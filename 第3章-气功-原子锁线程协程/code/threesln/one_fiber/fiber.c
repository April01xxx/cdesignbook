#include <stdio.h>
#include <windows.h>

static void WINAPI _fiber_run(LPVOID fiber) {
	puts("_fiber_run begin");
	// �л������˳���
	SwitchToFiber(fiber);
	puts("_fiber_run e n d");

	// �����л������˳���, ���˳̲�֪�����˳�����
	SwitchToFiber(fiber);
}

//
// winds fiber hello world
//
int main(int argc, char * argv[]) {
	PVOID fiber, fiberc;
	// A pointer to a variable that is passed to the fiber. 
	// The fiber can retrieve this data by using the GetFiberData macro.
    fiber = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
	// ������ͨ�˳�, ��ǰ���������˳���
	fiberc = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, _fiber_run, fiber);
	puts("main ConvertThreadToFiberEx begin");

	SwitchToFiber(fiberc);
	puts("main ConvertThreadToFiberEx SwitchToFiber begin");
	
	SwitchToFiber(fiberc);
	puts("main ConvertThreadToFiberEx SwitchToFiber again begin");

	DeleteFiber(fiberc);
	ConvertFiberToThread();
	puts("main ConvertThreadToFiberEx e n d");
	return EXIT_SUCCESS;
}