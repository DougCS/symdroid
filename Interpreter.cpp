#include "gpu.cpp"
#include "SDpath.py"
#include "config.cpp"
#include "dalvik\Dalvik.h"
#include "GLES.cpp"

class symdroid {
	int a;
	int overclock;
	overclock = 0; //turn overclock off
public:
	static void openfile();
	static void emulateCycle();
	static void initialize();
	static void overclock();
};
void symdroid::openfile() {
	#include "Filemanager.cpp";
}

void symdroid::initialize() {
        opcode = 0;
	int sp = 0;
	int pc = 0;
}

if overclock == 1; {
	void phone::overclock() {
	        #include "cpuoverclock"
	}
}

int main() {
	symdroid::openfile();
	symdroid::emulateCycle();
	symdroid::initialize();
}

return 0;
