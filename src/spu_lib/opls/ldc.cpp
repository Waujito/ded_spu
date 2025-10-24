#include <assert.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"

DEFINE_BINARY_PARSER(ldc, {
	_CT_CHECKED(instr_get_register(&instr_data->rdest, bin_instr,
				0, USE_R_HEAD_BIT));
	{
		uint32_t unum = 0;
		_CT_CHECKED(instr_get_bitfield(&unum, LDC_INTEGER_BLEN,
 					bin_instr, FREGISTER_BIT_LEN));

		instr_data->snum = bit_extend_signed(unum, LDC_INTEGER_BLEN);
	}

	_CT_CHECKED(get_raw_opcode(&instr_data->opcode, bin_instr));
});

DEFINE_BINARY_WRITER(ldc, {

	_CT_CHECKED(instr_set_register(instr_data->rdest, bin_instr,
				0, USE_R_HEAD_BIT));

	{
		uint32_t arg_num = (uint32_t)instr_data->snum;
		_CT_CHECKED(instr_set_bitfield(arg_num, LDC_INTEGER_BLEN,
				bin_instr, FREGISTER_BIT_LEN));
	}

	_CT_CHECKED(set_raw_opcode(instr_data->opcode, bin_instr));
});


DEFINE_ASM_PARSER(ldc, {
	if (asm_instr->n_args != 1 + 2) {
		_CT_FAIL();
	}

	_CT_CHECKED(parse_register(asm_instr->argsptrs[1], &instr_data->rdest));
	_CT_CHECKED(parse_literal_number(asm_instr->argsptrs[2], &instr_data->snum));

	if (test_integer_bounds(instr_data->snum, LDC_INTEGER_BLEN)) {
		log_error("number <%s> is too long", asm_instr->argsptrs[2]);
		_CT_FAIL();
	}
});


DEFINE_ASM_WRITER(ldc, {
	status = fprintf(out_stream, "%s r%d $%d", op_cmd->cmd_name,
			instr_data->rdest, instr_data->snum);
});

DEFINE_BINASM_PARSERS(ldc, 0);
