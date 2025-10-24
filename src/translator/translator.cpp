/**
 * @file
 *
 * @brief A translator
 */

#include <string.h>
#include <stdlib.h>

#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "spu_debug.h"
#include "translator_parsers.h"
#include "translator.h"
#include "pvector.h"
#include "ctio.h"
#include "jmp_opl.h"

#ifdef _DEBUG
#define T_DEBUG // FIXME:
#endif

static inline int is_comment_string(char *arg) {
	assert (arg);

	char *comment_ptr = strpbrk(arg, ";#");
	if (comment_ptr) {
		*comment_ptr = '\0';
		return 1;
	}

	return 0;
}

static const char *const argsplit_syms = " \n\t";

static ssize_t tokenize_opcodeline(char *lineptr, char *argsptrs[MAX_INSTR_ARGS]) {
	assert (lineptr);
	assert (argsptrs);

	char *saveptr = NULL;

	ssize_t n_args = 0;
	char *strtok_ptr = lineptr;
	for (; n_args < MAX_INSTR_ARGS; 
			strtok_ptr = NULL, n_args++) {
		char *subtoken = strtok_r(strtok_ptr,
			    argsplit_syms, &saveptr);
		if (subtoken == NULL)
			break;

		argsptrs[n_args] = subtoken;

		if (is_comment_string(subtoken)) {
			if (*subtoken != '\0') {
				n_args++;
			}

			break;
		}
	}

	if (n_args == MAX_INSTR_ARGS) {
		return S_FAIL;
	}

	return n_args;
}

struct instruction_line {
	char *lineptr;
	size_t linesz;
	char *argsptrs[MAX_INSTR_ARGS];
	size_t n_args;
};

static int tokenize_instructions(struct pvector *instr_lines_arr,
				 char *textbuf, size_t textbuf_len) {
	int ret = S_OK;

	PVECTOR_CREATE(lines_arr, sizeof(struct text_line));
	_CT_FAIL_NONZERO(pvector_read_lines(&lines_arr,
			  textbuf, textbuf_len));

	_CT_FAIL_NONZERO(pvector_init(instr_lines_arr, sizeof(struct instruction_line)));
	_CT_FAIL_NONZERO(pvector_set_capacity(instr_lines_arr, lines_arr.len + 1));

	for (size_t i = 0; i < lines_arr.len; i++) {
		struct text_line *line = NULL;
		_CT_FAIL_NONZERO(pvector_get(
			&lines_arr, i, (void **)&line));

		struct instruction_line instr_line = {
			.lineptr = line->line_ptr,
			.linesz = line->line_sz,
			.argsptrs = {0},
			.n_args = 0,
		};

		ssize_t n_args = 0;

		if ((n_args = tokenize_opcodeline(
			instr_line.lineptr,
			instr_line.argsptrs)) < 0) {
			log_error("Invalid line #%zu", i + 1);
			_CT_FAIL();
		}
		instr_line.n_args = (size_t)n_args;

		_CT_FAIL_NONZERO(pvector_push_back(
			instr_lines_arr, &instr_line));
	}

_CT_EXIT_POINT:
	pvector_destroy(&lines_arr);

	return ret;
}

static int lookup_asm_instruction(size_t n_args, char **argsptrs,
				  struct asm_instruction *asm_instr) {
	assert (argsptrs);
	assert (asm_instr);

	asm_instr->n_args	= n_args;
	asm_instr->argsptrs	= argsptrs;

	if (asm_instr->n_args == 0) {
		asm_instr->is_empty = 1;
		return S_OK;
	}
	
	char *cmd = asm_instr->argsptrs[0];

	// Some instructions accept arguments divided by function name and point
	char *point_arg = strchr(cmd, '.');

	// If point is placed in start of instruction, it is label declaration
	if (cmd == point_arg) {
		asm_instr->is_label = 1;
		return S_OK;
	}

	if (point_arg) {
		*(point_arg++) = '\0';

		if (*point_arg == '\0') {
			return S_FAIL;
		}
	}


	const struct op_cmd *op_cmd_ptr = find_op_cmd(cmd);
	if (!op_cmd_ptr) {
		return S_FAIL;
	}

	asm_instr->op_cmd = op_cmd_ptr;
	asm_instr->op_arg = point_arg;

	return S_OK;
}

static int assemble_instruction(struct asm_instruction *asm_instr) {
	assert (asm_instr);

	struct spu_instruction bin_instr = {0};

	int ret = S_OK;

	const struct op_cmd *op_cmd = asm_instr->op_cmd;

	struct spu_instr_data instr_data = {0};

	_CT_CHECKED(op_cmd->layout->parse_asm_fn(asm_instr, &instr_data));
	_CT_CHECKED(op_cmd->layout->write_bin_fn(&instr_data, &bin_instr));

#ifdef T_DEBUG
	eprintf("Writing instruction: <0x");
	buf_dump_hex(&bin_instr, sizeof (bin_instr), stderr);
	eprintf(">\n");
#endif /* T_DEBUG */

	_CT_FAIL_NONZERO(pvector_push_back(
		&asm_instr->ctx->instructions_arr, &bin_instr));

	asm_instr->ctx->n_instruction++;

_CT_EXIT_POINT:
	return ret;
}

static int prepare_asm_instructions(struct translating_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	for (size_t nline = 0; nline < ctx->instr_lines_arr.len; nline++) {
		struct instruction_line *instr_line = NULL;
		_CT_FAIL_NONZERO(pvector_get(&ctx->instr_lines_arr,
			  nline, (void **)&instr_line));

		struct asm_instruction asm_instr = {0};

		if (lookup_asm_instruction(instr_line->n_args, 
				instr_line->argsptrs, &asm_instr)) {
			log_error("Invalid line #%zu", nline + 1);
			_CT_FAIL();
		}

		asm_instr.ctx = ctx;

		if (asm_instr.is_empty) {
			continue;
		}

		asm_instr.nline = nline;

		_CT_FAIL_NONZERO(pvector_push_back(&ctx->asm_instr_arr, &asm_instr));
	}

_CT_EXIT_POINT:
	return ret;
}

static int assembly(struct translating_context *ctx) {
	assert (ctx);

	int ret = S_OK;

	for (size_t i = 0; i < ctx->asm_instr_arr.len; i++) {
		struct asm_instruction *asm_instr = NULL;
		_CT_FAIL_NONZERO(pvector_get(&ctx->asm_instr_arr,
			  i, (void **)&asm_instr));

		if (asm_instr->is_empty) {
			continue; // REVIEW:
		} else if (asm_instr->is_label) {
			_CT_CHECKED(process_label(asm_instr));
		} else {
			if (assemble_instruction(asm_instr)) {
				log_error("Invalid line #%zu", asm_instr->nline + 1);
				_CT_FAIL();
			}
		}
	}

_CT_EXIT_POINT:
	return ret;
}

static int parse_text(const char *in_filename, FILE *out_stream) {
	assert (in_filename);
	assert (out_stream);

	int ret = S_OK;

	char *textbuf = NULL;
	size_t textbuf_len = 0;

	struct translating_context ctx = {
		.instr_lines_arr = {0},
		.instructions_arr = {0},

		.n_instruction = 0,

		.out_stream = out_stream,
		.labels_table = {0},
		.second_compilation = 0,
	};

	_CT_CHECKED(read_file(in_filename, &textbuf, &textbuf_len));

	_CT_CHECKED(tokenize_instructions(&ctx.instr_lines_arr,
			textbuf, textbuf_len));
	
	_CT_FAIL_NONZERO(pvector_init(&ctx.labels_table,
			  sizeof(struct label_instance)));
	//TODO bin-INSTR
	_CT_FAIL_NONZERO(pvector_init(&ctx.instructions_arr,
			  sizeof(spu_instruction_t)));
	_CT_FAIL_NONZERO(pvector_init(&ctx.asm_instr_arr,
			       sizeof(struct asm_instruction)));
	     
	_CT_CHECKED(prepare_asm_instructions(&ctx));

	_CT_CHECKED(assembly(&ctx));

	ctx.second_compilation = 1;
	_CT_FAIL_NONZERO(pvector_empty(&ctx.instructions_arr));
	ctx.n_instruction = 0;
	_CT_CHECKED(assembly(&ctx));

	for (size_t i = 0; i < ctx.instructions_arr.len; i++) {
		spu_instruction_t *bin_instr = NULL;
		_CT_FAIL_NONZERO(pvector_get(
			&ctx.instructions_arr, i, (void **)&bin_instr));
		fwrite(bin_instr, sizeof(*bin_instr), 1, out_stream);
	}

_CT_EXIT_POINT:
	pvector_destroy(&ctx.instructions_arr);
	pvector_destroy(&ctx.labels_table);
	pvector_destroy(&ctx.asm_instr_arr);
	pvector_destroy(&ctx.instr_lines_arr);
	free(textbuf);

	return ret;
}


int main(int argc, const char *argv[]) {
	const char *asm_filename = "example.asm";

	if (argc < 2) {
	} else if (argc == 2) {
		asm_filename = argv[1];
	} else {
		log_error("Invalid args");
		return EXIT_FAILURE;
	}

	if (parse_text(asm_filename, stdout)) {
		log_error("Error while parsing asm");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
