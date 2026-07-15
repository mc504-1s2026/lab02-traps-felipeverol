#include <kernel/printf.h>
#include <kernel/mm.h>
#include <kernel/string.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>

#define SHELL_LINE_MAX SERIAL_BUF_SIZE

static void shell_prompt()
{
	serial_puts("> ");
}

static void cmd_uptime()
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%lus\r\n", timer_read() / TIMER_FREQ);
	serial_puts(buf);
}

static void cmd_echo(char *arg)
{
	if (*arg == ' ')
		arg++;

	serial_puts(arg);
	serial_puts("\r\n");
}

static void cmd_alarm(char *arg)
{
	if (*arg == ' ')
		arg++;

	timer_set_alarm(strtou64(arg, 10));
}

static void shell_exec(char *line)
{
	if (strlen(line) == 0)
		return;

	if (strcmp(line, "uptime") == 0) {
		cmd_uptime();
	} else if (strncmp(line, "echo", 4) == 0 && (line[4] == ' ' || line[4] == '\0')) {
		cmd_echo(line + 4);
	} else if (strncmp(line, "alarm", 5) == 0 && (line[5] == ' ' || line[5] == '\0')) {
		cmd_alarm(line + 5);
	} else {
		serial_puts("unknown command\r\n");
	}
}

extern int _hartid[];
void kmain()
{
	char rbuf[SERIAL_BUF_SIZE];
	char line[SHELL_LINE_MAX];
	size_t line_len = 0;
	size_t n, i;
	char c;

	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	hart_irq_enable();

	shell_prompt();
	while (1) {
		n = serial_read(rbuf);

		for (i = 0; i < n; i++) {
			c = rbuf[i];

			if (c == '\r') {
				line[line_len] = '\0';
				shell_exec(line);
				line_len = 0;
				shell_prompt();
			} else if (line_len < SHELL_LINE_MAX - 1) {
				line[line_len++] = c;
			}
		}

		if (n == 0)
			__asm__ __volatile__("wfi");
	}
}
