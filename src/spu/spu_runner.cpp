/**
 * @file
 *
 * @brief Runner for SPU
 */

#include <stdlib.h>

#include "spu.h"


static int run_spu(const char *in_filename) {
	SPUCreate(ctx);

	int ret = S_OK;

	_CT_CHECKED(SPULoadBinary(&ctx, in_filename));

	if ((ret = SPUExecute(&ctx))) {
		SPUDump(&ctx, stderr);

		log_error("[CORE DUMPED] The program exited with non-zero exit code: <%d>", ret);
		ret = S_OK;
	}


_CT_EXIT_POINT:
	SPUDtor(&ctx);
	return ret;
}

int main(int argc, const char *argv[]) {
	const char *binary_filename = "example.o";

	if (argc < 2) {
	} else if (argc == 2) {
		binary_filename = argv[1];
	} else {
		log_error("Invalid args");
		return EXIT_FAILURE;
	}

	if (run_spu(binary_filename)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
