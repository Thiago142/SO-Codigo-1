// gcc main.c fifo.c -pthread -o prog (to compile)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include "fifo.h"

#define MAXSIZE 10
#define MEMORY_SHM 4096
#define KEY 2021
#define N_PROCESSOS 7
#define T1_P4 1
#define T2_P4 2
#define P5_TURN 0
#define P6_TURN 1
#define P7_TURN 2
#define P7_THREADS 3
#define END 10000

//Estrutura dos dados compartilhados
struct shared_area{
  FIFO f1, f2; // Filas a serem usadas
  int *pipe1, *pipe2;
  int writeAcess, readAcess;
  sem_t mutex_producers, mutex_threadsp4;
  int pid4, turn;
  int p5_processed , p6_processed ;
};
struct shared_area *shm;//Ponteiro para shared memory

int last_Turn = P5_TURN;

//Estrutura dos dados para Relatório
struct results{
  int count, maior, menor, moda;
  int numbers[END];
  clock_t timer;
};

//Tratador(Handle) of SIGUSR1
void handler_signal(int sig);

//Process Functions
void *consumeF2(void *args);
void produce_to_F1();
void task_P4();
void *get_F1_toPipes(void *args);
void get_pipe1_toF2();
void get_pipe2_toF2();
void get_F2_toPrint(struct results results);
void relatorio(struct results results);


int main(int argc, char* argv[]) {
  int pid[7], status[7], id;
  struct results results;

//////////////// Criando shared memory do programa //////////////// 
  int shmid;
  key_t key = KEY;

  void *shared_memory = (void *)0;
  results.timer = clock();

  shmid = shmget(key, MEMORY_SHM, 0666 | IPC_CREAT);
  if (shmid == -1)
    exit(-1);

  shared_memory = shmat(shmid, (void *)0, 0); // vinculando a um ponteiro
  if (shared_memory == (void *)-1)
    exit(-1);

  shm = (struct shared_area *)shared_memory;


//////////////// Instanciando os Pipes //////////////////////

  int pipe1[2];
  if (pipe(pipe1) == -1) return -1;

  int pipe2[2];
  if (pipe(pipe2) == -1) return -1;


/////// Inicializando semaphore e Dados Shared Memory ///////

  if (sem_init((sem_t *)&shm->mutex_producers, 1, 0) != 0)
    exit(-1);
  //instancia pipes
  shm->pipe1 = pipe1;
  shm->pipe2 = pipe2;
  shm->writeAcess = 1;//há acesso para escrita
  shm->readAcess = 0;//não há acesso para leitura(fila vazia)
  shm->turn = -2;
  //contagens dados passados p5 e p6
  shm->p5_processed  = 0;
  shm->p6_processed  = 0;
  //verificador de qtd de dados processados
  results.count = 0;
  //cria filas fifo da Shm
  queue_create(&shm->f1);
  queue_create(&shm->f2);

/////////////////// Processos //////////////////////////////
  for (int i = 0; i < N_PROCESSOS; i++) {
    id = fork();
    if (id > 0) { //PAI
      pid[i] = id;
    }
    else {
      if (i == 3) { //Processo 4
        signal(SIGUSR1, handler_signal);//ao receber o SIGUSR1 usa o handler como tratamento
        task_P4();
        while (1){}
      } else if (i < 3 && i >= 0){//Processos produtores p1,p2 e p3
         produce_to_F1();
      }else {
         if (i == 4) { // Processo 5
           get_pipe1_toF2();
        }if (i == 5) { // Processo 6
           get_pipe2_toF2();
        } if (i == 6) { // Processo 7
           get_F2_toPrint(results);
        }
      }
      break;
    }
 }
 
  if (id > 0) { //PAI
    int status;

    //permite que os processos produtores em espera sejam desbloqueados
    sem_post(&shm->mutex_producers);
    sleep(1);

    //inicializa P5
    shm->turn = P5_TURN;
    //espera contexto de P6 chegue ao pai e finaliza até p6
    waitpid(pid[6], &status, WUNTRACED);
    for (int i = 0; i < 6; i++){
      kill(pid[i], SIGKILL);
    }
    close(shm->pipe1[1]);
    close(shm->pipe2[1]);

    return 0;
  }
}


// >>>>>>>>>>>>>>>>>>>  THREADS  AND TREATS  <<<<<<<<<<<<<<<<<<<<<<<<

void handler_signal(int sig) {//altera o acesso para dados disponíveis para retirada
  shm->readAcess = 1;
}

void produce_to_F1() {
  while (1) {
    sem_wait((sem_t *)&shm->mutex_producers);
    if (shm->writeAcess) {//verifica se não há nenhum processo em escrita
      if (!queue_isfull(&shm->f1)) {

        int n_rand = rand() % 1000 + 1;
        queue_push(&shm->f1, n_rand);
       
        if (queue_isfull(&shm->f1)) {
          shm->writeAcess = 0;
          kill(shm->pid4, SIGUSR1);//Se fila estiver cheia, envia sinal p/P4
        }
      }
    }
    sem_post((sem_t *)&shm->mutex_producers);
  }
}

void *get_F1_toPipes(void *args) {
  int *atual = (int *) args;

  while (1) {
    sem_wait((sem_t *)&shm->mutex_threadsp4);//semafaro vermelho para região critica
    if (shm->readAcess) {//verifica se os dados estão disponíveis para leitura(1)
      if (queue_isempty(&shm->f1)) {
        shm->readAcess = 0;
        shm->writeAcess = 1;
        sem_post((sem_t *)&shm->mutex_threadsp4);
      }else{
        int num_fi = queue_getfirst(&shm->f1);
        queue_pop(&shm->f1);

        if (*atual == T1_P4){//verifica qual processo é o atual e coloca no pipe respectivo
          write(shm->pipe1[1], &num_fi, sizeof(int));
        }else{
          write(shm->pipe2[1], &num_fi, sizeof(int));
        }
      }
    }
    sem_post((sem_t *)&shm->mutex_threadsp4); //semafaro verde
  }
}

void task_P4() {
  shm->pid4 = getpid();
  pthread_t p4_thread02;
  int p4T1 = T1_P4;
  int p4T2 = T2_P4;

  if (sem_init((sem_t *)&shm->mutex_threadsp4, 0, 1) != 0)
    exit(-1);

  pthread_create(&p4_thread02, NULL, get_F1_toPipes, &p4T1); 
  get_F1_toPipes(&p4T2);
}

void get_pipe1_toF2() {
  while (1) {
    if(shm->turn == P5_TURN){
      if (queue_isfull(&shm->f2)){
        shm->turn = P7_TURN;
      }else{
        int value = 0;

        if(read(shm->pipe1[0], &value, sizeof(int)) > -1){
            queue_push(&shm->f2, value);
            shm->p5_processed ++;
            shm->turn = P7_TURN;
        }else{
            shm->turn = P6_TURN;
        }
      }
    }
  }
}

void get_pipe2_toF2() {
    while (1) {
      if(shm->turn == P6_TURN){
        if(queue_isfull(&shm->f2)) {
          shm->turn = P7_TURN;
        }else{
          int value = 0;
          
          if(read(shm->pipe2[0], &value, sizeof(int)) > -1){
              queue_push(&shm->f2, value);
              shm->p6_processed ++;

              shm->turn = P7_TURN;
          }else{
              shm->turn = P5_TURN;
          }
        }
    }
  }
}

void *consumeF2(void *args) {
  struct results *results = (struct results *) args;

  while (1) {
   if (shm->turn == P7_TURN){
        if (results->count == END){
          break;
        }
      
        if(!queue_isempty(&shm->f2)) {
          int value = queue_getfirst(&shm->f2);
          
          queue_pop(&shm->f2);

            if(value >= 1){
              if (results->count == 0) {
                  results->menor = value;
                  results->maior = value;
              }else{
                  if (results->menor > value){
                      results->menor = value;
                  } if (value > results->maior){
                      results->maior = value;
                  }
              }

            results->numbers[results->count] = value;
            results->count++;
            printf("P7: %d\n", value);
          }
        }
        if (last_Turn == P5_TURN) shm->turn = P6_TURN;
        else shm->turn = P5_TURN;
        last_Turn = shm->turn;
    }
  }
}

void get_F2_toPrint(struct results results) {
  pthread_t t[P7_THREADS];

  for (int i = 0; i < P7_THREADS -1; i++) {
    if (pthread_create(&t[i], NULL,
          consumeF2, &results))
      exit(-1);
  }
  consumeF2(&results);

  //Após consumo de F2 pelas Threads gera o relatório
  relatorio(results);

  exit(0);//Finaliza P7
}

void relatorio(struct results results){
  
  int c = 1, tempCount;
  int temp = 0,i = 0,j = 0;
  int popular = results.numbers[0];
  
  for (i = 0; i < (results.count - 1); i++)
    {
        temp = results.numbers[i];
        tempCount = 0;
        for (j = 1; j < results.count; j++)
        {
            if (temp == results.numbers[j])
                tempCount++;
        }
        if (tempCount > c)
        {
            popular = temp;
            c = tempCount;
        }
    }

  results.timer = clock() - results.timer; 

  printf("\nReseultados: \n\n");
  printf("Tempo de execução: %.3lf segundos \n", ((double)results.timer) / ((CLOCKS_PER_SEC)));
  printf("Quandidade processada em P5: %d\n", shm->p5_processed);
  printf("Quandidade processada em P6: %d\n", shm->p6_processed );
  printf("Moda:   %d\n",popular);
  printf("Mínimo: %d\n", results.menor);
  printf("Máximo: %d\n", results.maior);
  printf("------------------------------------------------------\n\n");
}
