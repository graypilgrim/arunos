#include <console.h>
#include <vm.h>
#include <kalloc.h>
#include <monitor.h>
#include <proc.h>
#include <timer.h>
#include <klib.h>
#include <irq.h>

#define SPACE_WIRE_BASE MMIO_P2V(0x10030000)
#define SPACE_WIRE_RCV (SPACE_WIRE_BASE + 1)
#define SPACE_WIRE_TRNS (SPACE_WIRE_BASE + 2)

#define TABLE_SIZE 16

#define RCV_DESCRIPTOR_WRAP_MASK (0x1 << 26)
#define RCV_DESCRIPTOR_ENABLE_MASK (0x1 << 25)

uint8_t defaultHeader[15];

typedef struct {
    uint32_t word0;
    uint32_t word1;
} SpWReceiveDescriptor;

#define TRNS_DESCRIPTOR_WRAP_MASK (0x1 << 13)
#define TRNS_DESCRIPTOR_ENABLE_MASK (0x1 << 12)

typedef struct {
    uint32_t word0;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} SpWTransmitDescriptor;

static void fake_interrupt_handler(void) {
	kprintf(">>> interrupt\n");
	int a = *SPACE_WIRE_BASE;
}

void set_receive_descriptor_table()
{
  static SpWReceiveDescriptor table[TABLE_SIZE];

  for (int i = 0; i < TABLE_SIZE; ++i) {
    if (i < (TABLE_SIZE - 1))
      table[i].word0 &= ~RCV_DESCRIPTOR_WRAP_MASK;
    else
      table[i].word0 |= RCV_DESCRIPTOR_WRAP_MASK;

    table[i].word0 |= RCV_DESCRIPTOR_ENABLE_MASK;
    table[i].word1 = 0xDEADBEEF;

    // kprintf("%d: %x %x\n", i, table[i].word0, table[i].word1);
  }

  *SPACE_WIRE_RCV = V2P(table);
}

void set_transmit_descriptor_table(uint32_t len)
{
  static SpWTransmitDescriptor table[TABLE_SIZE];

  for (int i = 0; i < TABLE_SIZE; ++i) {
    table[i].word0 = 0;

    if (i < (TABLE_SIZE - 1))
      table[i].word0 &= ~TRNS_DESCRIPTOR_WRAP_MASK;
    else
      table[i].word0 |= TRNS_DESCRIPTOR_WRAP_MASK;

    table[i].word0 |= TRNS_DESCRIPTOR_ENABLE_MASK;

    defaultHeader[12] = len & 0xFF0000;
    defaultHeader[13] = len & 0xFF00;
    defaultHeader[14] = len & 0xFF;

    table[i].word0 += 15; //header len
    table[i].word1 = V2P(defaultHeader);
    table[i].word2 = len;
    table[i].word3 = 0xBADC0FFE;

    // kprintf("%d: %x %x | %x %x\n", i, table[i].word0, table[i].word1, defaultHeader[0], defaultHeader[1]);
  }

  *SPACE_WIRE_TRNS = V2P(table);
}

void fill_header(uint8_t* header)
{
  header[0] = 0xa;
  header[1] = 0x01;
  header[2] = 0xb;
  header[3] = 0xc;
  header[4] = 0xd;
  header[5] = 0xe;
  header[6] = 0xf;
  header[7] = 0xaa;
  header[8] = 0xab;
  header[9] = 0xac;
  header[10] = 0xad;
  header[11] = 0xae;
  header[12] = 0xaf;
  header[13] = 0xba;
  header[14] = 0xbb;
  header[15] = 0xbc;
}

/*
 * Kernel starts executing C code here. Before entring this function, some
 * initialization has been done in the assembly code.
 */
void c_entry(void)
{
	kalloc_init(ALLOCATABLE_MEMORY_START,
		    KERNEL_BASE + INITIAL_MEMORY_SIZE);
	vm_init();
	proc_init();
	console_init();
	kalloc_init(KERNEL_BASE + INITIAL_MEMORY_SIZE,
		    KERNEL_BASE + TOTAL_MEMORY_SIZE);

  register_interrupt_handler(18, fake_interrupt_handler);

  char command[128];
  fill_header(defaultHeader);
  // kprintf("header address: %x", (uint32_t)&defaultHeader);

  while (1) {
    kgets(command);

    if (command[0] == 's') {
      set_receive_descriptor_table();
    }

    if (command[0] == 't') {
      char* ptr = command + 2;
      int size_len = 0;
      while (*ptr) {
        ++size_len;
        ++ptr;
      }

      ptr = command + 2 + (size_len - 1);
      uint32_t base = 1;
      uint32_t len = 0;

      while (size_len > 0) {
        // kprintf("ptr: %d\n", *ptr - '0');
        len += base * (*ptr - '0');
        base *= 10;
        --size_len;
        --ptr;
      }

      // kprintf("len: %d\n", len);
      set_transmit_descriptor_table(len);
    }
  }

	/* start the first program */
	{
		struct Process *proc = proc_create();
		proc_load_program(proc, 0);
		scheduler_init();
		schedule();
	}
}
