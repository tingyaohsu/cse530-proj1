/*
 * Computer Architecture CSE530
 * MIPS pipeline cycle-accurate simulator
 * PSU
 */

#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <cstdint>
#include <cassert>
#include "util.h"

/*
 * Cache Block
 */
class Block {
private:
	uint8_t blkSize;
	uint8_t* data;
	uint32_t tag;
	bool dirty;
	bool valid;
public:
	bool getValid() {
		return valid;
	}

	bool getDirty() {
		return dirty;
	}

	uint32_t getTag() {
		return tag;
	}

	uint8_t* getData() {
		return data;
	}

	void setData(uint8_t* _data) {
		data = _data;
	}

	void setValid(bool flag) {
		valid = flag;
	}

	void setDirty(bool flag) {
		dirty = flag;
	}

	void setTag(uint32_t _tag) {
		tag = _tag;
	}

	void clear(uint32_t blk_tag) {
		dirty = false;
		tag = blk_tag;
		valid = true;
	}

	Block(uint8_t blkSize) :
			blkSize(blkSize) {
		valid = false;
		dirty = false;
		data = new uint8_t[blkSize];
	}

	~Block() {
		delete (data);
	}

};

#endif
