#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include "spu_asm.h"
#include "pvector.h"

struct label_instance {
	char label[LABEL_MAX_LEN];
	ssize_t instruction_ptr;
};

struct translating_context {
	// Input file with asm text
	struct pvector instr_lines_arr;

	// Prepared assembler instructions
	struct pvector asm_instr_arr;

	// An array with binary instructions
	struct pvector instructions_arr;	

	size_t n_instruction;

	FILE *out_stream;

	// Table of labels
	// Of type label_instance
	struct pvector labels_table;

	int second_compilation;
};

#endif /* TRANSLATOR_H */
