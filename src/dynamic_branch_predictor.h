
#ifndef SRC_DYNAMIC_BRANCH_PREDICTOR_H_
#define SRC_DYNAMIC_BRANCH_PREDICTOR_H_

#include "abstract_branch_predictor.h"

typedef struct Predictor {
	int index_pc_bits;
	int index_btb_bits;
	int bht_entries;
	int bht_entry_width;
	int pht_width;
	int pht_values;
	int btb_size;
	int* bht; //16 entries (4-bit index) - 3-bit BHSR
	int* pht; //8 entries (3-bit BHSR) - 2 bit counter
} Predictor;

typedef struct BTB {
	uint32_t tag; //32-bit tag
	uint32_t target; //target address
} BTB;

/*
 * Dynamic branch predictor
 */
class DynamicBranchPredictor : public AbstractBranchPredictor {
public:
	DynamicBranchPredictor();
	virtual ~DynamicBranchPredictor();
	Predictor* predictor;
	BTB* btb;
	virtual uint32_t getTarget(uint32_t PC) override;
	virtual void update(uint32_t PC, bool taken, uint32_t target) override;
	virtual bool checkMisprediction(uint32_t PC, bool taken) override;
};

#endif /* SRC_DYNAMIC_BRANCH_PREDICTOR_H_ */