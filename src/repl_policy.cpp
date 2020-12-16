/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include <cstdio>
#include "repl_policy.h"
#include "cache.h"

AbstarctReplacementPolicy::AbstarctReplacementPolicy(Cache* cache) :
		cache(cache) {
}

RandomRepl::RandomRepl(Cache* cache) :
		AbstarctReplacementPolicy(cache) {
}


Block* RandomRepl::getVictim(uint32_t addr, bool isWrite) {
	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	//first check if there is a free block to allocate
	for (int i = 0; i < (int) cache->getAssociativity(); i++) {
		if (cache->blocks[setIndex][i]->getValid() == false) {
			return cache->blocks[setIndex][i];
		}
	}
	//randomly choose a block
	int victim_index = rand() % cache->getAssociativity();
	return cache->blocks[setIndex][victim_index];
}

void RandomRepl::update(uint32_t addr, int way, bool isWrite) {
	//there is no metadata to update:D
	return;
}

/*
 * LRU replacement policy
 *
 * Reference - https://www.youtube.com/watch?v=bq6N7Ym81iI
 */
LRURepl::LRURepl(Cache *cache) :
		AbstarctReplacementPolicy(cache) {

	uint32_t numSets = cache->getNumSets();
	lruCounter = new uint32_t*[numSets];

	for (int i = 0; i < (int) numSets; i++) {
		lruCounter[i] = new uint32_t[cache->getAssociativity()];
		for (int j = 0; j < cache->getAssociativity(); j++)
			lruCounter[i][j] = 0;
	}

}


Block* LRURepl::getVictim(uint32_t addr, bool isWrite) {
	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	//first check if there is a free block to allocate
	for (int i = 0; i < (int) cache->getAssociativity(); i++) {
		if (cache->blocks[setIndex][i]->getValid() == false) {
			return cache->blocks[setIndex][i];
		}
	}

	int minCounterIdx = 0;
	for (int i = 0; i < cache->getAssociativity(); i++) {
		if (lruCounter[setIndex][i] == 0) {
			minCounterIdx = i;
			break;
		}
	}
	return cache->blocks[setIndex][minCounterIdx];
}

void LRURepl::update(uint32_t addr, int way, bool isWrite) {
	DPRINTF(DEBUG_MEMORY, "update LRU metadata :: START\n");

	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	for (int i = 0; i < cache->getAssociativity(); i++) {
		if(lruCounter[setIndex][i] > lruCounter[setIndex][way])
			lruCounter[setIndex][i] -= 1;
	}

	lruCounter[setIndex][way] = cache->getAssociativity() - 1;

	DPRINTF(DEBUG_MEMORY, "update LRU metadata :: DONE\n");

	return;
}

LRURepl::~LRURepl() {
	uint32_t numSets = cache->getNumSets();
	for (int i = 0; i < (int) numSets; i++) {
		delete lruCounter[i];
	}
	delete lruCounter;
}

/*
 * Pseudo LRU replacement policy
 *
 * Reference - https://www.youtube.com/watch?v=8CjifA2yw7s
 */
PLRURepl::PLRURepl(Cache* cache) :
		AbstarctReplacementPolicy(cache) {

	uint32_t numSets = cache->getNumSets();
	plruFlags = new bool*[numSets];

	for (int i = 0; i < (int) numSets; i++) {
		plruFlags[i] = new bool[cache->getAssociativity()];
		for (int j = 0; j < cache->getAssociativity(); j++)
			plruFlags[i][j] = false;
	}
}


Block* PLRURepl::getVictim(uint32_t addr, bool isWrite) {
	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	//first check if there is a free block to allocate
	for (int i = 0; i < (int) cache->getAssociativity(); i++) {
		if (cache->blocks[setIndex][i]->getValid() == false) {
			return cache->blocks[setIndex][i];
		}
	}
	// If all lines are valid, then
	// choose first line in set which has false bit
	int victimIdx = 0;
	for (int i = 0; i < cache->getAssociativity(); i++) {
		if (!plruFlags[setIndex][i]) {
			victimIdx = i;
			break;
		}
	}
	return cache->blocks[setIndex][victimIdx];
}

void PLRURepl::update(uint32_t addr, int way, bool isWrite) {
	DPRINTF(DEBUG_MEMORY, "update PLRU metadata :: START\n");

	addr = addr / cache->getBlockSize();
	uint64_t setIndex = (cache->getNumSets() - 1) & addr;

	plruFlags[setIndex][way] = true;
	bool isAllLinesOn = true;

	for (int i = 0; i < cache->getAssociativity(); i++) {
		if (!plruFlags[setIndex][i]) {
			isAllLinesOn = false;
			break;
		}
	}
	if(isAllLinesOn) {
		for (int i = 0; i < cache->getAssociativity(); i++) {
			plruFlags[setIndex][i] = false;
		}
		// Finally keep this access most recently line ON
		plruFlags[setIndex][way] = true;
	}

	DPRINTF(DEBUG_MEMORY, "update PLRU metadata :: DONE\n");

	return;
}

PLRURepl::~PLRURepl() {
	uint32_t numSets = cache->getNumSets();
	for (int i = 0; i < (int) numSets; i++) {
		delete plruFlags[i];
	}
	delete plruFlags;
}
