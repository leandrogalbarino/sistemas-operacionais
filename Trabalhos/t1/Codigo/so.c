// so.c
// sistema operacional
// simulador de computador
// so24b

// INCLUDES {{{1
#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// CONSTANTES E TIPOS {{{1
// intervalo entre interrupções do relógio
#define INTERVALO_INTERRUPCAO 30 // em instruções executadas
#define REG 2
#define A 0
#define X 1
#define INIT_TAM 4
#define QUANTUM_PADRAO 10
#define IRQ_N 6

#define ESCALONADOR 2
#define ESCALONADOR_SIMPLES 0
#define ESCALONADOR_ROUND_ROBIN 1
#define ESCALONADOR_CIRCULAR2 2

typedef enum
{
  ESTADO_EXECUTANDO,
  ESTADO_PRONTO,
  ESTADO_BLOQUEADO,
  ESTADO_MORTO,
  ESTADO_N
} estado_processo_t;

typedef enum
{
  BLOCK_LEITURA,
  BLOCK_ESCRITA,
  BLOCK_ESPERA_PROC,
  BLOCK_NENHUM
} block_processo_t;

typedef struct metricas_proc
{
  int n_preempcao;
  int n_estado[ESTADO_N];
  int tempo_estado[ESTADO_N];
  int tempo_resposta;
  int tempo_retorno;

} metricas_proc_t;

typedef struct processo
{
  int pid;
  int reg[REG];
  int PC;
  int estado;
  int pendencia;
  int tipo_bloqueio;
  float prioridade;
  metricas_proc_t metricas;
} processo_t;

typedef struct fila
{
  processo_t *processo;
  struct fila *prox;
} fila_proc_t;

typedef struct
{
  fila_proc_t *inicio; // Ponteiro para o início da fila
  fila_proc_t *fim;    // Ponteiro para o fim da fila
} fila_t;

typedef struct metricas_so
{
  int tempo_total_execucao;
  int tempo_total_ocioso;
  int n_interrupcoes[IRQ_N];
  int n_preempcoes;

} metricas_so_t;

struct so_t
{
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;
  processo_t **tabela_processos;
  processo_t *processo_corrente;
  fila_t *processos_prontos;
  int pid_contador;
  int n_processos;
  int processos_tam;
  int quantum;
  int r_agora;
  metricas_so_t metricas;
  // t1: tabela de processos, processo corrente, pendências, etc
};

// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_A);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

char *nome_estado(int estado)
{
  switch (estado)
  {
  case ESTADO_PRONTO:
    return "PRONTO";
  case ESTADO_EXECUTANDO:
    return "EXECUTANDO";
  case ESTADO_BLOQUEADO:
    return "BLOQUEADO";
  case ESTADO_MORTO:
    return "MORTO";
  default:
    return "NÃO TRATADO";
  }
}
static void so_criar_arquivo(so_t *self){
  FILE *arquivo = fopen("metricas_circular2.txt", "w");
  if (arquivo == NULL)
  {
    console_printf("Erro ao abrir o arquivo para escrita.\n");
    return;
  }

  fprintf(arquivo, "------------------------------\n");
  fprintf(arquivo, "\tMetricas do Sistema Operacional\n");
  fprintf(arquivo, "Numero de processos criados: %d\n", self->n_processos);
  fprintf(arquivo, "Tempo total de execucao: %ds\n", self->metricas.tempo_total_execucao);
  fprintf(arquivo, "Tempo total do sistema ocioso: %ds\n", self->metricas.tempo_total_ocioso);

  for (int i = 0; i < IRQ_N; i++)
    fprintf(arquivo, "Numero de interrupcoes %s :%d\n", irq_nome(i), self->metricas.n_interrupcoes[i]);

  fprintf(arquivo, "Numero de preempcoes: %d\n", self->metricas.n_preempcoes);

  fprintf(arquivo, "------------------------------\n");
  fprintf(arquivo, "\tMetricas dos Processos\n");
  for (int i = 0; i < self->n_processos; i++)
  {
    fprintf(arquivo, "Processo: %d\n", self->tabela_processos[i]->pid);

    fprintf(arquivo, "Tempo retorno: %ds\n", self->tabela_processos[i]->metricas.tempo_retorno);

    fprintf(arquivo, "Numero de preempcoes do processo: %d\n", self->tabela_processos[i]->metricas.n_preempcao);

    for (int j = 0; j < ESTADO_N - 1; j++)
    {

      fprintf(arquivo, "Estado %s numero de vezes: %d\n", nome_estado(j), self->tabela_processos[i]->metricas.n_estado[j]);

      fprintf(arquivo, "Estado %s tempo total: %ds\n", nome_estado(j), self->tabela_processos[i]->metricas.tempo_estado[j]);
    }
    fprintf(arquivo, "Tempo resposta: %ds\n", self->tabela_processos[i]->metricas.tempo_resposta);
    fprintf(arquivo, "------------------------------\n");
  }

  fclose(arquivo);
}

static void so_imprime_metricas(so_t *self)
{
  so_criar_arquivo(self);
  
  console_printf("------------------------------");
  console_printf("\tMetricas do Sistema Operacional\n");
  console_printf("Numero de processos criados: %d\n", self->n_processos);
  console_printf("Tempo total de execucao: %ds\n", self->metricas.tempo_total_execucao);
  console_printf("Tempo total do sistema ocioso: %ds\n", self->metricas.tempo_total_ocioso);
  for (int i = 0; i < IRQ_N; i++)
    console_printf("Numero de interrupcoes %s :%d\n", irq_nome(i), self->metricas.n_interrupcoes[i]);
  console_printf("Numero de preempcoes: %d\n", self->metricas.n_preempcoes);

  console_printf("------------------------------\n");
  console_printf("\tMetricas dos Processos\n");
  for (int i = 0; i < self->n_processos; i++)
  {
    console_printf("Processo: %d\n", self->tabela_processos[i]->pid);

    console_printf("Tempo retorno: %ds\n", self->tabela_processos[i]->metricas.tempo_retorno);

    console_printf("Numero de preempcoes do processo: %d\n", self->tabela_processos[i]->metricas.n_preempcao);

    for (int j = 0; j < ESTADO_N - 1; j++)
    {

      console_printf("Estado %s numero de vezes: %d\n", nome_estado(j), self->tabela_processos[i]->metricas.n_estado[j]);

      console_printf("Estado %s tempo total: %ds\n", nome_estado(j), self->tabela_processos[i]->metricas.tempo_estado[j]);
    }
    console_printf("Tempo resposta: %ds\n", self->tabela_processos[i]->metricas.tempo_resposta);
    console_printf("------------------------------\n");
  }
}

// FUNCOES DE FILA -> processos_prontos!
void fila_inicializar(so_t *self)
{
  self->processos_prontos = malloc(sizeof(fila_t));
  if (self->processos_prontos == NULL)
  {
    console_printf("SO: Erro ao alocar memória para a fila de processos prontos\n");
    self->erro_interno = true;
    return;
  }
  self->processos_prontos->inicio = NULL;
  self->processos_prontos->fim = NULL;
}

int fila_vazia(fila_t *fila)
{
  return (fila->inicio == NULL);
}

int fila_enfileirar(fila_t *fila, processo_t *processo)
{
  fila_proc_t *novo = malloc(sizeof(fila_proc_t));
  if (novo == NULL)
    return 0;
  novo->processo = processo;
  novo->prox = NULL;

  if (fila->fim == NULL)
  {
    fila->inicio = novo;
    fila->fim = novo;
  }
  else
  {
    fila->fim->prox = novo;
    fila->fim = novo;
  }
  return 1;
}

processo_t *fila_desenfileirar(fila_t *fila)
{
  if (fila_vazia(fila))
  {
    console_printf("SO: Fila vazia. Nao foi possivel desinfileira.\n");
    return NULL;
  }

  fila_proc_t *remover = fila->inicio;
  processo_t *processo = remover->processo;
  fila->inicio = remover->prox;

  if (fila->inicio == NULL)
    fila->fim = NULL;

  free(remover);
  return processo;
}

void fila_limpar(fila_t *fila)
{
  while (!fila_vazia(fila))
    fila_desenfileirar(fila);
}

// fila_proc_t **anterior
processo_t *procurar_melhor_proc(so_t *self)
{
  processo_t *melhor_proc = NULL;
  fila_proc_t *atual = self->processos_prontos->inicio;

  while (atual != NULL)
  {
    processo_t *proc = atual->processo;
    if (proc->estado == ESTADO_PRONTO &&
        (melhor_proc == NULL || proc->prioridade < melhor_proc->prioridade))
    {
      melhor_proc = proc;
    }

    atual = atual->prox;
  }

  return melhor_proc;
}

int remover_processo_fila(fila_t *fila, processo_t *proc)
{
  if (fila == NULL || proc == NULL)
  {
    console_printf("SO: Fila Vazia!\n");
    return 0;
  }

  fila_proc_t *atual = fila->inicio;
  fila_proc_t *anterior = NULL;

  while (atual != NULL)
  {
    if (atual->processo == proc)
    {
      if (anterior == NULL)
      {
        fila->inicio = atual->prox;
        if (fila->inicio == NULL)
          fila->fim = NULL;
      }
      else
      {
        anterior->prox = atual->prox;
        if (atual->prox == NULL)
          fila->fim = anterior;
      }
      free(atual);
      console_printf("SO: Processo removido com sucesso: PID = %d\n", proc->pid);
      return 1;
    }
    anterior = atual;
    atual = atual->prox;
  }

  console_printf("SO: Processo não encontrado na fila.\n");
  return 0;
}

// CRIAÇÃO {{{1

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL)
    return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->erro_interno = false;
  self->tabela_processos = NULL;
  self->processo_corrente = NULL;
  self->pid_contador = 0;
  self->n_processos = 0;
  self->quantum = QUANTUM_PADRAO;
  for (int i = 0; i < IRQ_N; i++)
    self->metricas.n_interrupcoes[i] = 0;
  self->metricas.n_preempcoes = 0;
  self->metricas.tempo_total_execucao = 0;
  self->metricas.tempo_total_ocioso = 0;
  self->r_agora = -1;

  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);

  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR)
  {
    console_printf("SO: problema na carga do programa de tratamento de interrupção\n");
    self->erro_interno = true;
  }

  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK)
  {
    console_printf("SO: problema na programação do timer\n");
    self->erro_interno = true;
  }

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}

// TRATAMENTO DE INTERRUPÇÃO {{{1

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona_simples(so_t *self);
static void so_escalona_round_robin(so_t *self);
static void so_escalona_circular2(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção
static bool so_processos_mortos(so_t *self)
{
  for (int i = 0; i < self->n_processos; i++)
  {
    if (self->tabela_processos[i]->estado != ESTADO_MORTO)
      return 0;
  }
  return 1;
}

static int so_para(so_t *self)
{
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_TIMER, 0);
  e2 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0);
  if (e1 != ERR_OK || e2 != ERR_OK)
  {
    console_printf("SO: nao consigo desligar o timer\n");
    self->erro_interno = true;
  }

  so_imprime_metricas(self);

  return 1;
}

static void processo_atualiza_metricas(processo_t *proc, int diferenca_tempo)
{
  if (proc->estado != ESTADO_MORTO)
  {
    proc->metricas.tempo_retorno += diferenca_tempo;
  }
  proc->metricas.tempo_estado[proc->estado] += diferenca_tempo;
  proc->metricas.tempo_resposta = proc->metricas.tempo_estado[ESTADO_PRONTO] / proc->metricas.n_estado[ESTADO_PRONTO];

  console_printf("Processo PID: %d, Tempo: %d, Estado: %s, Prioridade: %f\n", proc->pid, proc->metricas.tempo_retorno, nome_estado(proc->estado), proc->prioridade);
}

static void so_atualiza_metricas(so_t *self, int diferenca_tempo)
{
  self->metricas.tempo_total_execucao += diferenca_tempo;
  if (self->processo_corrente == NULL || self->processo_corrente->estado == ESTADO_BLOQUEADO)
    self->metricas.tempo_total_ocioso += diferenca_tempo;
  for (int i = 0; i < self->n_processos; i++)
    processo_atualiza_metricas(self->tabela_processos[i], diferenca_tempo);
}

static void so_sincroniza(so_t *self)
{
  int r_anterior = self->r_agora;

  if (es_le(self->es, D_RELOGIO_INSTRUCOES, &self->r_agora) != ERR_OK)
  {
    console_printf("SO: erro na leitura do relógio\n");
    return;
  }

  if (r_anterior == -1)
    return;

  int diferenca_tempo = self->r_agora - r_anterior;
  so_atualiza_metricas(self, diferenca_tempo);
}

static int so_trata_interrupcao(void *argC, int reg_A)
{
  so_t *self = argC;
  irq_t irq = reg_A;
  self->metricas.n_interrupcoes[irq]++;
  // esse print polui bastante, recomendo tirar quando estiver com mais confiança
  console_printf("SO: recebi IRQ %d (%s)", irq, irq_nome(irq));

  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);

  so_sincroniza(self);
  // faz o atendimento da interrupção
  so_trata_irq(self, irq);
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  // escolhe o próximo processo a executar
  so_escalona(self);
  // recupera o estado do processo escolhido
  if (!so_processos_mortos(self))
    return so_despacha(self);
  else
    return so_para(self);
}

static void so_salva_estado_da_cpu(so_t *self)
{
  if (self->processo_corrente != NULL)
  {
    mem_le(self->mem, IRQ_END_PC, &self->processo_corrente->PC);
    mem_le(self->mem, IRQ_END_A, &self->processo_corrente->reg[A]);
    mem_le(self->mem, IRQ_END_X, &self->processo_corrente->reg[X]);
  }
}
static int calcula_dispositivo(int disp, int terminal)
{
  return disp + terminal * 4;
}

static int obtem_terminal(int pid)
{
  return (pid) % 4;
}

static void mudar_estado(so_t *self, processo_t *proc, int estado)
{
  switch (estado)
  {
  case ESTADO_EXECUTANDO:
    proc->estado = ESTADO_EXECUTANDO;
    break;
  case ESTADO_PRONTO:
    proc->estado = ESTADO_PRONTO;
    break;
  case ESTADO_BLOQUEADO:
    proc->estado = ESTADO_BLOQUEADO;
    break;
  case ESTADO_MORTO:
    proc->estado = ESTADO_MORTO;
    break;
  default:
    self->erro_interno = true;
    console_printf("SO: Erro ao mudar de estado\n");
  }
  proc->metricas.n_estado[estado]++;
}

static void so_trata_pendencias(so_t *self)
{
  for (int i = 0; i < self->n_processos; i++)
  {
    processo_t *proc = self->tabela_processos[i];
    if (proc->estado == ESTADO_BLOQUEADO)
    {
      int terminal = obtem_terminal(proc->pid);
      int dispositivo_teclado = calcula_dispositivo(D_TERM_A_TECLADO_OK, terminal);
      int dispositivo_tela = calcula_dispositivo(D_TERM_A_TELA_OK, terminal);

      int estado_teclado, estado_tela;
      switch (proc->tipo_bloqueio)
      {
      case BLOCK_LEITURA:
        if (es_le(self->es, dispositivo_teclado, &estado_teclado) == ERR_OK && estado_teclado != 0)
        {
          mudar_estado(self, proc, ESTADO_PRONTO);
        }
        break;
      case BLOCK_ESCRITA:
        // se o dispositivo de tela está pronto, escreve
        if (es_le(self->es, dispositivo_tela, &estado_tela) == ERR_OK && estado_tela != 0)
        {
          if (es_escreve(self->es, calcula_dispositivo(D_TERM_A_TELA, terminal), proc->pendencia) == ERR_OK)
          {
            mudar_estado(self, proc, ESTADO_PRONTO);
          }
        }
        break;
      case BLOCK_ESPERA_PROC:
        // verifica se o processo esperado já morreu
        for (int j = 0; j < self->processos_tam; j++)
        {
          if (self->tabela_processos[j]->pid == proc->reg[A] && self->tabela_processos[j]->estado == ESTADO_MORTO)
          {
            mudar_estado(self, proc, ESTADO_PRONTO);
            break;
          }
        }
        break;
      default:
        return;
        break;
      }
      if (proc->estado == ESTADO_PRONTO)
      {
        if (!fila_enfileirar(self->processos_prontos, proc))
        {
          console_printf("SO: Erro ao enfilar processo pronto\n");
          self->erro_interno = true;
          return;
        }
        proc->tipo_bloqueio = BLOCK_NENHUM;
        console_printf("SO: processo %d desbloqueado e inserido na fila de prontos\n", proc->pid);
      }
    }
  }
}

static void so_escalona(so_t *self)
{
  switch (ESCALONADOR)
  {
  case ESCALONADOR_SIMPLES:
    so_escalona_simples(self);
    break;
  case ESCALONADOR_ROUND_ROBIN:
    so_escalona_round_robin(self);
    break;
  case ESCALONADOR_CIRCULAR2:
    so_escalona_circular2(self);
    break;
  default:
    self->erro_interno = true;
    console_printf("SO: Escalonador selecionado nao existe\n");
    break;
  }
}

// Funcao correta!
static void so_escalona_simples(so_t *self)
{
  if (self->processo_corrente != NULL && self->processo_corrente == ESTADO_EXECUTANDO)
    return;

  if (self->processo_corrente != NULL && self->processo_corrente->estado == ESTADO_PRONTO)
  {
    mudar_estado(self, self->processo_corrente, ESTADO_EXECUTANDO);
    console_printf("Processo %d continua executando\n", self->processo_corrente->pid);
    return;
  }
  if (self->processo_corrente != NULL && self->processo_corrente->estado != ESTADO_MORTO)
  {
  }
  processo_t *proc = NULL;
  if (self->tabela_processos != NULL)
  {
    for (int i = 0; i < self->n_processos; i++)
    {
      if (self->tabela_processos[i]->estado == ESTADO_PRONTO || self->tabela_processos[i]->estado == ESTADO_EXECUTANDO)
      {
        proc = self->tabela_processos[i];
        self->processo_corrente = proc;
        self->quantum = QUANTUM_PADRAO;
        mudar_estado(self, proc, ESTADO_EXECUTANDO);
        console_printf("Processo %d escalonado para executar\n", proc->pid);
        return;
      }
    }
    return;
  }
  console_printf("SO: Tabela de processos vazia ou não inicializada.\n");
  self->erro_interno = true;
}

// static void resetar_fila(so_t *self)
// {
//   if (fila_vazia(self->processos_prontos))
//     for (int i = 0; i < self->n_processos; i++)
//     {
//       processo_t *proc = self->tabela_processos[i];
//       if (proc->estado == ESTADO_PRONTO)
//         if (!fila_enfileirar(self->processos_prontos, proc))
//         {
//           self->erro_interno = true;
//           console_printf("SO: Erro ao enfileirar\n");
//         }
//     }
// }

static void so_escalona_round_robin(so_t *self)
{
  if (self->processo_corrente != NULL &&
      self->processo_corrente->estado == ESTADO_EXECUTANDO && self->quantum > 0)
    return;

  // if (self->processo_corrente != NULL && self->processo_corrente->estado == ESTADO_EXECUTANDO)
  // {
  //   mudar_estado(self, self->processo_corrente, ESTADO_PRONTO);
  //   if (!fila_enfileirar(self->processos_prontos, self->processo_corrente))
  //   {
  //     console_printf("SO: Erro ao enfilar processo pronto\n");
  //     self->erro_interno = true;
  //     return;
  //   }
  // }
  // PROCESSO->estado == ESTADO_MORTO
  processo_t *proc = fila_desenfileirar(self->processos_prontos);
  if (proc == NULL && self->processo_corrente != NULL && self->processo_corrente->estado == ESTADO_BLOQUEADO)
  {
    so_trata_pendencias(self);
    proc = fila_desenfileirar(self->processos_prontos);
  }
  if (proc != NULL && proc->estado == ESTADO_PRONTO)
  {
    // self->quantum = QUANTUM_PADRAO;
    console_printf("SO: Processo %d escalonado para execucao..\n", proc->pid);

    if (
        self->processo_corrente != NULL &&
        self->processo_corrente != proc &&
        self->processo_corrente->estado == ESTADO_EXECUTANDO)
    {
      mudar_estado(self, self->processo_corrente, ESTADO_PRONTO);
      if (!fila_enfileirar(self->processos_prontos, self->processo_corrente))
      {
        console_printf("SO: Erro ao enfilar processo pronto\n");
        self->erro_interno = true;
        return;
      }
      self->metricas.n_preempcoes++;
      self->processo_corrente->metricas.n_preempcao++;
      console_printf("SO: Processo %d preempedido\n", self->processo_corrente->pid);
    }

    console_printf("SO: Processo %d executando\n", proc->pid);
    proc->estado = ESTADO_EXECUTANDO;

    self->processo_corrente = proc;
    self->quantum = QUANTUM_PADRAO;
    proc->metricas.n_estado[ESTADO_EXECUTANDO]++;
  }

  else
  {
    console_printf("SO: Nenhum processo pronto encontrado. CPU ociosa.\n");
    return;
  }
}

static void calcular_prioridade(so_t *self, processo_t *proc)
{
  float t_quantum = (float)QUANTUM_PADRAO;
  float t_exec = (float)(QUANTUM_PADRAO - self->quantum);
  self->processo_corrente->prioridade = (proc->prioridade + (t_exec / t_quantum)) / 2;
}

static void so_escalona_circular2(so_t *self)
{
  if (self->processo_corrente != NULL &&
      self->processo_corrente->estado == ESTADO_EXECUTANDO && self->quantum > 0)
    return;

  // if (self->processo_corrente != NULL && self->processo_corrente->estado == ESTADO_EXECUTANDO)
  // {
  //   mudar_estado(self, self->processo_corrente, ESTADO_PRONTO);
  //   if (!fila_enfileirar(self->processos_prontos, self->processo_corrente))
  //   {
  //     console_printf("SO: Erro ao enfilar processo pronto\n");
  //     self->erro_interno = true;
  //     return;
  //   }
  // }
  // PROCESSO->estado == ESTADO_MORTO
  processo_t *proc = procurar_melhor_proc(self);
  if (!remover_processo_fila(self->processos_prontos, proc))
  {
    if (proc == NULL && self->processo_corrente != NULL && self->processo_corrente->estado == ESTADO_BLOQUEADO)
    {
      so_trata_pendencias(self);
      proc = procurar_melhor_proc(self);
      if (!remover_processo_fila(self->processos_prontos, proc))
      {
        return;
      }
    }
  }

  if (proc != NULL && proc->estado == ESTADO_PRONTO)
  {
    // self->quantum = QUANTUM_PADRAO;
    console_printf("SO: Processo %d escalonado para execucao..\n", proc->pid);

    if (
        self->processo_corrente != NULL &&
        self->processo_corrente != proc &&
        self->processo_corrente->estado == ESTADO_EXECUTANDO)
    {
      self->metricas.n_preempcoes++;
      self->processo_corrente->metricas.n_preempcao++;
      calcular_prioridade(self, self->processo_corrente);
      mudar_estado(self, self->processo_corrente, ESTADO_PRONTO);
      if (!fila_enfileirar(self->processos_prontos, self->processo_corrente))
        return;
    }

    console_printf("SO: processo %d executando\n", proc->pid);
    proc->estado = ESTADO_EXECUTANDO;

    self->processo_corrente = proc;
    self->quantum = QUANTUM_PADRAO;
    proc->metricas.n_estado[ESTADO_EXECUTANDO]++;
  }
  else
  {
    console_printf("SO: Nenhum processo pronto encontrado. CPU ociosa.\n");
    return;
  }
}

// static void so_escalona_circular21(so_t *self)
// {
//   if (self->processo_corrente != NULL &&
//       self->processo_corrente->estado == ESTADO_EXECUTANDO && self->quantum > 0)
//   {
//     return; // Continua com o processo corrente
//   }
//   printf("debug1");
//   processo_t *proc = procurar_melhor_proc(self);
//   if (!remover_processo_fila(self->processos_prontos, proc))
//   {
//     self->quantum = QUANTUM_PADRAO;
//     return;
//   }
//   printf("debug2");

//   // REVER FILA
//   if (proc != NULL && proc->estado == ESTADO_PRONTO)
//   {
//     self->quantum = QUANTUM_PADRAO;
//     console_printf("SO: Processo %d escalonado para execucao com quantum = %d\n", proc->pid, self->quantum);

//     if (
//         self->processo_corrente != NULL &&
//         self->processo_corrente != proc &&
//         self->processo_corrente->estado == ESTADO_EXECUTANDO)
//     {
//       self->metricas.n_preempcoes++;
//       self->processo_corrente->metricas.n_preempcao++;
//       calcular_prioridade(self, self->processo_corrente);
//       console_printf("PRIORIDADE: %d", self->processo_corrente->prioridade);

//       mudar_estado(self, self->processo_corrente, ESTADO_PRONTO);
//       if (!fila_enfileirar(self->processos_prontos, self->processo_corrente))
//         return;
//     }

//     console_printf("SO: processo %d executando\n", proc->pid);
//     proc->estado = ESTADO_EXECUTANDO;

//     self->processo_corrente = proc;
//     self->quantum = QUANTUM_PADRAO;
//     proc->metricas.n_estado[ESTADO_EXECUTANDO]++;
//   }
//   else
//     console_printf("SO: processo %d preempedido\n", self->processo_corrente->pid);

//   {
//     console_printf("SO: Nenhum processo pronto encontrado. CPU ociosa.\n");
//     self->processo_corrente = NULL; // CPU ficará ociosa
//   }
// }

// Função correta!
static void so_recupera_estado_da_cpu(so_t *self)
{
  if (self->processo_corrente != NULL)
  {
    mem_escreve(self->mem, IRQ_END_PC, self->processo_corrente->PC);
    mem_escreve(self->mem, IRQ_END_A, self->processo_corrente->reg[A]);
    mem_escreve(self->mem, IRQ_END_X, self->processo_corrente->reg[X]);
    console_printf("SO: Estado do processador recuperado\n");
    return;
  }
  console_printf("SO: Erro ao recuperar o estado do processador\n");
  self->erro_interno = true;
}
// Função correta!
static int so_despacha(so_t *self)
{
  so_recupera_estado_da_cpu(self);
  if (self->erro_interno)
    return 1;
  else
    return 0;
}

// TRATAMENTO DE UMA IRQ {{{1

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_irq_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  switch (irq)
  {
  case IRQ_RESET:
    so_trata_irq_reset(self);
    break;
  case IRQ_SISTEMA:
    so_trata_irq_chamada_sistema(self);
    break;
  case IRQ_ERR_CPU:
    so_trata_irq_err_cpu(self);
    break;
  case IRQ_RELOGIO:
    so_trata_irq_relogio(self);
    break;
  default:
    so_trata_irq_desconhecida(self, irq);
  }
}

static processo_t *alocar_proc(so_t *self)
{
  processo_t *novo_processo = malloc(sizeof(processo_t));
  if (novo_processo == NULL)
  {
    console_printf("SO: Erro ao alocar memoria para o processo\n");
    self->erro_interno = true;
    return NULL;
  }
  return novo_processo;
}

static void inicializar_processo(processo_t *proc, int pid, int PC)
{
  proc->pid = pid;
  proc->PC = PC;
  proc->estado = ESTADO_PRONTO;
  proc->reg[A] = 0;
  proc->reg[X] = 0;
  proc->pendencia = 0;
  for (int i = 0; i < ESTADO_N; i++)
  {
    proc->metricas.n_estado[i] = 0;
    proc->metricas.tempo_estado[i] = 0;
  }
  proc->metricas.n_estado[ESTADO_PRONTO]++;
  proc->metricas.tempo_resposta = 0;
  proc->metricas.tempo_retorno = 0;
  proc->prioridade = 0.5;
}

// OK
static bool inicializar_tabela_processos(so_t *self)
{
  self->processos_tam = INIT_TAM; // Tamanho inicial
  self->tabela_processos = malloc(self->processos_tam * sizeof(processo_t *));
  if (self->tabela_processos == NULL)
  {
    console_printf("SO: Erro ao alocar memória para tabela de processos\n");
    self->erro_interno = true;
    return false;
  }

  return true;
}

// OK
static bool adicionar_processo(so_t *self, processo_t *novo_processo)
{
  if (self->n_processos >= self->processos_tam)
  {
    console_printf("SO: Limite de programas atingido");
    self->erro_interno = true;
    return false;
  }
  self->tabela_processos[self->n_processos++] = novo_processo;
  return true;
}
// interrupção gerada uma única vez, quando a CPU inicializa
static void so_trata_irq_reset(so_t *self)
{
  processo_t *processo_init = alocar_proc(self);

  // coloca o programa init na memória
  int ender = so_carrega_programa(self, "init.maq");
  if (ender != 100)
  {
    console_printf("SO: Problema na carga do programa inicial\n");
    self->erro_interno = true;
    return;
  }
  inicializar_processo(processo_init, self->pid_contador++, ender);
  if (!inicializar_tabela_processos(self))
    return;
  fila_inicializar(self);
  if (!adicionar_processo(self, processo_init))
    return;
  self->processo_corrente = processo_init;
  fila_enfileirar(self->processos_prontos, self->processo_corrente);

  // altera o PC para o endereço de carga
  mem_escreve(self->mem, IRQ_END_PC, processo_init->PC);
}

static void so_verifica_espera(so_t *self, int pid_morto)
{
  for (int i = 0; i < self->n_processos; i++)
  {
    if (self->tabela_processos[i]->estado == ESTADO_BLOQUEADO && self->tabela_processos[i]->tipo_bloqueio == SO_ESPERA_PROC && self->tabela_processos[i]->pendencia == pid_morto)
    {
      mudar_estado(self, self->tabela_processos[i], ESTADO_PRONTO);
      fila_enfileirar(self->processos_prontos, self->tabela_processos[i]);
      console_printf("SO: Processo %d desbloqueado após a morte do processo %d\n", self->tabela_processos[i]->pid, pid_morto);
    }
  }
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  int err_int;
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf("SO: Erro na CPU: %s\n", err_nome(err));

  if (self->processo_corrente != NULL)
  {
    console_printf("SO: matando processo %d devido a erro na CPU\n", self->processo_corrente->pid);
    mudar_estado(self, self->processo_corrente, ESTADO_MORTO);
    so_verifica_espera(self, self->processo_corrente->pid);
    self->processo_corrente = NULL;
  }
  else
  {
    console_printf("SO: Erro na CPU sem processo corrente");
    self->erro_interno = true;
  }
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK)
  {
    console_printf("SO: Problema da reinicialização do timer");
    self->erro_interno = true;
  }
  if (self->quantum > 0)
  {
    self->quantum--;
  }
  console_printf("Quantum: %d\n", self->quantum);
  // t1: deveria tratar a interrupção
  //   por exemplo, decrementa o quantum do processo corrente, quando se tem
  //   um escalonador com quantum
}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)\n", irq, irq_nome(irq));
  self->erro_interno = true;
}

// CHAMADAS DE SISTEMA {{{1

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static char *nome_chamada_sistema(int id_chamada)
{
  switch (id_chamada)
  {
  case SO_LE:
    return "Leitura";
    break;
  case SO_ESCR:
    return "Escrita";
    break;
  case SO_CRIA_PROC:
    return "Cria processo";
    break;
  case SO_MATA_PROC:
    return "Mata processo";
    break;
  case SO_ESPERA_PROC:
    return "Espera processo";
    break;
  default:
    return "Desconhecida";
  }
}

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t1: com processos, o reg A tá no descritor do processo corrente
  int id_chamada; // = self->processo_corrente->reg[A];
  if (mem_le(self->mem, IRQ_END_A, &id_chamada) != ERR_OK)
  {
    console_printf("SO: Erro no acesso ao id da chamada de sistema\n");
    self->erro_interno = true;
    return;
  }
  console_printf("SO: Chamada de sistema: %s\n", nome_chamada_sistema(id_chamada));
  switch (id_chamada)
  {
  case SO_LE:
    so_chamada_le(self);
    break;
  case SO_ESCR:
    so_chamada_escr(self);
    break;
  case SO_CRIA_PROC:
    so_chamada_cria_proc(self);
    break;
  case SO_MATA_PROC:
    so_chamada_mata_proc(self);
    break;
  case SO_ESPERA_PROC:
    so_chamada_espera_proc(self);
    break;
  default:
    // t1: deveria matar o processo
    self->erro_interno = true;
  }
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
static void so_chamada_le(so_t *self)
{
  int terminal = obtem_terminal(self->processo_corrente->pid);
  int dispositivo = calcula_dispositivo(D_TERM_A_TECLADO, terminal);
  int dispositivo_ok = calcula_dispositivo(D_TERM_A_TECLADO_OK, terminal);

  int estado;
  if (es_le(self->es, dispositivo_ok, &estado) != ERR_OK)
  {
    console_printf("SO: Problema no acesso ao estado do teclado\n");
    self->erro_interno = true;
    return;
  }
  if (estado == 0)
  {
    mudar_estado(self, self->processo_corrente, ESTADO_BLOQUEADO);
    self->processo_corrente->tipo_bloqueio = BLOCK_LEITURA;
    calcular_prioridade(self, self->processo_corrente);
    return;
  }

  int dado;
  if (es_le(self->es, dispositivo, &dado) != ERR_OK)
  {
    console_printf("SO: Problema no acesso ao teclado do terminal %d\n", terminal);
    self->erro_interno = true;
    return;
  }
  // escreve no reg A do processador
  // (na verdade, na posição onde o processador vai pegar o A quando retornar da int)
  // T1: se houvesse processo, deveria escrever no reg A do processo
  // T1: o acesso só deve ser feito nesse momento se for possível; se não, o processo
  //   é bloqueado, e o acesso só deve ser feito mais tarde (e o processo desbloqueado)
  mem_escreve(self->mem, IRQ_END_A, dado);
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
  int terminal = obtem_terminal(self->processo_corrente->pid);
  int dispositivo = calcula_dispositivo(D_TERM_A_TELA, terminal);
  int dispositivo_ok = calcula_dispositivo(D_TERM_A_TELA_OK, terminal);

  // implementação com espera ocupada
  //   T1: deveria bloquear o processo se dispositivo ocupado
  // implementação escrevendo direto do terminal A
  //   T1: deveria usar o dispositivo de saída corrente do processo

  int estado;
  if (es_le(self->es, dispositivo_ok, &estado) != ERR_OK)
  {
    console_printf("SO: Problema no acesso ao estado da tela\n");
    self->erro_interno = true;
    return;
  }

  if (estado == 0)
  {
    if (mem_le(self->mem, IRQ_END_X, &self->processo_corrente->pendencia) != ERR_OK)
    {
      console_printf("SO: Problema ao ler o valor do registrador X\n");
      self->erro_interno = true;
      return;
    }
    mudar_estado(self, self->processo_corrente, ESTADO_BLOQUEADO);
    self->processo_corrente->tipo_bloqueio = BLOCK_ESCRITA;
    calcular_prioridade(self, self->processo_corrente);
    return;
  }

  int dado;
  if (mem_le(self->mem, IRQ_END_X, &dado) != ERR_OK)
  {
    console_printf("SO: Problema ao ler o valor do registrador X\n");
    self->erro_interno = true;
    return;
  }
  // está lendo o valor de X e escrevendo o de A direto onde o processador colocou/vai pegar
  // T1: deveria usar os registradores do processo que está realizando a E/S
  // T1: caso o processo tenha sido bloqueado, esse acesso deve ser realizado em outra execução
  //   do SO, quando ele verificar que esse acesso já pode ser feito.
  if (es_escreve(self->es, dispositivo, dado) != ERR_OK)
  {
    console_printf("SO: Problema no acesso a tela\n");
    self->erro_interno = true;
    return;
  }
  mem_escreve(self->mem, IRQ_END_A, 0);
}

// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  // T1: deveria criar um novo processo

  // em X está o endereço onde está o nome do arquivo
  int ender_proc;
  // t1: deveria ler o X do descritor do processo criador
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK)
  {
    char nome[100];
    if (copia_str_da_mem(100, nome, self->mem, ender_proc))
    {
      int ender_carga = so_carrega_programa(self, nome);
      if (ender_carga > 0)
      {
        // t1: deveria escrever no PC do descritor do processo criado
        processo_t *processo = alocar_proc(self);
        if (processo == NULL)
          return;
        mem_escreve(self->mem, IRQ_END_PC, ender_carga);
        inicializar_processo(processo, self->pid_contador++, ender_carga);

        self->processo_corrente->reg[A] = processo->pid;

        if (!adicionar_processo(self, processo))
          return;

        if (!fila_enfileirar(self->processos_prontos, processo))
        {
          console_printf("SO: Erro ao enfilar processo pronto\n");
          self->erro_interno = true;
          return;
        }
        return;
      }
      else
        console_printf("SO: Erro na carga inicial\n");
    }
    else
      console_printf("SO: Erro ao copiar o nome do arquivo da memória\n");
  }
  else
  {
    console_printf("SO: Erro ao acessar o endereço do nome do arquivo\n");
    self->erro_interno = true;
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  mem_escreve(self->mem, IRQ_END_A, -1);
}

static processo_t *encontrar_pid(so_t *self, int pid)
{
  processo_t *proc = NULL;
  for (int i = 0; i < self->n_processos; i++)
  {
    if (self->tabela_processos[i] && self->tabela_processos[i]->pid == pid)
    {
      proc = self->tabela_processos[i];
      break;
    }
  }
  return proc;
}
// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  int pid = self->processo_corrente->reg[X];
  if (pid == 0)
  {
    console_printf("SO: Matando processo o proprio processo, PID: %d\n", self->processo_corrente->pid);
    pid = self->processo_corrente->pid;
    mudar_estado(self, self->processo_corrente, ESTADO_MORTO);
    self->processo_corrente = NULL;
    mem_escreve(self->mem, IRQ_END_A, 0);
    return;
  }
  else
  {
    console_printf("SO: Matando processo com PID %d\n", pid);
    processo_t *proc = encontrar_pid(self, pid);
    if (proc != NULL)
    {
      mudar_estado(self, proc, ESTADO_MORTO);
      mem_escreve(self->mem, IRQ_END_A, 0);
      if (!remover_processo_fila(self->processos_prontos, proc))
        console_printf("SO: Processo nao esta na fila de prontos\n");

      return;
    }
  }
  // T1: deveria matar um processo
  // ainda sem suporte a processos, retorna erro -1
  console_printf("SO: Processo com PID %d nao encontrado\n", pid);
  mem_escreve(self->mem, IRQ_END_A, -1);
}
// A = 0
// X = 1
// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self)
{
  int pid = self->processo_corrente->reg[X];
  if (pid == self->processo_corrente->pid)
  {
    console_printf("SO: Processo não pode esperar por si mesmo\n");
    mem_escreve(self->mem, IRQ_END_A, -1);
    return;
  }
  processo_t *proc_esperado = encontrar_pid(self, pid);

  if (proc_esperado == NULL)
  {
    console_printf("SO: Processo com PID %d nao encontrado\n", pid);
    mem_escreve(self->mem, IRQ_END_A, -1);
    return;
  }
  if (proc_esperado->estado == ESTADO_MORTO)
  {
    console_printf("SO: Processo com PID %d ja esta morto\n", pid);
    mem_escreve(self->mem, IRQ_END_A, 0);
    return;
  }
  // Bloqueia o processo chamador
  console_printf("SO: Processo %d bloqueado aguardando termino de %d\n", self->processo_corrente->pid, pid);

  mudar_estado(self, self->processo_corrente, ESTADO_BLOQUEADO);
  self->processo_corrente->tipo_bloqueio = BLOCK_ESPERA_PROC;
  calcular_prioridade(self, self->processo_corrente);
  self->processo_corrente->reg[A] = pid;
  mem_escreve(self->mem, IRQ_END_A, 0);
  // T1: deveria bloquear o processo se for o caso (e desbloquear na morte do esperado)
  // ainda sem suporte a processos, retorna erro -1
}

// CARGA DE PROGRAMA {{{1

// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL)
  {
    console_printf("SO: Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++)
  {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK)
    {
      console_printf("SO: Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: Carga de '%s' em %d-%d\n", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}

// ACESSO À MEMÓRIA DOS PROCESSOS {{{1

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// T1: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++)
  {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK)
    {
      return false;
    }
    if (caractere < 0 || caractere > 255)
    {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0)
    {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}

// vim: foldmethod=marker
