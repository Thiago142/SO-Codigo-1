//Estrutura das Filas
#define MAXSIZE 10

struct queue{
  int front,pos;
  int size;
  int rand_numbers[MAXSIZE];
};
typedef struct queue FIFO;

//Funções Fila
void queue_create(FIFO *fi);
int queue_push(FIFO *fi, int value);
int queue_pop(FIFO *fi);
int queue_getfirst(FIFO *fi);
int queue_isfull(FIFO *fi);
int queue_isempty(FIFO *fi);
int queue_size(FIFO *fi);