/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#include <cstdlib>
#include <cstdio>
#include "repl_policy.h"
#include "next_line_prefetcher.h"
#include "cache.h"
#include "block.h"

Cache::Cache(uint32_t size, uint32_t associativity, uint32_t blkSize,
		enum ReplacementPolicy replType, uint32_t delay, enum CacheType cacheType):
		AbstractMemory(delay, 100),cSize(size),
		associativity(associativity), blkSize(blkSize), cacheType(cacheType) {

	numSets = cSize / (blkSize * associativity);
	blocks = new Block**[numSets];
	for (int i = 0; i < (int) numSets; i++) {
		blocks[i] = new Block*[associativity];
		for (int j = 0; j < associativity; j++)
			blocks[i][j] = new Block(blkSize);
	}

	switch (replType) {
	case RandomReplPolicy:
		replPolicy = new RandomRepl(this);
		break;
	case LRUReplPolicy:
		replPolicy = new LRURepl(this);
		break;
	case PLRUReplPolicy:
		replPolicy = new PLRURepl(this);
		break;
	default:
		assert(false && "Unknown Replacement Policy");
	}

	/*
	 * add other required initialization here
	 * (e.g: prefetcher and mshr)
	 */
	prefetcher = NULL;
	mshr = NULL;

}

Cache::~Cache() {
	delete replPolicy;
	for (int i = 0; i < (int) numSets; i++) {
		for (int j = 0; j < associativity; j++)
			delete blocks[i][j];
		delete blocks[i];
	}
	delete blocks;

}

uint32_t Cache::getAssociativity() {
	return associativity;
}

uint32_t Cache::getNumSets() {
	return numSets;
}

uint32_t Cache::getBlockSize() {
	return blkSize;
}

int Cache::getWay(uint32_t addr) {
	uint32_t _addr = addr / blkSize;
	uint32_t setIndex = (numSets - 1) & _addr;
	uint32_t addrTag = _addr / numSets;
	for (int i = 0; i < (int) associativity; i++) {
		if ((blocks[setIndex][i]->getValid() == true) && (blocks[setIndex][i]->getTag() == addrTag)) {
			return i;
		}
	}
	return -1;
}

uint32_t Cache::getTagValue(uint32_t addr) {
	uint32_t _addr = addr / this->blkSize;
	uint32_t addrTag = _addr / numSets;

	return addrTag;
}

Block* Cache::getCacheBlock(uint32_t addr) {
	uint32_t _addr = addr / blkSize;
	uint32_t setIndex = (numSets - 1) & _addr;

	int wayIdx = getWay(addr);

	if (wayIdx == -1) {
		return NULL;
	} else {
		return blocks[setIndex][wayIdx];
	}
}

void Cache::updateBlockDataWithPktData(Block* block, Packet* pkt) {
	uint32_t blockOffset = pkt->addr & (blkSize - 1);
	uint8_t *mem = block->getData();

	for (uint32_t i = blockOffset; i < (blockOffset + pkt->size); i++) {
		mem[i] = *(pkt->data + i - blockOffset);
	}
}

void Cache::updatePktDataWithBlockData(Block *block, Packet *pkt) {
	uint32_t blockOffset = pkt->addr & (blkSize - 1);
	uint8_t *mem = block->getData();

	for (uint32_t i = blockOffset; i < (blockOffset + pkt->size); i++) {
		*(pkt->data + i - blockOffset) = mem[i];
	}

}

bool Cache::sendReq(Packet * pkt){
	pkt->ready_time += accessDelay;

	DPRINTF(DEBUG_MEMORY, "request for %d cache for pkt : addr = %x, type = %d, size = %d, ready_time = %d\n",
			this->cacheType, pkt->addr, pkt->type, pkt->size, pkt->ready_time);

	Block* block = getCacheBlock(pkt->addr);

	// Should have been called from L2 to L1D/L1I
	if(pkt->type == PacketToInvalidate) {
		if(block)
			block->setValid(false);
		return true;
	}

	if(!block) {
		// Cache block not found, send the pkt to next level.

		// For load/i-fetch bring block data in "pkt->blockData"
		if(this->cacheType == L2 && !pkt->isWrite) {
			pkt->cacheBlockAddr = pkt->addr & ~(this->blkSize - 1);
			pkt->cacheBlockSize = this->blkSize;
			uint8_t* blkDataFill = new uint8_t[this->blkSize];
			pkt->cacheBlockData = blkDataFill;
		}

		this->next->sendReq(pkt);
	}
	else {
		if(pkt->isWrite) {
			// set the block to dirty and update the data in block
			block->setDirty(true);
			updateBlockDataWithPktData(block, pkt);

			// write-through policy so send the pkt to next level
			this->next->sendReq(pkt);
		}
		else {
			updatePktDataWithBlockData(block, pkt);
			reqQueue.push(pkt);
		}
		
		replPolicy->update(pkt->addr, getWay(pkt->addr), pkt->isWrite);
	}
	return true;
}

void Cache::recvResp(Packet* readRespPkt){

	DPRINTF(DEBUG_MEMORY, "recvResp for %d cache for pkt : addr = %x, type = %d, size = %d, ready_time = %d\n",
				this->cacheType, readRespPkt->addr, readRespPkt->type, readRespPkt->size, readRespPkt->ready_time);

	Block* block = getCacheBlock(readRespPkt->addr);

	if(readRespPkt->isWrite) {

		if(block) {
			block->setDirty(false);
		}

		// Now send the Pkt data: from L2 to L1D -or- from L1D to pipe
		if(this->cacheType == L2) {
			this->prevl1d->recvResp(readRespPkt);
		}
		else if(this->cacheType == L1D) {
			this->prev->recvResp(readRespPkt);
		}
	}
	else {
		// This will be executed when there was cache miss for lw / i-fetch insn.
		// So, either pkt is: from mem to L2 -or- from L2 to L1D/L1I
		if(!block) {
			// block not found in cache..
			// need to evict some other cache line and write it to cache
			Block* evictedBlock = replPolicy->getVictim(readRespPkt->addr, readRespPkt->isWrite);
			if(evictedBlock->getValid() && this->cacheType == L2) {
				// Need to evict the cache line from L1 too
				Packet* packetToInvalidate = new Packet(true, true, PacketToInvalidate, readRespPkt->addr,
						readRespPkt->size, NULL, readRespPkt->ready_time);

				if(readRespPkt->type == PacketTypeLoad)
					this->prevl1d->sendReq(packetToInvalidate);
				else if(readRespPkt->type == PacketTypeFetch)
					this->prevl1i->sendReq(packetToInvalidate);

				packetToInvalidate->data = nullptr;
				delete packetToInvalidate->data;
				delete packetToInvalidate;
			}
			// For all L2 and L1D/L1I replace 'evictedBlock' data with 'readRespPkt'
			evictedBlock->setTag(getTagValue(readRespPkt->cacheBlockAddr));

			for (uint32_t i = 0; i < readRespPkt->cacheBlockSize; i++) {
				evictedBlock->getData()[i] = *(readRespPkt->cacheBlockData + i);
			}

			evictedBlock->setDirty(false);
			evictedBlock->setValid(true);

			DPRINTF(DEBUG_MEMORY, "replaced evictedBlock in %d cache with pkt : addr = %x, type = %d, size = %d, ready_time = %d\n",
							this->cacheType, readRespPkt->cacheBlockAddr, readRespPkt->type, readRespPkt->cacheBlockSize, readRespPkt->ready_time);

			replPolicy->update(readRespPkt->addr, getWay(readRespPkt->addr), readRespPkt->isWrite);
		}
		else {
			block->setTag(getTagValue(readRespPkt->cacheBlockAddr));

			for (uint32_t i = 0; i < readRespPkt->cacheBlockSize; i++) {
				block->getData()[i] = *(readRespPkt->cacheBlockData + i);
			}

			block->setDirty(false);
		}

		// Now send the Pkt data: from L2 to L1D/L1I -or- from L1D/L1I to pipe
		if(this->cacheType == L2) {
			if(readRespPkt->type == PacketTypeLoad)
				this->prevl1d->recvResp(readRespPkt);
			else if(readRespPkt->type == PacketTypeFetch)
				this->prevl1i->recvResp(readRespPkt);
		}
		else if(this->cacheType == L1D || this->cacheType == L1I) {
			delete readRespPkt->cacheBlockData;
			this->prev->recvResp(readRespPkt);
		}
	}

	return;
}

void Cache::Tick(){

	while(!reqQueue.empty()) {
		//check if any packet is ready to be serviced
		if (reqQueue.front()->ready_time <= currCycle) {
			Packet* respPkt = reqQueue.front();
			reqQueue.pop();

			DPRINTF(DEBUG_MEMORY,"Tick():: cache send respond for pkt: addr = %x, ready_time = %d\n",
								respPkt->addr, respPkt->ready_time);

			// packet to be send as response to prev in memory hierarchy
			respPkt->isReq = false;

			// sw instruction
			if(respPkt->isWrite) {

				if(this->cacheType == L2) {
					this->prevl1d->recvResp(respPkt);
				}
				else if(this->cacheType == L1D){
					this->prev->recvResp(respPkt);
				}

			}
			else { // lw / i-fetch
				Block* block = getCacheBlock(respPkt->addr);

				// Set data in respPkt
				if(this->cacheType == L2) {

					respPkt->cacheBlockAddr = respPkt->addr & ~(this->blkSize - 1);
					respPkt->cacheBlockSize = this->blkSize;
					uint8_t* blkDataFill = new uint8_t[this->blkSize];
					respPkt->cacheBlockData = blkDataFill;

					for (uint32_t i = 0; i < respPkt->cacheBlockSize; i++) {
						*(respPkt->cacheBlockData + i) = block->getData()[i];
					}

				}

				updatePktDataWithBlockData(block, respPkt);

				// Now send the pkt as response to prev in memory hierarchy
				if(this->cacheType == L2 && respPkt->type == PacketTypeFetch) {
					this->prevl1i->recvResp(respPkt);
				}
				else if(this->cacheType == L2 && respPkt->type == PacketTypeLoad){
					this->prevl1d->recvResp(respPkt);
				}
				else if(this->cacheType == L1D || this->cacheType == L1I){
					this->prev->recvResp(respPkt);
				}
			}
		}
		else {
			/*
			 * assume that reqQueue is sorted by ready_time for now
			 * (because the pipeline is in-order)
			 */
			break;
		}
	}
	return;
}

void Cache::dumpRead(uint32_t addr, uint32_t size, uint8_t *data) {
	Block *block = getCacheBlock(addr);

	if (block) {
		uint32_t blockOffset = addr & (blkSize - 1);
		uint8_t *mem = block->getData();

		for (uint32_t i = blockOffset; i < (blockOffset + size); i++) {
			*(data + i - blockOffset) = mem[i];
		}
	} else {
		this->next->dumpRead(addr, size, data);
	}

}
