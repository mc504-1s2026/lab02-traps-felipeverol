#include <arch/timer.h>
#include <arch/csr.h>
#include <arch/spinlock.h>
#include <kernel/panic.h>
#include <kernel/serial.h>

#define MAX_ALARMS 8

static u64 alarm_deadlines[MAX_ALARMS];
static size_t alarm_count;
static struct spinlock alarm_lock;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

static void timer_program_next()
{
	u64 earliest;
	size_t i;

	if (alarm_count == 0) {
		csr_write(CSR_STIMECMP, ~0UL);
		return;
	}

	earliest = alarm_deadlines[0];
	for (i = 1; i < alarm_count; i++) {
		if (alarm_deadlines[i] < earliest)
			earliest = alarm_deadlines[i];
	}

	csr_write(CSR_STIMECMP, earliest);
}

void timer_irq_enable()
{
	spin_init(&alarm_lock);
	alarm_count = 0;
	csr_write(CSR_STIMECMP, ~0UL);
	csr_set(CSR_SIE, CSR_SIE_STIE);
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 flags;

	flags = spin_lock_irqsave(&alarm_lock);

	if (alarm_count < MAX_ALARMS)
		alarm_deadlines[alarm_count++] = timer_read() + secs * TIMER_FREQ;

	timer_program_next();

	spin_unlock_irqrestore(&alarm_lock, flags);
}

void timer_irq()
{
	u64 now;
	size_t i;

	spin_lock(&alarm_lock);

	now = timer_read();
	i = 0;
	while (i < alarm_count) {
		if (alarm_deadlines[i] <= now) {
			serial_puts("alarm\r\n");
			alarm_deadlines[i] = alarm_deadlines[--alarm_count];
		} else {
			i++;
		}
	}

	timer_program_next();

	spin_unlock(&alarm_lock);
}
