#include "test_config.h"

#include "spu_bit_ops.h"

TEST(TestBitOps, TestSetGetTBitfield) {
	struct spu_instruction instr = {0};

	ASSERT_EQ(instr_set_bitfield(0x3, 2, &instr, 0), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0xC00000);

	ASSERT_EQ(instr_set_bitfield(0x3, 2, &instr, 23), (int)S_FAIL);	

	ASSERT_EQ(instr_set_bitfield(0x3, 2, &instr, 22), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0xC00003);

	uint32_t field = 0;

	set_instr_arg(&instr, 0xC00030);
	ASSERT_EQ(instr_get_bitfield(&field, 2, &instr, 0), (int)S_OK);
	ASSERT_EQ((int)field, 0x3);

	ASSERT_EQ(instr_get_bitfield(&field, 2, &instr, 22), (int)S_OK);
	ASSERT_EQ((int)field, 0x0);

	ASSERT_EQ(instr_get_bitfield(&field, 2, &instr, 18), (int)S_OK);
	ASSERT_EQ((int)field, 0x3);
}

TEST(TestBitOps, TestSetGetRegister) {
	struct spu_instruction instr = {0};

	spu_register_num_t rn = 0;

	ASSERT_EQ(instr_set_register(3, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0x300000);
	ASSERT_EQ((int)instr.opcode.reg_extended, 0);
	ASSERT_EQ(instr_get_register(&rn, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)rn, 3);

	ASSERT_EQ(instr_set_register(30, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0xE00000);
	ASSERT_EQ((int)instr.opcode.reg_extended, 1);
	ASSERT_EQ(instr_get_register(&rn, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)rn, 30);

	ASSERT_EQ(instr_set_register(0, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0x000000);
	ASSERT_EQ((int)instr.opcode.reg_extended, 0);
	ASSERT_EQ(instr_get_register(&rn, &instr, 0, 1), (int)S_OK);	
	ASSERT_EQ((int)rn, 0);

	ASSERT_EQ(instr_set_register(31, &instr, 1, 0), (int)S_OK);	
	ASSERT_EQ((int)get_instr_arg(&instr), 0x7C0000);
	ASSERT_EQ((int)instr.opcode.reg_extended, 0);
	ASSERT_EQ(instr_get_register(&rn, &instr, 1, 0), (int)S_OK);	
	ASSERT_EQ((int)rn, 31);
}
