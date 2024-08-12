#include "simulator.h"
#include<cstdio>
//#define DEBUG

Simulator::Simulator(std::string filepath){
    std::FILE * file;
    file = std::fopen(filepath.c_str(), "r");
    if (file==NULL) {
        std::printf("ERRO AO ABRIR ARQUIVO EM %s\n", filepath.c_str());
        exit(-1);
    }
    
    //essa parte deve ter um do while que sirva mas o que fiz não foi
    uint8_t pid=0;
    uint32_t cpu_time=0;
    int err = fscanf(file, "%hhu %u", &pid, &cpu_time);
    while (err != EOF){
        Process proc = {pid, cpu_time};
        active_processes.push_back(proc);
        err = fscanf(file, "%hhu %u", &pid, &cpu_time);
    }

    #ifdef DEBUG
        for (auto proc : active_processes) {
            std::printf("Add %hhu e %u\n", proc.pid, proc.cpu_time);
        }
    #endif
    std::fclose(file);
}

void Simulator::set_strategy(void (*strat)(Simulator&)){
    this->strategy = strat;
}

//se aqui eu pensasse em usar -1 como o "até terminar" ao invés de 0 daria
//pra economizar um if e deixar só o min ()
void Simulator::process(Process* proc, u_int32_t max_time){
    
    uint32_t run_time = (max_time >proc->cpu_time) ? proc->cpu_time : max_time;
    if (max_time == 0){run_time = proc->cpu_time;}
    /*
    esse cálculo de tempo está diferente do sugerido e é uma tentativa de não
    atualizar todos o wait_time de todos os demais processos em espera quando
    se processa.
    o tempo total decorrido pra finalização de um processo (T_total) seria
    a soma do tempo em que foi executado na CPU (T_executado) com o seu
    tempo de espera (T_espera). Com o atributo time, guardado pelo Simulator,
    conseguimos ter T_total, enquanto que T_executado seria o cpu_time_total
    guardado. só vamos precisar do T_espera pro resultado (pro requisito da
    tarefa) então não tem problema deixar de atualizar os demais processos
    é um tradeoff de memória x velocidade
    */
    
    time += run_time;
    #ifdef DEBUG
        if ((int)(proc->cpu_time - run_time) < 0){
            std::printf("ERROU NO SIMULATOR::PROCESS\n");
            exit(-1);
            }
    #endif
    proc->cpu_time -= run_time;
    if (proc->cpu_time == 0){
        proc->finished = true;
        proc->wait_time = time - proc->cpu_time_total;
    }
}

Process* Simulator::next_process(){
    //pode haver jeito mais bonito usando iterator mas esse aqui eu consigo
    //entender a ponto de explicar independente de quanto tempo eu fique longe
    //static uint16_t index = 0;
    //static bool first = true;
    time += switch_penalty; //só pro caso docê querer simular custo de troca
    if (first){
        first = false;
        return &active_processes.at(index);
    }
    uint16_t startingPoint = index;
    uint16_t vsize = active_processes.size();
    for (index=(index+1)%vsize; index!=startingPoint; index=(index+1)%vsize){
        Process* proc = &active_processes.at(index);
        if (!proc->finished) {return proc;}
    }
    //deu a volta inteira e não achou processo inacabado
    if (active_processes.at(index).finished){
        return NULL;
    } else {return &active_processes.at(index);}
}

bool Simulator::run(){
    //verificação de erros
    if (strategy==NULL){
        std::printf("ERRO: STRATEGY NÃO DEFINIDA (NULL)\n");
        return false;
    }
    if (active_processes.empty()){
        std::printf("ERRO: LISTA DE PROCESSOS VAZIA\n");
        return false;
    }
    //hora do show
    strategy(*this);

    //verificação dos resultados
    double wait_time_avg = 0.;
    for (auto proc : active_processes){
        if (!proc.finished){
            std::printf("ERRO: RESULTADO INVÁLIDO, PROCESSO %hhu INACABADO\n",
                proc.pid);
            return false;
        }
        wait_time_avg += proc.wait_time;
        #ifdef DEBUG
            std::printf("pid: %hhu\twait_time: %u\n", proc.pid, proc.wait_time);
        #endif
    }
    wait_time_avg /= active_processes.size();
    std::printf("Sucesso! Tempo médio: %f\n", wait_time_avg);
    return true;
}