#include <string.h>
#include <assert.h>

#include "spu_asm.h"

// first array used for directive flag
static const struct op_cmd *op_cmd_opcode_table[2][MAX_OPCODE + 1] = {0};
static int opcode_table_initialized = 0;

void init_op_cmd_opcode_table(void) {
	if (opcode_table_initialized) {
		return;
	}

	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		int cmd_is_directive = (op_cmd_ptr->layout->is_directive != 0);
		unsigned int cmd_opcode = op_cmd_ptr->opcode;
		assert (cmd_opcode < MAX_OPCODE);

		op_cmd_opcode_table[cmd_is_directive][cmd_opcode] = op_cmd_ptr;
		op_cmd_ptr++;
	}

	opcode_table_initialized = 1;
}

const struct op_cmd *find_op_cmd_opcode(unsigned int opcode, int is_directive) {
	is_directive = (is_directive != 0);

	if (opcode >= MAX_OPCODE) {
		log_error("Opcode is too big");
		return NULL;
	}

	assert (opcode_table_initialized && "Initialize op cmd table!");

	return op_cmd_opcode_table[is_directive][opcode];
}

/*
const struct op_cmd *find_op_cmd_opcode(unsigned int opcode, int is_directive) {
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (op_cmd_ptr->opcode == opcode &&
			op_cmd_ptr->layout->is_directive == is_directive) {
			return op_cmd_ptr;
		}
		op_cmd_ptr++;
	}

	return NULL;
}
*/

const struct op_cmd *find_op_cmd(const char *cmd_name) {
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (!strcmp(cmd_name, op_cmd_ptr->cmd_name)) {
			return op_cmd_ptr;
		}
		op_cmd_ptr++;
	}

	return NULL;
}

/*
const struct op_cmd *find_op_cmd(const char *cmd_name) {
	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		if (!strcmp(cmd_name, op_cmd_ptr->cmd_name)) {
			return op_cmd_ptr;
		}
		op_cmd_ptr++;
	}

	return NULL;
}
*/
