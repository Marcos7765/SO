#include "simulator.h"
#include<algorithm>

template <uint32_t rr_time> void RR(Simulator& sim){
    Process* proc = sim.next_process();
    while (proc!=NULL){
        sim.process(proc, rr_time);
        proc = sim.next_process();
    }
}

//um jeito legal de mostrar que o FCFS e um RR de quantum inf. são equivalentes
void (*FCFS)(Simulator&) = RR<0>;

bool comp_process(Process* a, Process* b){
    return (a->get_cpu_time() < b->get_cpu_time());
}

void SJF(Simulator& sim){
    Process* proc = sim.next_process();
    std::vector<Process*> procs;
    do {
        procs.push_back(proc);
        proc = sim.next_process();
    } while (proc != procs[0]);
    std::sort(procs.begin(), procs.end(), comp_process);
    for (auto p : procs){
        sim.process(p, 0);
    }
}

typedef struct cand {
    void (*func)(Simulator&);
    std::string name;
} cand;

#define CANDSIZE 4
cand candidates[] = {
    {FCFS,"FCFS"},
    {SJF,"SJF"},
    {RR<10>,"RR<10>"},
    {RR<100>,"RR<100>"},
};

int main(int argc, char* argv[]){
    if (argc != 2){
        std::printf("Use \"%s <CAMINHO PARA A IMAGEM>\"\n", argv[0]);
        return -1;
    }
    for (int i = 0; i < CANDSIZE; i++){
        Simulator sim = Simulator(argv[1]);
        std::printf("Resultado %s:\n", candidates[i].name.c_str());
        sim.set_strategy(candidates[i].func);
        sim.run();
        std::printf("----------------------------------------\n");
    }
}