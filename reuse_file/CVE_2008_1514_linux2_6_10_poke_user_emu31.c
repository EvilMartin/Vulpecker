static int
CVE_2008_1514_linux2_6_10_poke_user_emu31(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user32 *dummy32 = NULL;
	per_struct32 *dummy_per32 = NULL;
	addr_t offset;
	__u32 tmp;

	if (!test_thread_flag(TIF_31BIT) ||
	    (addr & 3) || addr > sizeof(struct user32) - 3)
		return -EIO;

	tmp = (__u32) data;

	if (addr < (addr_t) &dummy32->regs.acrs) {
		/*
		 * psw, gprs, acrs and orig_gpr2 are stored on the stack
		 */
		if (addr == (addr_t) &dummy32->regs.psw.mask) {
			/* Build a 64 bit psw mask from 31 bit mask. */
			if (tmp != PSW32_MASK_MERGE(PSW32_USER_BITS, tmp))
				/* Invalid psw mask. */
				return -EINVAL;
			__KSTK_PTREGS(child)->psw.mask =
				PSW_MASK_MERGE(PSW_USER32_BITS, (__u64) tmp << 32);
		} else if (addr == (addr_t) &dummy32->regs.psw.addr) {
			/* Build a 64 bit psw address from 31 bit address. */
			__KSTK_PTREGS(child)->psw.addr = 
				(__u64) tmp & PSW32_ADDR_INSN;
		} else {
			/* gpr 0-15 */
			*(__u32*)((addr_t) &__KSTK_PTREGS(child)->psw
				  + addr*2 + 4) = tmp;
		}
	} else if (addr < (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * access registers are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy32->regs.acrs;
		*(__u32*)((addr_t) &child->thread.acrs + offset) = tmp;

	} else if (addr == (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * orig_gpr2 is stored on the kernel stack
		 */
		*(__u32*)((addr_t) &__KSTK_PTREGS(child)->orig_gpr2 + 4) = tmp;

	} else if (addr < (addr_t) (&dummy32->regs.fp_regs + 1)) {
		/*
		 * floating point regs. are stored in the thread structure 
		 */
		if (addr == (addr_t) &dummy32->regs.fp_regs.fpc &&
		    (tmp & ~FPC_VALID_MASK) != 0)
			/* Invalid floating point control. */
			return -EINVAL;
	        offset = addr - (addr_t) &dummy32->regs.fp_regs;
		*(__u32 *)((addr_t) &child->thread.fp_regs + offset) = tmp;

	} else if (addr < (addr_t) (&dummy32->regs.per_info + 1)) {
		/*
		 * per_info is found in the thread structure.
		 */
		offset = addr - (addr_t) &dummy32->regs.per_info;
		/*
		 * This is magic. See per_struct and per_struct32.
		 * By incident the offsets in per_struct are exactly
		 * twice the offsets in per_struct32 for all fields.
		 * The 8 byte fields need special handling though,
		 * because the second half (bytes 4-7) is needed and
		 * not the first half.
		 */
		if ((offset >= (addr_t) &dummy_per32->control_regs &&
		     offset < (addr_t) (&dummy_per32->control_regs + 1)) ||
		    (offset >= (addr_t) &dummy_per32->starting_addr &&
		     offset <= (addr_t) &dummy_per32->ending_addr) ||
		    offset == (addr_t) &dummy_per32->lowcore.words.address)
			offset = offset*2 + 4;
		else
			offset = offset*2;
		*(__u32 *)((addr_t) &child->thread.per_info + offset) = tmp;

	}

	FixPerRegisters(child);
	return 0;
}