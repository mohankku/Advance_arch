#include "bpred.h"
#include "stdio.h"

#define TAKEN   true
#define NOTTAKEN false

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

BPRED::BPRED(uint32_t policy) {
	int i;
	BPRED::policy = policy;
	
	if (policy == BPRED_GSHARE) {
		for(i=0; i<LIST_LENGTH; i++) {
			BPRED::bp[i].pred = 2;
			BPRED::ghr = 0;
		}
	} 
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool BPRED::GetPrediction(uint64_t PC){
	uint64_t index, temp1, temp2;
	if (policy == BPRED_ALWAYS_TAKEN) {
		return TAKEN;
	}
	if (policy == BPRED_GSHARE) {
		temp1 = PC << 52;
		temp2 = BPRED::ghr << 52;
		index = temp1 ^ temp2;
		index >>= 52;
		//printf("%d %d\n", index, bp[index].pred);
		if (bp[index].pred > 1) {
			return TAKEN;
		} else {
			return NOTTAKEN;
		}
	}
	return TAKEN;
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void  BPRED::UpdatePredictor(uint64_t PC, bool resolveDir, bool predDir) {
	uint64_t index, temp1, temp2;
	if (policy == BPRED_GSHARE) {
		temp1 = PC << 52;
		temp2 = ghr << 52;
		index = temp1 ^ temp2;
		index >>= 52;
		if (resolveDir) {
			if (bp[index].pred != 3) {
				bp[index].pred++;
			}
		} else {
			if (bp[index].pred != 0) {
				bp[index].pred--;
			}
		}
		//printf("update %d %d\n", index, bp[index].pred);
		ghr = ghr << 1;
		ghr |= resolveDir;
		//printf("update %d \n", ghr);
	}
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

