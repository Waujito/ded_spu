#include <assert.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"

DEFINE_BINARY_PARSER(mov, {
	_CT_CHECKED(instr_get_register(&instr_data->rdest, bin_instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_get_register(&instr_data->rsrc1, bin_instr,
				FREGISTER_BIT_LEN, NO_R_HEAD_BIT));
	_CT_CHECKED(get_raw_opcode(&instr_data->opcode, bin_instr));
});

DEFINE_BINARY_WRITER(mov, {

	_CT_CHECKED(instr_set_register(instr_data->rdest, bin_instr,
				0, USE_R_HEAD_BIT));
	_CT_CHECKED(instr_set_register(instr_data->rsrc1, bin_instr,
				FREGISTER_BIT_LEN, NO_R_HEAD_BIT));

	_CT_CHECKED(set_raw_opcode(instr_data->opcode, bin_instr));
});

DEFINE_ASM_PARSER(mov, {
	if (asm_instr->n_args != 1 + 2) {
		_CT_FAIL();
	}

	_CT_CHECKED(parse_register(asm_instr->argsptrs[1], &instr_data->rdest));
	_CT_CHECKED(parse_register(asm_instr->argsptrs[2], &instr_data->rsrc1));
});


DEFINE_ASM_WRITER(mov, {
	status = fprintf(out_stream, "%s r%d r%d", op_cmd->cmd_name,
			instr_data->rdest, instr_data->rsrc1);
});

DEFINE_OP_LAYOUT(mov, 0);
