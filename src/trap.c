#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <kernel/serial.h>
#include <arch/csr.h>
#include <arch/timer.h>
#include <arch/plic.h>

/* defined in src/trap_entry.S */
extern void trap_entry();

void handle_irq()
{
	u64 scause = csr_read(CSR_SCAUSE);

	switch (scause) {
	case TRAP_TIMER_IRQ:
		timer_irq();
		break;
	case TRAP_EXTERNAL_IRQ: {
		u32 irq = plic_hart_claim_irq(0);

		if (irq == IRQ_SERIAL)
			serial_irq();

		if (irq != 0)
			plic_hart_complete_irq(0, irq);
		break;
	}
	default:
		warn("trap: unhandled interrupt, scause=%#x\n", scause);
	}
}

void handle_exception()
{
	u64 scause = csr_read(CSR_SCAUSE);
	u64 stval = csr_read(CSR_STVAL);
	u64 sepc = csr_read(CSR_SEPC);

	switch (scause) {
	case EXCEPTION_INST_ACCESS_FAULT:
		error("instruction access fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	case EXCEPTION_LOAD_ACCESS_FAULT:
		error("load access fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	case EXCEPTION_STORE_ACCESS_FAULT:
		error("store access fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	case EXCEPTION_INST_PAGE_FAULT:
		error("instruction page fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	case EXCEPTION_LOAD_PAGE_FAULT:
		error("load page fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	case EXCEPTION_STORE_PAGE_FAULT:
		error("store page fault at address %#x, sepc = %#x\n", stval, sepc);
		break;
	default:
		error("uncaught exception! cause=%#x, sepc=%#x, stval=%#x\n", scause, sepc, stval);
	}

	panic("unhandled exception (scause=%#x)\n", scause);
}

void trap_setup()
{
	hart_irq_disable();
	csr_write(CSR_STVEC, (u64) trap_entry);
}

void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);

	if (scause & TRAP_IRQ_BIT)
		handle_irq();
	else
		handle_exception();
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	return csr_read_clear(CSR_SSTATUS, CSR_SSTATUS_SIE) & CSR_SSTATUS_SIE;
}

void hart_irq_restore(u64 flags)
{
	if (flags & CSR_SSTATUS_SIE)
		csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}
