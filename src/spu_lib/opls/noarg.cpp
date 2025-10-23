#include <assert.h>
#include "spu_asm.h"
#include "spu_bit_ops.h"
#include "opls.h"
#include "translator_parsers.h"

DEFINE_BINARY_PARSER(inoarg, OPL_NOARG, {
	_CT_CHECKED(get_directive_opcode(&instr_data->opcode, bin_instr));
});

DEFINE_BINARY_WRITER(inoarg, OPL_NOARG, {
	_CT_CHECKED(set_directive_opcode(instr_data->opcode, bin_instr));
});

DEFINE_ASM_PARSER(inoarg, OPL_NOARG, {
});


DEFINE_ASM_WRITER(inoarg, OPL_NOARG, {
	status = fprintf(out_stream, "%s", op_cmd->cmd_name);
});
