#include "dynamic_branch_predictor.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include "config_reader.h"
DynamicBranchPredictor::DynamicBranchPredictor() {
	//Read config.json file
	FILE* config;
	char line[100];
	config = fopen("config.json", "r");
	// assert(config != NULL && "Could not open config file");
	std::string str;
	while (fscanf(config, "%s\n", line) != EOF) {
		str.append(line);
	}
	fclose(config);
	ConfigReader msg;
	msg.setJson(str);

	int bht_entries = msg.getValue("bht_entries").asInt();
	int pht_entries = pow(2, msg.getValue("bht_entry_width").asInt());
	// predictor = (struct Predictor*)malloc(
	// 	sizeof(struct Predictor*)
	// 	+ sizeof(int) * bht_entries + sizeof(int) * pht_entries);

	predictor = new Predictor;

	predictor->bht_entries = msg.getValue("bht_entries").asInt();
	predictor->bht_entry_width = msg.getValue("bht_entry_width").asInt();
	predictor->pht_width = msg.getValue("pht_width").asInt();
	predictor->btb_size = msg.getValue("btb_size").asInt();

	predictor->index_pc_bits = log2(predictor->bht_entries);
	predictor->pht_values = pow(2, predictor->pht_width);
	predictor->index_btb_bits = log2(predictor->btb_size);

	//Initialise BH, PHT and BTB with default values
	predictor->bht = new int[predictor->bht_entries];
	predictor->pht = new int[pht_entries];

	btb = new BTB[predictor->btb_size];
	for (int i = 0; i < predictor->btb_size; i++) {
		btb[i].tag = 0x00000000;
		btb[i].target = 0x00000000;
	}
}

DynamicBranchPredictor::~DynamicBranchPredictor() {

}

uint32_t DynamicBranchPredictor::getTarget(uint32_t PC) {
	// 1. Get the last log2(bht_entries) bits of PC to index into BHT and in turn index into PHT
	// 2. If prediction = 1: Branch taken else Branch not taken
	int prediction = 0;
	unsigned r = 0;
	for (unsigned i = 0; i <= predictor->index_pc_bits - 1; i++)
		r |= 1 << i;
	int index_pc = PC & r; //Get the last log2(bht_entries) bits of PC as index

	int bhtEntry = predictor->bht[index_pc];
	int predValue = predictor->pht[bhtEntry];

	if (predValue < predictor->pht_values / 2) {
		prediction = 0;
	}
	else {
		prediction = 1;
	}

	// 1. Get the last log2(btb_size) bits of PC to index into BTB and check for matching tag
	// 2. Get target address from BTB if branch is taken
	r = 0;
	for (unsigned i = 0; i <= predictor->index_btb_bits - 1; i++)
		r |= 1 << i;
	int index_btb = PC & r;  //Get the last log2(btb_size) bits of PC as index

	uint32_t target;
	if (btb[index_btb].tag == PC && prediction == 1) {
		target = btb[index_btb].target;
	}
	else {
		target = -1; //Not a branch or jump ie. actual target = PC + 4; Or Branch not yet in BTB
	}

	return target;
}

void DynamicBranchPredictor::update(uint32_t PC, bool taken, uint32_t target) {
	//Check if a misprediction occured

	unsigned r = 0;
	for (unsigned i = 0; i <= predictor->index_pc_bits - 1; i++)
		r |= 1 << i;
	int index_pc = PC & r; //Get the last log2(bht_entries) bits of PC as index

	int bhtEntry = predictor->bht[index_pc];
	int predValue = predictor->pht[bhtEntry];
	bool mispred = true;

	r = 0;
	for (unsigned i = 0; i <= predictor->index_btb_bits - 1; i++)
		r |= 1 << i;
	int index_btb = PC & r; //Get the last log2(btb_size) bits of PC as index

	if (btb[index_btb].tag == PC && predValue < predictor->pht_values / 2 && taken == true) {
		mispred = false;
	}
	else if (btb[index_btb].tag != PC && predValue >= predictor->pht_values / 2 && taken == false) {
		mispred = false;
	}
	// Update PHT and then BHT
	if (predValue < predictor->pht_values / 2 && taken == true) { //Misprediction
		predValue = (predValue + 1) % predictor->pht_values;
	}
	else if (predValue >= predictor->pht_values / 2 && taken == false) { //Misprediction
		predValue = (predValue - 1) % predictor->pht_values;
	}
	predictor->pht[bhtEntry] = predValue;
	//Get only the last bits
	int shiftedbits = predictor->bht[index_pc] << 1;
	if (taken) {
		shiftedbits += 1;
	}
	r = 0;
	for (unsigned i = 0; i <= predictor->bht_entry_width - 1; i++)
		r |= 1 << i;
	predictor->bht[index_pc] = shiftedbits & r;
	
	//If there is a misprediction and branch is taken, update BTB
	if (mispred == true && taken == true) {
		if (btb[index_btb].tag == PC) {
			btb[index_btb].target = target;
		}
		else {
			btb[index_btb].tag = PC;
			btb[index_btb].target = target;
		}
	}
}

bool DynamicBranchPredictor::checkMisprediction(uint32_t PC, bool taken) {
	//Check if a misprediction occured

	unsigned r = 0;
	for (unsigned i = 0; i <= predictor->index_pc_bits - 1; i++)
		r |= 1 << i;
	int index_pc = PC & r;

	int bhtEntry = predictor->bht[index_pc];
	int predValue = predictor->pht[bhtEntry];
	int prediction;
	if (predValue < predictor->pht_values / 2) {
		prediction = 0;
	}
	else {
		prediction = 1;
	}

	r = 0;
	for (unsigned i = 0; i <= predictor->index_btb_bits - 1; i++)
		r |= 1 << i;
	int index_btb = PC & r;

	uint32_t target;
	bool mispred = true;
	if (btb[index_btb].tag == PC && prediction == 1 && taken == true) {
		mispred = false;
	}
	else if (btb[index_btb].tag != PC && prediction == 0 && taken == false) {
		mispred = false;
	}
	return mispred;
}