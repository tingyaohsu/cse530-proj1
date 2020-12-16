/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdio>
#include <iostream>
#include "simulator.h"
#include "util.h"

uint64_t currCycle;

Simulator::Simulator(MemHrchyInfo* info) {
	currCycle = 0;
	printf("initialize simulator\n\n");
	//initializing core
	pipe = new PipeState();

	/*
	 * initializing memory hierarchy
	 *  you should update this for adding the caches
	 */
	main_memory = new BaseMemory(info->memDelay);
	main_memory->next = nullptr;

	// CSE530: add caches
	main_memory = new BaseMemory(info->memDelay);
	l1DCache = new Cache(info->cache_size_l1, info->cache_assoc_l1, info->cache_blk_size, info->repl_policy_l1d, info->access_delay_l1, L1D);
	l1ICache = new Cache(info->cache_size_l1, info->cache_assoc_l1, info->cache_blk_size, info->repl_policy_l1i, info->access_delay_l1, L1I);
	l2Cache = new Cache(info->cache_size_l2, info->cache_assoc_l2, info->cache_blk_size, info->repl_policy_l2, info->access_delay_l2, L2);

	//set the responder for memory operations
	l1ICache->next = l2Cache;
	l1DCache->next = l2Cache;
	l1ICache->prev = pipe;
	l1DCache->prev = pipe;

	l2Cache->next = main_memory;
	l2Cache->prevl1i = l1ICache;
	l2Cache->prevl1d = l1DCache;

	main_memory->next = nullptr;
	main_memory->prev = l2Cache;

	//set the first memory in the memory-hierarchy
	pipe->data_mem = l1DCache;
	pipe->inst_mem = l1ICache;
}



void Simulator::cycle() {
	//check if memory needs to respond to any packet in this cycle
	main_memory->Tick();

	// CSE530: check if caches need to respond to any packet in this cycle
	l2Cache->Tick();
	l1DCache->Tick();
	l1ICache->Tick();

	//progress of the pipeline in this clock
	pipe->pipeCycle();
	pipe->stat_cycles++;
	// increment the global clock of the simulator
	currCycle++;
}



void Simulator::run(int num_cycles) {
	int i;
	if (pipe->RUN_BIT == false) {
		printf("Can't simulate, Simulator is halted\n\n");
		return;
	}

	printf("Simulating for %d cycles...\n\n", num_cycles);
	for (i = 0; i < num_cycles; i++) {
		if (pipe->RUN_BIT == false) {
			printf("Simulator halted\n\n");
			break;
		}
		cycle();
	}
}


void Simulator::go() {
	if (pipe->RUN_BIT == false) {
		printf("Can't simulate, Simulator is halted\n\n");
		return;
	}

	printf("Simulating...\n\n");
	while (pipe->RUN_BIT)
		cycle();
	printf("Simulator halted\n\n");
}



uint32_t Simulator::readMemForDump(uint32_t address) {
	uint32_t data;
	//you should update this since adding the caches
	pipe->data_mem->dumpRead(address, 4, (uint8_t*) &data);
	return data;
}

void Simulator::registerDump() {
	int i;

	printf("PC: 0x%08x\n", pipe->PC);

	for (i = 0; i < 32; i++) {
		printf("R%d: 0x%08x\n", i, pipe->REGS[i]);
	}

	printf("HI: 0x%08x\n", pipe->HI);
	printf("LO: 0x%08x\n", pipe->LO);
	printf("Cycles: %u\n", pipe->stat_cycles);
	printf("FetchedInstr: %u\n", pipe->stat_inst_fetch);
	printf("RetiredInstr: %u\n", pipe->stat_inst_retire);
	printf("IPC: %0.3f\n",
			((float) pipe->stat_inst_retire) / pipe->stat_cycles);
	printf("Flushes: %u\n", pipe->stat_squash);
}

void Simulator::memDump(int start, int stop) {
	int address;

	printf("\nMemory content [0x%08x..0x%08x] :\n", start, stop);
	printf("-------------------------------------\n");
	for (address = start; address < stop; address += 4) {
		printf("MEM[0x%08x]: 0x%08x\n", address, readMemForDump(address));
	}
	printf("\n");
}

Simulator::~Simulator() {
	delete main_memory;
	delete pipe;
}
