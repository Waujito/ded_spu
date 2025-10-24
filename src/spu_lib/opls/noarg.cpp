#include <assert.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"

DEFINE_BINARY_PARSER(noarg, {
	_CT_CHECKED(get_directive_opcode(&instr_data->opcode, bin_instr));
});

DEFINE_BINARY_WRITER(noarg, {
	_CT_CHECKED(set_directive_opcode(instr_data->opcode, bin_instr));
});

DEFINE_ASM_PARSER(noarg, {
});


DEFINE_ASM_WRITER(noarg, {
	status = fprintf(out_stream, "%s", op_cmd->cmd_name);
});


DEFINE_OP_LAYOUT(noarg, 1);
