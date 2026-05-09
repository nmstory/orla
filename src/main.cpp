#include <simulator.h>

int main(int argc, char *argv[]) {
	orla::Simulator simulator;

	if (simulator.init() != 0) {
		return 1;
	}

	simulator.run();
	return 0;
}
