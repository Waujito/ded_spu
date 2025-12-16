#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "spu_asm.h"
#include "sort.h"

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

struct hashed_op_cmd {
	uint32_t hash;
	const struct op_cmd *op_ptr;
};

static struct hashed_op_cmd
	hashed_ops[sizeof(op_table)/sizeof(struct op_cmd)] = {{0}};
ssize_t hashed_ops_size = -1;

uint32_t hash_str(const char *str) {
	uint32_t hash = 5381;
	int32_t c = 0;

	while ((c = *str++))
		/* hash * 33 + c */
		hash = ((hash << 5) + hash) + (uint32_t)c;

	return hash;
}

int hash_sorting_comparator(const void *st1, const void *st2) {
	const struct hashed_op_cmd *op1 = (const struct hashed_op_cmd *)st1;
	const struct hashed_op_cmd *op2 = (const struct hashed_op_cmd *)st2;
	uint64_t h1 = op1->hash;
	uint64_t h2 = op2->hash;

	if (h1 < h2) {
		return -1;
	} else if (h1 == h2) {
		return 0;
	} else {
		return 1;
	}
}

void prepare_hashed_table(void) {
	size_t i = 0;

	const struct op_cmd *op_cmd_ptr = op_table;

	while (op_cmd_ptr->cmd_name != NULL) {
		struct hashed_op_cmd hop = {
			.hash = 0,
			.op_ptr = op_cmd_ptr,
		};

		hop.hash = hash_str(op_cmd_ptr->cmd_name);
		hashed_ops[i] = hop;

		eprintf("%u, %s\n", hop.hash, hop.op_ptr->cmd_name);

		op_cmd_ptr++;
		i++;
	}
	hashed_ops_size = (ssize_t)i;

	qsort(hashed_ops, i, sizeof(hashed_ops[0]),
			       hash_sorting_comparator);
}

/*
const struct op_cmd *find_op_cmd(const char *cmd_name) {
	struct hashed_op_cmd hop = {
		.hash = hash_str(cmd_name),
		.op_ptr = NULL
	};

	if (hashed_ops_size == -1) {
		prepare_hashed_table();
	}

	assert (hashed_ops_size != -1 && "Initialize hash ops first!");

	struct hashed_op_cmd *shop = (struct hashed_op_cmd *)bsearch(
			&hop, hashed_ops,
			(size_t)hashed_ops_size, sizeof(hashed_ops[0]),
			hash_sorting_comparator
	);

	if (!shop) {
		return NULL;
	}

	return shop->op_ptr;
}
*/

// /*
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
// */
