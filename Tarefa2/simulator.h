#include<vector>
#include<string>

/* IDEIA GERAL:
A classe Simulator é responsável pela leitura do arquivo, 'processamento' dos
'processos' e registro do tempo.

A estratégia de escalonamento é desenvolvido como uma função que recebe uma
referência ao simulador e usa os métodos dele para obter info das listas e 
processar os dados, garantindo que todas as estratégias estejam contando o
tempo da mesma forma.

Pro caso do SJF isso exigirá que você monte uma lista catando cada referência
de processos do next_process() e ordene ela. Isso soa ineficiente porque é,
quando comparado a ordenar diretamente. Está mais complicado do que precisa
(isso tudo poderia ser resolvido num script python) mas espero que não ache tão
feio quanto estou achando (não sei ainda com quem vou fazer esse trabalho).

Por acaso isso libera qualquer outra pessoa, fora de contexto, a tentar
programar uma função de escalonamento.

Como o SJF parece o mais feio vou ver de deixar ele pronto.

OBS.: Esse uso de strategy pode parecer um pouco diferente de como é
convencionado (ex.: https://refactoring.guru/design-patterns/strategy) em
questão de implementação, isso foi a preguiça seletiva.
*/

class Process{
    private:
        uint32_t cpu_time; //tempo restante de exec
        uint32_t wait_time; //tempo que o proc esperou
        bool finished; //se o processo já terminou (cpu_time==0) ou não
    public:
        const uint8_t pid; //id do processo
        const uint32_t cpu_time_total; //tempo total de exec (explico na 
            //implementação de Simulator::process)
        uint32_t get_cpu_time(){return cpu_time;}; //getter só caso cpu_time
            //seja diferente de cpu_time_total quando alguém precisar
        //um construtor atoa só pra vir limpo
        Process(uint8_t pid, uint32_t cpu_time) : cpu_time{cpu_time}, 
        wait_time{0}, finished{false}, pid{pid}, cpu_time_total{cpu_time} {}
    friend class Simulator; //a amizade como acesso às partes privadas do outro
        //em C++ me é algo estranho mas esse comentário só faz mais sentido em
        //espanhol, dito isso
        //la amistad cómo lo acceso a las partes privadas de otro en C++ me és
        //un tanto estraño
};

class Simulator {
    private:
        std::vector<Process> active_processes; //a lista de processos ativos
        //std::vector<Process> finished_processes; //a lista de processos feitos
        void (*strategy)(Simulator&) = NULL; //a estratégia de escalonamento
        uint32_t time=0; //tempo decorrido
        uint16_t switch_penalty=0; //penalidade por trocar de processos (não)
        //variáveis usadas na f process que não podem ser static (porque static
        //de método é compartilhado entre objs diferentes)
        uint16_t index = 0;
        bool first = true;
            //pretendo usar ela mas fica pra posterioridade
    public: 
        Simulator(std::string filepath); //leitura do arq. pra montar a lista
        void set_strategy(void (*)(Simulator&)); //setter da estratégia
        void process(Process* proc, uint32_t max_time=0); //processa proc por até
            //max_time. caso max_time==0, processa até o fim de proc.
        Process* next_process(); //pega a ref do próx processo não terminado
            //como se acessasse uma lista circular
        bool run(); //roda strategy, confere se está tudo certo e printa o res
};