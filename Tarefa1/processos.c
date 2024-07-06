#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

//[pós-greve]Código e comentários precedem o período de greve exceto quando sinalizado.
//Código disponível eventualmente em https://github.com/Marcos7765/SO
//[pós-greve]procuro usar master/main/principal e worker/trabalhador como nomes.
//"master e workers" veio de quando mexi na biblioteca distributed do Julia e achei melhor
//de falar que o "master e slaves" do An Introduction to Parallel Programming.
/*
ATENÇÃO: não segui o fluxo de execução elaborado na descrição do problema,
escolhi primeiro criar os dois processos e passar 'em paralelo' as tarefas
deles. Prefiro este uso por achar ser mais provável de terminar primeiro,
seja por terem mais processos para disputarem tempo de cpu (tenho que lembrar
de perguntar se ser um processo filho/ter processos filhos impacta no scheduling)
ou por permitir que mais de uma cpu/core rodem-os.
*/

/*
Análise geral: 
O fluxo do programa está descrito no comentário acima. Criei uma struct 'imagem' para armazenar
a imagem (e retornar informações dela em funções). Procurei fazer este programa separando no
máximo de funções possíveis para ver se minimizo o trabalho na hora de fazê-lo para threads.
Em retrospecto, eu podia ter pensado em usar uma pasta para guardar as imagens.

Use "<CAMINHO_PARA_O_PROGRAMA> <CAMINHO_PARA_IMAGEM>" para executar o programa na imagem alvo.

Certifique-se de apagar os arquivos temporários criados para montagem da imagem (especialmente
se rodou a outra versão do código recentemente). Não os deleto aqui para caso se queira ver o
resultado de cada detecção de borda em separado.

As principais dificuldades que encontrei foram na leitura e escrita da imagem, porque escolhi 
escrever minhas próprias funções para elas. À parte isso, a criação dos processos e delegação
das atividades são bem triviais, apesar de me considerar suspeito à falar disso por já ter
tido experiência nisso antes.
*/

//um identificador único para cada processo que depende somente da topologia dos processos
int rank = 0;

//struct pra imagem
typedef struct imagem {
    u_int8_t** dados;
    unsigned int width;
    unsigned int height; 
} imagem;

//lendo o pgm de acordo com essa especificação:
//https://people.sc.fsu.edu/~jburkardt/data/pgma/pgma.html
//usando u_int8_t pelo espaço (e por ser mais curto que unsigned char)
imagem lerImagem(const char* path){
    FILE* pfile = fopen(path, "r");
    if (pfile == NULL){
        printf("Erro para abrir o arquivo em: %s\n",path);
        exit(1);
    }
    
    char identificador[3];
    fscanf(pfile, "%c%c%c", identificador, identificador+1, identificador+2);
    if (identificador[2] != '\n'){
        printf("Invalid file.\n");
        fclose(pfile);
        exit(1);
    }
    
    //pula a linha do #
    fscanf(pfile, "%*[^\n]\n");

    imagem img;
    fscanf(pfile, "%i %i", &img.width, &img.height);
    
    //printf("%i %i\n", img.width, img.height);
    if (img.height <= 0 && img.width <= 0){
        printf("Invalid size: (%i, %i)\n", img.width, img.height);
        fclose(pfile);
        exit(1);
    }

    //pula a linha do maior valor (por alguma razão o fscanf da linha 36 nn funfa)
    int valor;
    fscanf(pfile,"%i\n", &valor);

    u_int8_t** dados = malloc(img.height * sizeof(u_int8_t*));
    
    for(int i = 0; i < img.height; i++){
        dados[i] = malloc(img.width * sizeof(u_int8_t));
        for(int j = 0; j < img.width; j++){
            fscanf(pfile,"%i\n", &valor);
            dados[i][j] = valor;
        }
    }
    img.dados = dados;
    fclose(pfile);
    return img;
}

//função pra criar uma imagem do msm tamanho que outra
//[pos-greve]: como estou criando no mesmo tamanho mesmo então vai ter aquela borda de 1px estranha
//eu poderia trocar agora mas aí não seria realmente um "msmTamanho"
imagem msmTamanho(imagem img){
    imagem novaImg;
    novaImg.height = img.height;
    novaImg.width = img.width;
    
    u_int8_t** dados = malloc(img.height * sizeof(u_int8_t*));
    
    //usando calloc pra gerar a matriz zerada
    for(int i = 0; i < img.height; i++){
        dados[i] = calloc(img.width, sizeof(u_int8_t));
    }
    novaImg.dados = dados;
    return novaImg;
}

void escreverImagem(imagem img, const char* path){
    FILE* pfile = fopen(path, "w");
    if (pfile == NULL){
        printf("Erro para criar o arquivo em: %s\n",path);
        exit(1);
    }

    fprintf(pfile, "P2\n");
    //não pesquisei mesmo a fundo sobre isso, aparentemente os comentários
    //podem aparecer no meio dos dados tbm, mas não estou lidando com eles
    fprintf(pfile, "# nn sei, um cabecalho\n");
    fprintf(pfile, "%i %i\n", img.width, img.height);
    //255 porque é o máximo q representamos
    fprintf(pfile, "255\n");
    //conta de padaria: o máximo é 70 caracteres, contando que cada caractere
    //adiciona no máx 4 caracteres temos 17 números por linha no máx
    int total = img.width * img.height;
    for (int i = 0; i < total; i++){
        fprintf(pfile, "%i", img.dados[i/img.width][i%img.width]);
        if (i%17 == 0){
            fprintf(pfile, "\n");
        } else {
            fprintf(pfile, " ");
        }
    }
    fclose(pfile);
}

void liberaImg(imagem img){
    for(int i = 0; i < img.height; i++){
        free(img.dados[i]);
    }
    free(img.dados);
}

//aqui é uma tentativa de função para deixar o jeito que interajo
//com os processos mais parecidos com as bibliotecas de MPI
int criaProc(){
    static int size = 1;
    rank = size;
    int temp_pid = fork();
    if (temp_pid<0){
        printf("reiosse\n");
        exit(1);
    }
    if(temp_pid == 0){
        return 0;
    }
    size++;
    rank = 0;
    
    return size;
}

//handler da entrada
const char* getPath(int argc, char* argv[]){
    
    //só poderá ser chamado dessa forma
    //não quero abordar outros erros tipo um path separado por espaço sem usar aspas
    if (argc != 2){
        printf("Use \"%s <CAMINHO_PARA_IMAGEM>\"\n", argv[0]);
        exit(1);
    }

    //fato curioso: se você não usar essa quebra de linha esse print vai pro
    //buffer e parece ser duplicado para cada outro processo que tu criar no fork
    //(ainda que só o master tenha rodado essa função)
    //printf("%s\n", params[1]);
    return argv[1];
}

//detecção de borda horizontal, dacordo com o algoritmo da contextualização
void detBordaHor(imagem base, imagem dest){
    //printf("dento da det borda\n");
    int val;
    for (int i = 1; i < base.height-1; i++){
        //printf("durante a det borda, i: %i e h: %i\n", i, base.height);
        for (int j = 1; j < base.width-1; j++){
            //printf("durante a det borda, i: %i, j: %i e w: %i\n", i, j, base.width);
            val = (base.dados[i+1][j-1] + base.dados[i+1][j] + base.dados[i+1][j+1])-
            (base.dados[i-1][j-1] + base.dados[i-1][j] + base.dados[i-1][j+1]);
            //[pós-greve]se não for nessa ordem os valores abaixo de -255 estouram
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            dest.dados[i][j] = val;
            //printf("dest.dados[%i][%i] = %i\n", i, j, dest.dados[i][j]);
        }
    }
    //printf("final da det borda\n");
}

void detBordaVer(imagem base, imagem dest){
    int val;
    for (int i = 1; i < base.height -1; i++){
        for (int j = 1; j < base.width -1; j++){
            val = (base.dados[i+1][j+1] + base.dados[i][j+1] + base.dados[i-1][j+1])-
            (base.dados[i+1][j-1] + base.dados[i][j-1] + base.dados[i-1][j-1]);
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            dest.dados[i][j] = val;
        }
    }
}

#define nWorkers 2

void main(int argc, char* argv[]){

    const char* path = getPath(argc, argv);

    imagem origImg = lerImagem(path);

    //Loop pra criar os processos
    //o rank == 0 está aí pra garantir que só o nó master vai criar workers
    for (int i = 0; rank == 0 && (i < nWorkers); i++){
        criaProc();
    }
    //instanciando aqui a novaImg porque tenho quase ctz que a memória do malloc
    //seria compartilhada quando acessasse o ponteiro
    imagem novaImg = msmTamanho(origImg);

    //handler para o comportamento de cada processo
    switch (rank){
        case 0:
            for (int i = 0; i < nWorkers; i++){
                //como vai ter que esperar os dois de qualquer forma, nn vou guardar o pid
                //eu poderia checar se houve erro, entretanto
                wait(NULL);
            }
            //liberando o ponteiro de origImg
            liberaImg(origImg);
            origImg = lerImagem("hor_temp.ascii.pgm");
            //liberando o ponteiro de novaImg, apesar de não ter usado ele no master
            liberaImg(novaImg);
            novaImg = lerImagem("ver_temp.ascii.pgm");
            //combinando as duas imagens
            int val;
            for (int i = 0; i<origImg.height; i++){
                for (int j = 0; j<origImg.width; j++){
                    val = novaImg.dados[i][j] + origImg.dados[i][j];
                    val = val > 255 ? 255 : val;
                    //novaImg.dados[i][j] = (novaImg.dados[i][j] + origImg.dados[i][j] > 255) ? 255 : novaImg.dados[i][j] + origImg.dados[i][j];
                    novaImg.dados[i][j] = val;
                }
            }
            //escrever o resultado
            escreverImagem(novaImg, "resultado.ascii.pgm");
            //limpeza final (a do novaImg acontece depois do switch para tds)
            liberaImg(origImg);
            break;
        case 1:
            detBordaHor(origImg, novaImg);
            escreverImagem(novaImg, "hor_temp.ascii.pgm");
            break;
        case 2:
            detBordaVer(origImg, novaImg);
            escreverImagem(novaImg, "ver_temp.ascii.pgm");
            break;
        default:
            printf("????\n");
            break;
    }
    liberaImg(novaImg);

  exit(0);
}