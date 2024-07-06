#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
//Esse código foi feito após a greve, apesar de reciclar bastante do código e dos comentários
//de processos.c
//Código disponível eventualmente em https://github.com/Marcos7765/SO
/*
ATENÇÃO: não segui o fluxo de execução elaborado na descrição do problema novamente.
Ao invés de definir variáveis globais para as matrizes, preferi por passá-las por argumento.
Também fiz com que não houvessem matrizes do resultado parcial da detecção de borda,
paralelizando a escrita do resultado entre as threads filhas. Por precaução sobre
a definição de thread mãe, decidi criar uma thread para então criar as outras duas.
No meu entendimento, a main já é executada por uma thread principal/thread mãe, mas
a especificação do passo 1 me faz pensar o contrário.
*/
//comentário da linha 241, feito antes deste aviso, fala um pouco sobre minha ideia
//de benefícios de passar por argumento
/*
Análise geral: 
O fluxo do programa está descrito no comentário acima. Criei uma struct 'imagem' para armazenar
a imagem (e retornar informações dela em funções). Procurei fazer este programa separando no
máximo de funções possíveis para ver se minimizo o trabalho na hora de fazê-lo para threads.
Em retrospecto, eu podia ter pensado em usar uma pasta para guardar as imagens.

Use "<CAMINHO_PARA_O_PROGRAMA> <CAMINHO_PARA_IMAGEM>" para executar o programa na imagem alvo.

As principais dificuldades que encontrei foram na passagem de dados à thread via argumentos,
porque inventei de usar outro caminho para ir à roma e passei um tempo não conseguindo passar
os dados das locks (o pthread_mutex_t[2]) via argumento. (vale dizer, essa dificuldade foi
tanta que desisti de passá-los por argumento e criei uma variável global.)

Espero que, apesar das diferenças entre o fluxo proposto e o fluxo implementado, seja possível
de ver que soube lidar com threads (criação, gerenciamento, espera, locks que não foi cobrado
ainda, etc.).

Boa parte dos comentários são reaproveitados dos de processos.c, até porque usei ele de base
para este arquivo.
*/

//struct pra imagem
typedef struct imagem {
    u_int8_t** dados;
    unsigned int width;
    unsigned int height; 
} imagem;

//struct pra passar arg pras threads
typedef struct targs {
    imagem* base;
    imagem* dest;
} targs;

//por alguma razão passar as locks por parâmetro não funcionou, então ficou global
pthread_mutex_t locks[2];
//Na linha 173 que volta a ter algo novo em relação a processos.c

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
//separei o loop em duas partes e estou usando uma lock para acesso
//de cada parte da imagem dest
void detBordaHor(imagem base, imagem dest){
    //começando alternado com detBordaVer para aproveitar a concorrência
    pthread_mutex_lock(locks+1);

    int val;
    for (int i = (base.height -1)/2; i < (base.height -1); i++){
        //printf("durante a det borda, i: %i e h: %i\n", i, base.height);
        for (int j = 1; j < base.width-1; j++){
            //printf("durante a det borda, i: %i, j: %i e w: %i\n", i, j, base.width);
            val = (base.dados[i+1][j-1] + base.dados[i+1][j] + base.dados[i+1][j+1])-
            (base.dados[i-1][j-1] + base.dados[i-1][j] + base.dados[i-1][j+1]);
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            //aqui poderia-se ter receio de overflow no uint8, mas a condição do ternário tá sendo
            //calculado implicitamente num tipo de inteiro com mais capacidade.
            //percebi a possibilidade no pós-greve, mas a imagem final indica que não teve esse
            //problema. por acaso isso conseguiu revelar um erro na ordem das checagens de valor, que comentei lá em processos.c
            dest.dados[i][j] = (dest.dados[i][j] + val > 255) ? 255 : dest.dados[i][j] + val;
            //printf("dest.dados[%i][%i] = %i\n", i, j, dest.dados[i][j]);
        }
    }

    pthread_mutex_unlock(locks+1);
    pthread_mutex_lock(locks);

    for (int i = 1; i < (base.height -1)/2; i++){
        for (int j = 1; j < base.width-1; j++){
            val = (base.dados[i+1][j-1] + base.dados[i+1][j] + base.dados[i+1][j+1])-
            (base.dados[i-1][j-1] + base.dados[i-1][j] + base.dados[i-1][j+1]);
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            dest.dados[i][j] = (dest.dados[i][j] + val > 255) ? 255 : dest.dados[i][j] + val;
        }
    }
    pthread_mutex_unlock(locks);
}

void detBordaVer(imagem base, imagem dest){
    
    pthread_mutex_lock(locks);

    for (int i = 1; i < (base.height -1)/2; i++){
        for (int j = 1; j < base.width -1; j++){
            int val = (base.dados[i+1][j+1] + base.dados[i][j+1] + base.dados[i-1][j+1])-
            (base.dados[i+1][j-1] + base.dados[i][j-1] + base.dados[i-1][j-1]);
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            dest.dados[i][j] = (dest.dados[i][j] + val > 255) ? 255 : dest.dados[i][j] + val;
        }
    }
    pthread_mutex_unlock(locks);

    pthread_mutex_lock(locks+1);
    for (int i = (base.height -1)/2; i < (base.height -1); i++){
        for (int j = 1; j < base.width -1; j++){
            int val = (base.dados[i+1][j+1] + base.dados[i][j+1] + base.dados[i-1][j+1])-
            (base.dados[i+1][j-1] + base.dados[i][j-1] + base.dados[i-1][j-1]);
            val = val > 0 ? val : -val;
            val = val > 255 ? 255 : val;
            dest.dados[i][j] = (dest.dados[i][j] + val > 255) ? 255 : dest.dados[i][j] + val;
        }
    }
    pthread_mutex_unlock(locks+1);
}

//funções pra contornar a preguiça de mexer nos argumentos das
//funções de leitura do pgm sem ter que criar uma global.
//(não que eu consiga ver um uso para isso neste código,
//mas deve ser útil pra programas dinâmicos)
void* dbhWrapped(void* targ){
    targs* tmp = (targs*) targ;
    detBordaHor(*tmp->base, *tmp->dest);
    pthread_exit(NULL);
}

void* dbvWrapped(void* targ){
    targs* tmp = (targs*) targ;
    detBordaVer(*tmp->base, *tmp->dest);
    pthread_exit(NULL);
}

void* thread_mae(void* path_targ){
    const char* path = path_targ;
    imagem origImg = lerImagem(path);
    imagem novaImg = msmTamanho(origImg);

    //iniciando as locks com os args padrão
    for (int i=0; i<2;i++){pthread_mutex_init(locks+i, NULL);}
    //formatando os dados passados às threads
    targs targ = {&origImg, &novaImg};
    pthread_t tids[2];

    pthread_create(tids, NULL, dbhWrapped, (void*) &targ);
    pthread_create(tids+1, NULL, dbvWrapped, (void*) &targ);

    for (int i=0; i<2;i++){pthread_join(tids[i], NULL);}
    escreverImagem(novaImg, "resultado.ascii.pgm");
    liberaImg(origImg);
    liberaImg(novaImg);
    for (int i=0; i<2;i++){pthread_mutex_destroy(locks+i);}
    pthread_exit(NULL);
}

void main(int argc, char* argv[]){

    const char* path = getPath(argc, argv);
    pthread_t tid;
    pthread_create(&tid, NULL, &thread_mae, (void*) path);
    pthread_join(tid, NULL);
}