#include <lib/stdio.h>
#include <lib/syscall.h>

#include <klib.h>
#include <irq.h>
#include <vm.h>

typedef struct {
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} SpWTransmitDescriptor;

#define FAKE_DEV MMIO_P2V(0x10030000)

static void fake_interrupt_handler(void) {
	kprintf(">>> interrupt\n");
	int a = *FAKE_DEV;
}

void send_package(uint32_t size)
{
	SpWTransmitDescriptor desc;
	desc.word0 = 13;
	desc.word1 = 17;
	desc.word2 = 23;
	desc.word3 = 29;

	*FAKE_DEV = V2P(&desc);
}

void _start()
{
	char command[128];
	int program_idx = 0;
	int child_pid = 0;

	register_interrupt_handler(18, fake_interrupt_handler);

	while (1) {
		printf("$ ");
		gets(command);

		if (command[0] == 's') {
			send_package(0);
		}

		if (command[0] < '0' || command[0] > '9')
			continue;

		program_idx = command[0] - '0';
		child_pid = fork();
		if (child_pid == 0) {
			exec(program_idx);
		}
		else {
			wait(child_pid);
		}
	}

	exit(0);
}
