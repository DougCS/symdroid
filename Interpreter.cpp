#include "gpu.cpp"
#include "SDpath.py"
#include "config.cpp"
#include "dalvik\Dalvik.h"
#include "GLES.cpp"

class symdroid {
	int a;
public:
	static void openfile();
	static void emulateCycle();
	static void initialize();
};
void symdroid::openfile() {
	#include "Filemanager.cpp";
}

}

void symdroid::initialize() {
	opcode = 0;
	int sp = 0;
	int pc = 0;
}

int main() {
	symdroid::openfile();
	symdroid::emulateCycle();
	symdroid::initialize();
}

return 0;
