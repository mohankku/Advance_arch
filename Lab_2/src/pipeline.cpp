/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Soham J. Desai 
 * Date         : 14th January 2014
 * Description  : Superscalar Pipeline for Lab2 ECE 6100
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>

extern int32_t PIPE_WIDTH;
extern int32_t ENABLE_MEM_FWD;
extern int32_t ENABLE_EXE_FWD;
extern int32_t BPRED_POLICY;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Op
 **********************************************************************/

void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
    uint8_t bytes_read = 0;
    bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);

    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
      fetch_op->valid=false;
      p->halt_op_id=p->op_id_tracker;
      return;
    }

    // got an instruction ... hooray!
    fetch_op->valid=true;
    fetch_op->stall=false;
    fetch_op->is_mispred_cbr=false;
    p->op_id_tracker++;
    fetch_op->op_id=p->op_id_tracker;
    
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->tr_file = tr_file_in;
    p->halt_op_id = ((uint64_t)-1) - 3;           

    // Allocated Branch Predictor
    if(BPRED_POLICY){
      p->b_pred = new BPRED(BPRED_POLICY);
    }

    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;

    uint8_t latch_type_i = 0;   // Iterates over Latch Types
    uint8_t width_i      = 0;   // Iterates over Pipeline Width
    for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
        switch(latch_type_i) {
            case 0:
                printf(" FE: ");
                break;
            case 1:
                printf(" ID: ");
                break;
            case 2:
                printf(" EX: ");
                break;
            case 3:
                printf(" MEM: ");
                break;
            default:
                printf(" ---- ");
        }
    }
    printf("\n");
    for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
        for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
            if(p->pipe_latch[latch_type_i][width_i].valid == true) {
	      printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
            } else {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
	//printf("%d\t %d\t %d\t %d\t %d\t %d\t",p->pipe_latch[0][0].tr_entry.op_type, p->pipe_latch[0][0].tr_entry.mem_read, p->pipe_latch[0][0].tr_entry.mem_write, p->pipe_latch[0][0].tr_entry.dest_needed, p->pipe_latch[0][0].tr_entry.src1_needed, p->pipe_latch[0][0].tr_entry.src2_needed);
    printf("\n");

}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);
	    
}
/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(p->pipe_latch[MEM_LATCH][ii].valid){
      p->stat_retired_inst++;
	  if (p->pipe_latch[MEM_LATCH][ii].tr_entry.op_type == OP_CBR) {
 	    //p->b_pred->UpdatePredictor(p->pipe_latch[MEM_LATCH][ii].tr_entry.inst_addr, p->pipe_latch[MEM_LATCH][ii].tr_entry.br_dir, false); 
		if (p->pipe_latch[MEM_LATCH][ii].op_id == p->op_id) {
	      p->fetch_cbr_stall = false;
		}
	  }
      if(p->pipe_latch[MEM_LATCH][ii].op_id >= p->halt_op_id){
	    p->halt=true;
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
  }
}

//--------------------------------------------------------------------//
// returns true if there is no data dependency
bool check_data_dependency_exe(Pipeline *p, int entry) {
	Trace_Rec *exec_tr_entry, *fe_tr_entry;
	int ii;
 
	fe_tr_entry = &p->pipe_latch[FE_LATCH][entry].tr_entry;

	
    for(ii=0; ii<PIPE_WIDTH; ii++){
		exec_tr_entry = &p->pipe_latch[EX_LATCH][ii].tr_entry;

		if (p->pipe_latch[EX_LATCH][ii].valid) {
			if (exec_tr_entry->dest_needed) {
				if (fe_tr_entry->src1_needed) {
					if (fe_tr_entry->src1_reg == exec_tr_entry->dest) {
						return false;
					}
				}
				if (fe_tr_entry->src2_needed) {
					if (fe_tr_entry->src2_reg == exec_tr_entry->dest) {
						return false;
					}
				}
			}
			if (fe_tr_entry->cc_read) {
				if (exec_tr_entry->cc_write) {
					return false;
				}
			}
		}
	}
	return true;
}

bool check_fwd_data_dependency_exe(Pipeline *p, int entry) {
	Trace_Rec *exec_tr_entry, *fe_tr_entry;
	int ii;
 
	fe_tr_entry = &p->pipe_latch[FE_LATCH][entry].tr_entry;
	
    for(ii=0; ii<PIPE_WIDTH; ii++){
		exec_tr_entry = &p->pipe_latch[EX_LATCH][ii].tr_entry;

		if (p->pipe_latch[EX_LATCH][ii].valid) {
			if (exec_tr_entry->op_type == OP_LD) {
				if (exec_tr_entry->dest_needed) {
					if (fe_tr_entry->src1_needed) {
						if (fe_tr_entry->src1_reg == exec_tr_entry->dest) {
							return false;
						}
					}
					if (fe_tr_entry->src2_needed) {
						if (fe_tr_entry->src2_reg == exec_tr_entry->dest) {
							return false;
						}
					}
				}
			    if ((fe_tr_entry->op_type == OP_CBR) && (fe_tr_entry->cc_read)) {
				    if (exec_tr_entry->cc_write) {
					    return false;
				    }
				}
			}
		}
	}
	return true;
}

bool check_data_dependency_mem(Pipeline *p, int entry) {
	Trace_Rec *mem_tr_entry, *fe_tr_entry;
	int ii;
 
	fe_tr_entry = &p->pipe_latch[FE_LATCH][entry].tr_entry;

    for(ii=0; ii<PIPE_WIDTH; ii++){
		mem_tr_entry = &p->pipe_latch[MEM_LATCH][ii].tr_entry;
		if (p->pipe_latch[MEM_LATCH][ii].valid) {
			if (mem_tr_entry->dest_needed) {
				if (fe_tr_entry->src1_needed) {
					if (fe_tr_entry->src1_reg == mem_tr_entry->dest) {
						return false;
					}	
				} 
				if (fe_tr_entry->src2_needed) {
					if (fe_tr_entry->src2_reg == mem_tr_entry->dest) {
						return false;
					}
				}
			}
			if (fe_tr_entry->cc_read) {
				if (mem_tr_entry->cc_write) {
					return false;
				}
			}
		}
	}
	return true;
}

bool check_data_dependency_local(Pipeline *p, int entry) {
	Trace_Rec *prev_tr_entry, *fe_tr_entry;
	int ii;

	if (entry+1 >= PIPE_WIDTH) {
		return true;
	}

	fe_tr_entry = &p->pipe_latch[FE_LATCH][entry].tr_entry;
	prev_tr_entry = &p->pipe_latch[FE_LATCH][entry+1].tr_entry;


	if (p->pipe_latch[FE_LATCH][entry+1].valid) {
		if (fe_tr_entry->dest_needed) {
			if (prev_tr_entry->src1_needed) {
				if (prev_tr_entry->src1_reg == fe_tr_entry->dest) {
					return false;
				}
			}
			if (prev_tr_entry->src2_needed) {
				if (prev_tr_entry->src2_reg == fe_tr_entry->dest) {
					return false;
				}
			}
		}
		if (prev_tr_entry->cc_read) {
			if (fe_tr_entry->cc_write) {
				return false;
			}
		}
	}
	return true;
}

bool check_fwd_data_dependency_local(Pipeline *p, int entry) {
	Trace_Rec *prev_tr_entry, *fe_tr_entry;
	int ii;

	if (entry+1 >= PIPE_WIDTH) {
		return true;
	}

	fe_tr_entry = &p->pipe_latch[FE_LATCH][entry].tr_entry;
	prev_tr_entry = &p->pipe_latch[FE_LATCH][entry+1].tr_entry;


	if (p->pipe_latch[FE_LATCH][entry+1].valid) {
		//if ((fe_tr_entry->op_type == OP_LD) ||
		//	(fe_tr_entry->op_type == OP_ALU)){
			if (fe_tr_entry->dest_needed) {
				if (prev_tr_entry->src1_needed) {
					if (prev_tr_entry->src1_reg == fe_tr_entry->dest) {
						return false;
					}
				}
				if (prev_tr_entry->src2_needed) {
					if (prev_tr_entry->src2_reg == fe_tr_entry->dest) {
						return false;
					}
				}
			}
			if ((prev_tr_entry->op_type == OP_CBR) && (prev_tr_entry->cc_read)) {
				if (fe_tr_entry->cc_write) {
					return false;
				}
			}
		//}
	}
	return true;
}

void pipe_cycle_ID(Pipeline *p){
  int ii, jj;
  bool stalled = false;

  for(ii=0; ii<PIPE_WIDTH; ii++){
	if((ENABLE_MEM_FWD) && (ENABLE_EXE_FWD)){
		if (stalled) {
			p->pipe_latch[FE_LATCH][ii].stall = true;
			p->pipe_latch[ID_LATCH][ii].valid = false;
			continue;
		}
		if (check_fwd_data_dependency_exe(p,ii)) {
			// True if no dependency
			p->pipe_latch[FE_LATCH][ii].stall = false;
			p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
			p->pipe_latch[FE_LATCH][ii].valid = false;
			if (!check_fwd_data_dependency_local(p, ii)) {
				// TRUE if dependency
				stalled = true;
			}
		} else {
			p->pipe_latch[FE_LATCH][ii].stall = true;
			p->pipe_latch[ID_LATCH][ii].valid = false;
			stalled = true;
		}
		continue;
	}

	if (stalled) {
		p->pipe_latch[FE_LATCH][ii].stall = true;
		p->pipe_latch[ID_LATCH][ii].valid = false;
		continue;
	}

    if ((check_data_dependency_exe(p, ii)) && (check_data_dependency_mem(p, ii))) {
		// True if no dependency
		p->pipe_latch[FE_LATCH][ii].stall = false;
		p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
		p->pipe_latch[FE_LATCH][ii].valid = false;
		if (!check_data_dependency_local(p, ii)) {
			// TRUE if dependency
				stalled = true;
		}
	} else {
		p->pipe_latch[FE_LATCH][ii].stall = true;
		p->pipe_latch[ID_LATCH][ii].valid = false;
		stalled = true;
    }
  }

  // push entries to the bottom
  for(ii=PIPE_WIDTH-1; ii>0; ii--){
      if (p->pipe_latch[FE_LATCH][ii].valid) {
		  if (!p->pipe_latch[FE_LATCH][ii-1].valid) {
			  for (jj=ii-1; jj<PIPE_WIDTH-1; jj++){
				  p->pipe_latch[FE_LATCH][jj] = p->pipe_latch[FE_LATCH][jj+1];
				  p->pipe_latch[FE_LATCH][jj+1].valid = false;
			  }
		  }
	  }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p){
  int ii, entries;
  Pipeline_Latch fetch_op;
  bool tr_read_success;
  int free_id = 0;

  for(ii=0; ii<PIPE_WIDTH; ii++){
	if (p->pipe_latch[FE_LATCH][ii].valid) {
		continue;
	}

	if (p->fetch_cbr_stall) {
		p->pipe_latch[FE_LATCH][ii].valid = false;
		continue;
	}
	
    pipe_get_fetch_op(p, &fetch_op);

    if(BPRED_POLICY){
      pipe_check_bpred(p, &fetch_op);
    }
    
    // copy the op in FE LATCH
    p->pipe_latch[FE_LATCH][ii]=fetch_op;
  }
  
}

//--------------------------------------------------------------------//

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
  if (fetch_op->valid) {
	if (fetch_op->tr_entry.op_type == OP_CBR) {
	  p->b_pred->stat_num_branches++;
 	  if (p->b_pred->GetPrediction(fetch_op->tr_entry.inst_addr) != fetch_op->tr_entry.br_dir) { 
 	    p->b_pred->UpdatePredictor(fetch_op->tr_entry.inst_addr, fetch_op->tr_entry.br_dir, false);
        p->b_pred->stat_num_mispred++;
        p->fetch_cbr_stall = true;
		p->op_id = fetch_op->op_id;
 	  } else {
 	    p->b_pred->UpdatePredictor(fetch_op->tr_entry.inst_addr, fetch_op->tr_entry.br_dir, true);
	  }
	}
  }
}
//--------------------------------------------------------------------//
