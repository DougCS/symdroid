#include "CPU.cpp"
#include "gpu.cpp"
#include "SDpath.py"
#include "config.cpp"

unsigned char delay_timer;

class symdroid {
	int a;
public:
	static void openfile();
	static void emulateCycle();
	static void initialize();
};
void symdroid::openfile() {
	#include "filebrowser.py";
}

void symdroid::emulateCycle() {
	if (delay_timer > 0)
		--delay_timer;
}

void symdroid::initialize() {
	opcode = 0;
	int I = 0;
	int sp = 0;
}

int main() {
	symdroid::openfile();
	symdroid::emulateCycle();
	symdroid::initialize();
}
