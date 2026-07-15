#include <kernel/serial.h>
#include <kernel/panic.h>
#include <kernel/string.h>
#include <arch/io.h>
#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/spinlock.h>

struct serialdev {
	char buf[SERIAL_BUF_SIZE];
	size_t len;
	struct spinlock lock;
};

static struct serialdev dev;

static inline u8 serial_reg_read(u64 reg)
{
	return ioread8((void *) ((u64) SERIAL_BASE + reg));
}

static inline void serial_reg_write(u64 reg, u8 val)
{
	iowrite8(val, (void *) ((u64) SERIAL_BASE + reg));
}

void serial_init()
{
	spin_init(&dev.lock);
	dev.len = 0;

	serial_reg_write(SERIAL_LCR, 0x3);
	serial_reg_write(SERIAL_FCR, SERIAL_FCR_FIFO_ENABLE |
			  SERIAL_FCR_RX_FIFO_CLEAR | SERIAL_FCR_TX_FIFO_CLEAR);
	serial_reg_write(SERIAL_IER, SERIAL_IER_ERBFI);
}

void serial_irq_enable()
{
	plic_irq_set_priority(IRQ_SERIAL, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, IRQ_SERIAL);
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq()
{
	char c;

	spin_lock(&dev.lock);

	while (serial_reg_read(SERIAL_LSR) & SERIAL_LSR_DTR) {
		c = (char) serial_reg_read(SERIAL_RBR);

		if (dev.len < SERIAL_BUF_SIZE)
			dev.buf[dev.len++] = c;

		if (c == '\r') {
			serial_putc('\r');
			serial_putc('\n');
		} else {
			serial_putc(c);
		}
	}

	spin_unlock(&dev.lock);
}

size_t serial_read(char *buf)
{
	u64 flags;
	size_t n;

	flags = spin_lock_irqsave(&dev.lock);

	n = dev.len;
	memcpy(buf, dev.buf, n);
	dev.len = 0;

	spin_unlock_irqrestore(&dev.lock, flags);

	return n;
}

void serial_putc(char c)
{
	while (!(serial_reg_read(SERIAL_LSR) & SERIAL_LSR_THRE)) { }
	serial_reg_write(SERIAL_THR, (u8) c);
}

void serial_puts(char *str)
{
	while (*str != '\0') {
		serial_putc(*str);
		str++;
	}
}
