/**
  *
  * a4vmsim.c
  *
  * Lorenzo Zafra 1395521
  * CMPUT 379 Assignment 4
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#include "stack.h"
#include "strategy.h"
#include "pagetable.h"
#include "a4vmsim.h"

t_output *output;
int page_numbits;
uint32_t* memory;

int main(int argc, char *argv[]) {
  if (argc < 4 || argc > 4) {
    print_error(E_USG);
  }

  int pagesize = atoi(argv[1]);
  if (pagesize < 256 || pagesize > 8192 || !ispowerof2(pagesize)) {
    print_error(E_PAGESIZE);
  }

  uint32_t memsize = atoi(argv[2]);
  if (memsize <= 0) {
    print_error(E_MEMSIZE);
  }
  memsize = roundNearMult(memsize, pagesize);

  char* tmpstrat = argv[3];
  strat_t strategy = -1;
  if (strcmp(tmpstrat, "none") == 0) {
    strategy = NONE;
  } else if (strcmp(tmpstrat, "mrand") == 0) {
    strategy = MRAND;
  } else if (strcmp(tmpstrat, "lru") == 0) {
    strategy = LRU;
  } else if (strcmp(tmpstrat, "sec") == 0) {
    strategy = SEC;
  } else {
    print_error(E_STRAT);
  }

  output = (t_output *) calloc(1, sizeof(t_output));
  struct timeval start, end;

  gettimeofday(&start, NULL);
  init(pagesize, memsize);
  simulate(pagesize, memsize, strategy);
  gettimeofday(&end, NULL);
  double elapsed = end.tv_sec - start.tv_sec;
  print_output(tmpstrat, elapsed);
}

void init(int pagesize, uint32_t memsize) {
  uint32_t numframes = memsize/pagesize;
  page_numbits = SYS_BITS - log2(pagesize);

  init_ptable(pow(2, page_numbits));
  memory = malloc(numframes * sizeof(uint32_t));
}

// Main function for the simulator
void simulate(int pagesize, uint32_t memsize, strat_t strat) {
  char ref_string[SYS_BITS/8];

  // TODO: remove debug
  printf("page_numbits: %i\n", page_numbits);

  while(read(STDIN_FILENO, ref_string, SYS_BITS/8)) {
    // Order of ref_string from MSB to LSB is:
    // ref[3] ref[2] ref[1] ref[0]
    printf("ref_string: %x", ref_string[3] & 0xff);
    printf("%x", ref_string[2] & 0xff);
    printf("%x", ref_string[1] & 0xff);
    printf("%x\n", ref_string[0] & 0xff);
    // do things here
    output->memrefs++;
    output->pagefaults += parse_operation(ref_string, strat);
  }
}

// Parses the reference string for the operation
int parse_operation(char ref_string[], strat_t strat) {
  uint32_t oper_byte = ref_string[0];
  // Shift 6 bits to the right since the opcode
  // resides in the 2 most significant bits
  uint32_t oper_bits = (oper_byte & 0xff) >> 6;

  // concat the page number bits
  uint32_t tmp = ((ref_string[3] & 0xff) << 24)
                | ((ref_string[2] & 0xff) << 16)
                | ((ref_string[1] & 0xff) << 8);

  // extract the page number
  tmp = tmp >> 8;
  uint32_t pNum = tmp & ((1 << page_numbits) - 1);

  switch(oper_bits) {
    case 0:
      inc_acc(oper_byte);
      return 0;
    case 1:
      dec_acc(oper_byte);
      return 0;
    case 2:
      return write_op(pNum, strat);
    case 3:
      return read_op(pNum, strat);
    default:
      print_error(E_OP_PARSE);
      return 0;
  }
}

// Increment Accumulator operation
void inc_acc(char oper_byte) {
  int value = (oper_byte & 0x3F);
  output->acc += value;
}

// Decrement Accumulator operation
void dec_acc(char oper_byte) {
  int value = (oper_byte & 0x3F);
  output->acc -= value;
}

// Write data to page containing reference word
int write_op(uint32_t pNum, strat_t strat) {
  output->writes++;
  printf("pNum: %i\n", pNum);

  //TODO
  // If there is a page fault, handle
  if (!check_pmem(pNum)) {
    handle_pfault(strat);

    return 1;
  }

  return 0;
}

// Read data from page containing reference word
int read_op(uint32_t pNum, strat_t strat) {
  printf("pNum: %i\n", pNum);
  //TODO
}

bool check_pmem(uint32_t v_addr) {
  uint32_t len = sizeof(memory) / sizeof(uint32_t);
  for (int i = 0; i < len; i++) {
    if(memory[i] == v_addr) {
      return true;
    }
  }
  return false;
}

void handle_pfault(strat_t strat) {
  switch (strat) {
    case NONE:
      none_handler();
      break;
    case MRAND:
      mrand_handler();
      break;
    case LRU:
      lru_handler();
      break;
    case SEC:
      sec_handler();
      break;
  }
}

// Prints the output of the simulator
void print_output(char* strategy, double elapsed) {
  printf("%i references processed using `%s` in %f sec.\n",
      output->memrefs, strategy, elapsed);
  printf("page faults = %i, write count = %i, flushes %i\n",
      output->pagefaults, output->writes, output->flushes);
  printf("accumulator = %i\n", output->acc);
}

// Checks if value is a power of two
bool ispowerof2(unsigned int x) {
   return x && !(x & (x - 1));
}

// Rounds up the value to the nearest multiple of 'multipleof'
uint32_t roundNearMult(uint32_t value, int multipleof) {
  uint32_t tmp = value % multipleof;
  if (tmp == 0) {
    return value;
  }
  return value + multipleof - tmp;
}

// Prints errors to the user depending on the code.
void print_error(int errorcode) {
  switch (errorcode) {
    case E_USG:
      fprintf(stderr, "usage: a4vmsim pagesize memsize strategy\n");
      break;
    case E_PAGESIZE:
      fprintf(stderr, "pagesize must be a power of two between 256 bytes and 8192 bytes\n");
      break;
    case E_MEMSIZE:
      fprintf(stderr, "memsize must be a valid integer\n");
      break;
    case E_STRAT:
      fprintf(stderr, "strategy must be one of [none, mrand, lru, sec]\n");
      break;
    case E_OP_PARSE:
      fprintf(stderr, "failed to parse the operation\n");
      break;
    case 6:
      fprintf(stderr, "\n");
      break;
    case 7:
      fprintf(stderr, "\n");
      break;
    case 8:
      fprintf(stderr, "\n");
      break;
    case 9:
      fprintf(stderr, "\n");
      break;
    case 10:
      fprintf(stderr, "\n");
      break;
  }
  exit(EXIT_FAILURE);
}
