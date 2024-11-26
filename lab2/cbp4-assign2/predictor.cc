#include "predictor.h"
#include <stdio.h>

#include <bitset>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstdint>

using namespace std;

/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

// 8192 bits = 2 bits * 4096 entries
uint8_t twoBit[4096];

void InitPredictor_2bitsat() {
  for (int i = 0; i < 4096; i++) {
    twoBit[i] = 1; //init to weak NT
  }
}

bool GetPrediction_2bitsat(UINT32 PC) {
  return twoBit[PC & 4095] > 1;
}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int index = (PC) & 4095;
  if (resolveDir && twoBit[index] < 3)
    twoBit[index]++;
  else if (!resolveDir && twoBit[index] > 0)
    twoBit[index]--;
}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

// 512 entries * 6 bits/entry = 3072 bits
uint8_t BHR[512];
// 8 tables * 2^6 entries/table * 2 bits/entry = 1024 bits
uint8_t BPB[8][64];

void InitPredictor_2level() {
  for(int i = 0; i < 512; i++){
    BHR[i] = 0; // init to 000000
  }
  for(int i = 0; i < 8; i++){
    for(int j = 0; j < 64; j++){
      BPB[i][j] = 1; // init to Weak NT
    }
  }
}

bool GetPrediction_2level(UINT32 PC) {
  int BHR_entry = BHR[(PC >> 3) & 511];
  return BPB[PC & 7][BHR_entry] > 1;
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
  int BHR_index = (PC >> 3) & 511;
  int BPB_index = (PC & 7);
  int history = BHR[BHR_index];

  BHR[BHR_index] = ((BHR[BHR_index] << 1) + resolveDir) & 63;

  if (resolveDir && BPB[BPB_index][history] < 3)
    BPB[BPB_index][history]++;
  else if (!resolveDir && BPB[BPB_index][history] > 0)
    BPB[BPB_index][history]--;
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

// constants and configurations
// 130 timeout, 195 test1, 195 test2, 200 test3
#define GLOBAL_HISTORY_LENGTH 200   // length of the global history register
#define NUM_COMPONENTS 8            // number of tagged components
#define MAX_U_COUNTER 3             // maximum value for the useful counter (2-bit counter)
#define MAX_CTR 3                   // maximum value for the prediction counter (3-bit signed counter)
#define MIN_CTR -4                  // minimum value for the prediction counter (3-bit signed counter)
#define BASE_PREDICTOR_SIZE 8192    // number of entries in bimodel predictor
#define RESET_PERIOD (256 * 1024)     // reset usefulness bit for every x instructions
#define NOT_DEFINED -1

struct TaggedEntry {
    int8_t ctr;     // 3-bit signed counter (-4 to +3)
    uint16_t tag;   // tag to identify the branch
    uint8_t u;      // 2-bit usefulness counter
};

class TAGEPredictor {
private:
    vector<uint8_t> base_predictor;   // bimodel
    vector<vector<TaggedEntry>> tagged_components;
    std::vector<int> tag_bits;  // tag lengths for each component
    vector<int> history_lengths;   // history lengths for each component
    bitset<GLOBAL_HISTORY_LENGTH> global_history;

    // prediction state variables
    int provider_component;       // index of the provider component
    int provider_entry_index;     // entry index in the provider component
    int altpred_component;        // index of the alternative component
    int altpred_entry_index;      // entry index in the alternative component
    bool pred_taken;              // prediction made
    int num_branches;             // number of branches processed (for usefulness reset)

public:
    TAGEPredictor();
    bool predict(UINT32 PC);
    void update(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget);

private:
    int compute_index(UINT32 PC, int component);
    uint16_t compute_tag(UINT32 PC, int component);
    UINT32 get_history_hash(int length);
    void reset_prediction_state();
    void periodic_reset();
};

TAGEPredictor::TAGEPredictor() {

    // seed the random number generator (used in allocation)
    srand(time(NULL));
    
    base_predictor.resize(BASE_PREDICTOR_SIZE, 2);   // init to weak taken
    tagged_components.resize(NUM_COMPONENTS);
    tag_bits.resize(NUM_COMPONENTS);

    // TODO: PLAY AROUND WITH THESE VALUES
    // history_lengths = {4, 8, 16, 32, 64, 80, 100, 130};   // history lengths for each component, timeout 6.1811
    // history_lengths = {4, 11, 20, 35, 70, 103, 156, 195};    // test 1   6.172
    // history_lengths = {4, 11, 20, 35, 70, 103, 156, 195};    // test 2   6.034125
    // history_lengths = {4, 11, 20, 35, 70, 103, 156, 195};    // test 3 probability   5.98725
    history_lengths = {4, 11, 24, 56, 87, 119, 157, 200};    // test 4 probability   
    // initialize the tagged components
    for (int i = 0; i < NUM_COMPONENTS; i++) {
        int table_size;
        int current_tag_bits;

        // set table sizes and tag lengths based on component index
        // TODO: PLAY AROUND WITH THESE VALUES
        switch (i) {
            case 0:
                table_size = 2048; // T1
                current_tag_bits = 8;
                break;
            case 1:
                table_size = 2048; // T2
                current_tag_bits = 9;
                break;
            case 2:
                table_size = 1024; // T3
                current_tag_bits = 10;
                break;
            case 3:
                table_size = 1024; // T4
                current_tag_bits = 11;
                break;
            case 4:
                table_size = 512;  // T5
                current_tag_bits = 12;
                break;
            case 5:
                table_size = 512;  // T6
                current_tag_bits = 13;        //changed to 13 for test 2
                break;
            case 6:
                table_size = 256;  // T7
                current_tag_bits = 13;        //changed to 13 for test 2
                break;
            case 7:
                table_size = 256;  // T8
                current_tag_bits = 13;        //changed to 13 for test 2
                break;
            default:
                table_size = 512;
                current_tag_bits = 12;
                break;
        }

        tag_bits[i] = current_tag_bits;

        tagged_components[i].resize(table_size);

        // initialize entries
        for (int j = 0; j < table_size; j++) {
            tagged_components[i][j].ctr = 0;
            tagged_components[i][j].tag = 0;
            tagged_components[i][j].u = 0;
        }
    }

    global_history.reset();
    provider_component = NOT_DEFINED;
    altpred_component = NOT_DEFINED;
    pred_taken = true;  // default
    num_branches = 0;
}

bool TAGEPredictor::predict(UINT32 PC) {

    num_branches++;
    reset_prediction_state();

    // initialize provider and altpred with bimodel prediction
    int base_index = PC % base_predictor.size();
    bool base_pred = base_predictor[base_index] >= 2;
    bool provider_pred = base_pred;
    bool altpred_pred = base_pred;

    // search the tagged components
    for (int i = NUM_COMPONENTS - 1; i >= 0; i--) {
        
        int index = compute_index(PC, i);
        uint16_t tag = compute_tag(PC, i);

        if (tagged_components[i][index].tag == tag) {
            if (provider_component == NOT_DEFINED) {
                provider_component = i;
                provider_entry_index = index;
                provider_pred = tagged_components[i][index].ctr >= 0;
            } 
            else if (altpred_component == NOT_DEFINED) {
                altpred_component = i;
                altpred_entry_index = index;
                altpred_pred = tagged_components[i][index].ctr >= 0;
            }
        }
    }

    // if no provider, use base predictor
    if (provider_component == NOT_DEFINED) {
        pred_taken = base_pred;
        return pred_taken;
    }

    // check if provider entry is a newly allocated
    bool newly_allocated_provider = (tagged_components[provider_component][provider_entry_index].u == 0) &&
                                    (abs(tagged_components[provider_component][provider_entry_index].ctr) <= 1);

    // if provider is newly allocated and altpred is valid, use altpred
    bool use_altpred = false;
    if (newly_allocated_provider && altpred_component != NOT_DEFINED) {
        use_altpred = true;
    }

    // set the prediction
    if (use_altpred) {
        pred_taken = altpred_pred;
    } 
    else {
        pred_taken = provider_pred;
    }

    return pred_taken;
}

void TAGEPredictor::update(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    // Update the prediction counter of the provider or base predictor
    // remember do not update altpred's prediction counter
    // in TAGE, altpred is only used for decision logic
    if (provider_component != NOT_DEFINED) {
        // update the provider's prediction counter
        if (resolveDir == TAKEN) {
            if (tagged_components[provider_component][provider_entry_index].ctr < MAX_CTR) {
                tagged_components[provider_component][provider_entry_index].ctr++;
            }
        } 
        else {
            if (tagged_components[provider_component][provider_entry_index].ctr > MIN_CTR) {
                tagged_components[provider_component][provider_entry_index].ctr--;
            }
        }
    } 
    else {
        // update the base predictor
        int base_index = PC % base_predictor.size();
        if (resolveDir == TAKEN) {
            if (base_predictor[base_index] < 3) {
                base_predictor[base_index]++;
            }
        } 
        else {
            if (base_predictor[base_index] > 0) {
                base_predictor[base_index]--;
            }
        }
    }

    // update u counter only if both provider and altpred exist and don't agree with each other
    if (provider_component != NOT_DEFINED && altpred_component != NOT_DEFINED) {
        bool altpred_pred = tagged_components[altpred_component][altpred_entry_index].ctr >= 0;

        if (altpred_pred != pred_taken) {
            if (pred_taken == resolveDir) {
                if (tagged_components[provider_component][provider_entry_index].u < MAX_U_COUNTER) {
                    tagged_components[provider_component][provider_entry_index].u++;
                }
            } 
            else {
                if (tagged_components[provider_component][provider_entry_index].u > 0) {
                    tagged_components[provider_component][provider_entry_index].u--;
                }
            }
        }
    }

    // allocation on misprediction. but if collision happens, do not allocate, only age the entries
    // if (pred_taken != resolveDir) {
    //     if (provider_component < NUM_COMPONENTS - 1) {
    //         int allocated_component = NOT_DEFINED;
    //         int start_component = provider_component + 1;

    //         // try first to find a spot in a higher component
    //         // if u == 0, its valid because its either empty or not that useful anyway
    //         for (int i = start_component; i < NUM_COMPONENTS; i++) {
    //             int index = compute_index(PC, i);
    //             if (tagged_components[i][index].u == 0) {
    //                 allocated_component = i;
    //                 break;
    //             }
    //         }

    //         // if a spot is found, do allocation
    //         if (allocated_component != NOT_DEFINED) {
    //             int index = compute_index(PC, allocated_component);
    //             uint16_t tag = compute_tag(PC, allocated_component);

    //             tagged_components[allocated_component][index].tag = tag;
    //             tagged_components[allocated_component][index].ctr = resolveDir == TAKEN ? 0 : -1;  // weak taken or weak not taken
    //             tagged_components[allocated_component][index].u = 0;
    //         } 
    //         else {
    //             // if no spot is found, decrement u counters across the upper components,
    //             // forces aging of entries, ensure somewhere eventually will become eligible for allocation
    //             for (int i = start_component; i < NUM_COMPONENTS; i++) {
    //                 int index = compute_index(PC, i);
    //                 if (tagged_components[i][index].u > 0) {
    //                     tagged_components[i][index].u--;
    //                 }
    //             }
    //         }
    //     }
    // }

    // use probability model in allocation to avoid ping-phenomenon
    if (pred_taken != resolveDir) {
        if (provider_component < NUM_COMPONENTS - 1) {
            // collect eligible components
            std::vector<int> eligible_components;
            for (int i = provider_component + 1; i < NUM_COMPONENTS; i++) {
                int index = compute_index(PC, i);
                if (tagged_components[i][index].u == 0) {
                    eligible_components.push_back(i);
                }
            }

            if (!eligible_components.empty()) {
                // assign weights inversely proportional to the component index
                std::vector<double> weights;
                double total_weight = 0.0;
                for (int comp : eligible_components) {
                    double weight = 1.0 / (comp - provider_component);
                    weights.push_back(weight);
                    total_weight += weight;
                }

                // normalize weights to probabilities
                for (double& weight : weights) {
                    weight /= total_weight;
                }

                // generate a random number between 0 and 1
                double rand_val = static_cast<double>(rand()) / RAND_MAX;

                // Select a component based on probabilities
                double cumulative_prob = 0.0;
                int allocated_component = eligible_components.back(); // Default to the last component
                for (size_t i = 0; i < weights.size(); i++) {
                    cumulative_prob += weights[i];
                    if (rand_val <= cumulative_prob) {
                        allocated_component = eligible_components[i];
                        break;
                    }
                }

                int index = compute_index(PC, allocated_component);
                uint16_t tag = compute_tag(PC, allocated_component);

                tagged_components[allocated_component][index].tag = tag;
                tagged_components[allocated_component][index].ctr = resolveDir == TAKEN ? 0 : -1;
                tagged_components[allocated_component][index].u = 0;
            } else {
                // age u counters
                for (int i = provider_component + 1; i < NUM_COMPONENTS; i++) {
                    int index = compute_index(PC, i);
                    if (tagged_components[i][index].u > 0) {
                        tagged_components[i][index].u--;
                    }
                }
            }
        }
    }

    // periodically reset the useful counters
    if (num_branches % RESET_PERIOD == 0) {
        periodic_reset();
    }

    // update the global history register
    global_history = (global_history << 1) | bitset<GLOBAL_HISTORY_LENGTH>(resolveDir == TAKEN ? 1 : 0);
}

int TAGEPredictor::compute_index(UINT32 PC, int component) {
    int history_length = history_lengths[component];
    UINT32 history_hash = get_history_hash(history_length);
    int index = (PC ^ history_hash) % tagged_components[component].size();
    return index;
}

uint16_t TAGEPredictor::compute_tag(UINT32 PC, int component) {
    int history_length = history_lengths[component];
    int tag_length = tag_bits[component];

    UINT32 history_hash = get_history_hash(history_length);
    uint16_t tag = ((PC >> 1) ^ history_hash) & ((1 << tag_length) - 1);
    return tag;
}

UINT32 TAGEPredictor::get_history_hash(int length) {
    UINT32 hash = 0;
    for (int i = 0; i < length; i++) {
        if (global_history[i]) {
            hash ^= (1 << (i % 16));
        }
    }
    return hash;
}

void TAGEPredictor::reset_prediction_state() {
    provider_component = NOT_DEFINED;
    provider_entry_index = NOT_DEFINED;
    altpred_component = NOT_DEFINED;
    altpred_entry_index = NOT_DEFINED;
}

void TAGEPredictor::periodic_reset() {
    static bool reset_msb = true;
    for (int i = 0; i < NUM_COMPONENTS; i++) {
        for (auto& entry : tagged_components[i]) {
            if (reset_msb) {
                entry.u &= 1;  // reset most significant bit
            } 
            else {
                entry.u &= 2;  // reset least significant bit
            }
        }
    }
    reset_msb = !reset_msb;
}

TAGEPredictor* predictor;

void InitPredictor_openend() {
    predictor = new TAGEPredictor();
}

bool GetPrediction_openend(UINT32 PC) {
    return predictor->predict(PC);
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    predictor->update(PC, resolveDir, predDir, branchTarget);
}