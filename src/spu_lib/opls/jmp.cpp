#include <assert.h>
#include <string.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"
#include "translator.h"

DEFINE_BINARY_PARSER(jmp, {
	_CT_CHECKED(instr_get_register(&instr_data->jmp_condition, bin_instr,
				0, USE_R_HEAD_BIT));
	{
		uint32_t unum = 0;
		_CT_CHECKED(instr_get_bitfield(&unum, JMP_INTEGER_BLEN,
 					bin_instr, FREGISTER_BIT_LEN));

		instr_data->jmp_position = bit_extend_signed(unum, JMP_INTEGER_BLEN);
	}

	_CT_CHECKED(get_raw_opcode(&instr_data->opcode, bin_instr));
});

DEFINE_BINARY_WRITER(jmp, {
	// !!! Setts flags instead of registers !!!
	_CT_CHECKED(instr_set_register(instr_data->jmp_condition,
			bin_instr, 0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_bitfield((uint32_t) instr_data->jmp_position, JMP_INTEGER_BLEN,
				bin_instr, FREGISTER_BIT_LEN));

	_CT_CHECKED(set_raw_opcode(instr_data->opcode, bin_instr));
});

static struct label_instance *find_label(struct translating_context *ctx,
					 const char *label_name) {
	assert (ctx);
	assert (label_name);

	for (size_t i = 0; i < ctx->labels_table.len; i++) {
		struct label_instance *flabel = NULL;
		if (pvector_get(&(ctx->labels_table), i, (void **)&flabel)) {
			return NULL;
		}

		if (!strcmp(flabel->label, label_name)) {
			return flabel;
		}
	}

	return NULL;
}

static int parse_jmp_position(	const struct asm_instruction *asm_instr,
				const char *jmp_str, int32_t *jmp_arg) {
	assert (asm_instr);
	assert (jmp_str);
	assert (jmp_arg);

	int ret = S_OK;

	int32_t relative_jmp = 0;

	if (*jmp_str == '.') {
		struct label_instance *label_inst = find_label(asm_instr->ctx, jmp_str);
		if (!label_inst) {
			size_t label_len = strlen(jmp_str);
			struct label_instance label = {{0}};

			if (label_len > LABEL_MAX_LEN) {
				log_error("Invalid label: %s", jmp_str);
				_CT_FAIL();
			}

			memcpy(label.label, jmp_str, label_len);
			label.instruction_ptr = -1;

			return S_OK;
		}

		if (asm_instr->ctx->second_compilation && label_inst->instruction_ptr == -1) {
			log_error("Undefined label: %s", jmp_str);
			_CT_FAIL();
		}

		int32_t absolute_ptr = (int32_t)label_inst->instruction_ptr;
		int32_t current_ptr = (int32_t)asm_instr->ctx->n_instruction;
		int32_t relative_ptr = absolute_ptr - current_ptr - 1;

		relative_jmp = relative_ptr;
	} else if (*jmp_str == '$') {
		int32_t number = 0;

		_CT_CHECKED(parse_literal_number(jmp_str, &number));

		relative_jmp = number;
	} else {
		_CT_FAIL();
	}

	if (test_integer_bounds(relative_jmp, JMP_INTEGER_BLEN)) {
		log_error("jump number <%d> is too long", relative_jmp);
		_CT_FAIL();
	}
	
	*jmp_arg = relative_jmp;

_CT_EXIT_POINT:
	return ret;
}

int process_label(struct asm_instruction *asm_instr) {
	assert (asm_instr);

	int ret = S_OK;
	char *label_name = asm_instr->argsptrs[0];
	size_t label_len = strlen(label_name);
	struct label_instance label = {{0}};
	struct label_instance *found_label = NULL;

	if (asm_instr->ctx->second_compilation) {
		return S_OK;
	}

	size_t instr_ptr = asm_instr->ctx->n_instruction;

	if (	asm_instr->n_args != 1 || 
		*label_name != '.' || 
		// label_min_len + ':'
		label_len < LABEL_MIN_LEN + 1) {
		log_error("Invalid label: %s", label_name);
		_CT_FAIL();
	}

	if (label_name[label_len - 1] == ':') {
		label_name[label_len - 1] = '\0';
		label_len--;
	}

	if (label_len > LABEL_MAX_LEN) {
		log_error("Label %s is too long: maximum size is %d",
			label_name, LABEL_MAX_LEN);
		_CT_FAIL();
	}

	if ((found_label = find_label(asm_instr->ctx, label_name))) {
		if (found_label->instruction_ptr != -1) {
			log_error("Label %s is already used", label_name);
			_CT_FAIL();
		} else {
			found_label->instruction_ptr = (ssize_t)instr_ptr;
		}
	}

	memcpy(label.label, label_name, label_len);
	label.instruction_ptr = (ssize_t)instr_ptr;

	_CT_FAIL_NONZERO(pvector_push_back(&asm_instr->ctx->labels_table, &label));

_CT_EXIT_POINT:
	return ret;
}

static const struct jmp_cond_mapping {
	const char *jmp_cond_name;
	enum jmp_conditions condition;
} jmp_conditions[] = {
	{"eq",	EQUALS_JMP},
	{"neq", NOT_EQUALS_JMP},
	{"geq", GREATER_EQUALS_JMP},
	{"gt",	GREATER_JMP},
	{"leq", LESS_EQUALS_JMP},
	{"lt",	LESS_JMP},
	{0},
};

static int parse_jmp_condition(const struct asm_instruction *asm_instr,
			       spu_register_num_t *jmp_condition) {
	assert (asm_instr);
	assert (jmp_condition);

	if (!asm_instr->op_arg) {
		*jmp_condition = UNCONDITIONAL_JMP;
		return S_OK;
	}

	const struct jmp_cond_mapping *jmp_mp = jmp_conditions;

	while (jmp_mp->jmp_cond_name != NULL) {
		if (!strcmp(jmp_mp->jmp_cond_name, asm_instr->op_arg)) {
			*jmp_condition = (uint8_t)jmp_mp->condition;
			return S_OK;
		}
		jmp_mp++;
	}

	return S_FAIL;
}

DEFINE_ASM_PARSER(jmp, {
	if (asm_instr->n_args != 1 + 1) {
		_CT_FAIL();
	}

	char *jmp_str = asm_instr->argsptrs[1];

	_CT_CHECKED(parse_jmp_condition(asm_instr, &instr_data->jmp_condition));
	_CT_CHECKED(parse_jmp_position(asm_instr, jmp_str, &instr_data->jmp_position));
});

DEFINE_ASM_WRITER(jmp, {
	if (!instr_data->jmp_condition) {
		status = fprintf(out_stream, "%s $%d", op_cmd->cmd_name,
			instr_data->jmp_position);
	} else {
		const char *jmp_condition_name = "unknown_cond";

		const struct jmp_cond_mapping *jmp_mp = jmp_conditions;

		while (jmp_mp->jmp_cond_name != NULL) {
			if (jmp_mp->condition == instr_data->jmp_condition) {
				jmp_condition_name = jmp_mp->jmp_cond_name;
			}
			jmp_mp++;
		}

		status = fprintf(out_stream, "%s.%s $%d", op_cmd->cmd_name,
			jmp_condition_name,
			instr_data->jmp_position);

	}
});


DEFINE_OP_LAYOUT(jmp, 0);
