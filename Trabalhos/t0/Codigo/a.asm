
teclado  define 0
teclOK   define 1
tela     define 2
telaOK   define 3

NUM_ALERT DEFINE 18

; Início do programa
         cargi str1
         chama impstr
         
         ; Gera número aleatório
         chama geranumero   ; Chama função que gera o número aleatório
         armm chute         ; Armazena o número aleatório em 'chute'
         
         ; Calcula o valor secreto
         cargm chute
         SOMA ch_a           ; Adiciona o valor de 'a' (97)
         armm segredo       ; Armazena o resultado como o segredo

laco     chama lechute
         chama vechute
         desvz laco
         para

str1     string 'Olá. Escolhi uma letra minúscula. Adivinha qual.       '
str2     valor 10 ; \n, limpa a linha (o montador não entende \n dentro da string)
         string 'Digite uma letra minúscula '

; Le um caractere do usuário, até que seja minúscula
lechute  espaco 1
lechute1 cargi str2
         chama impstr
lechute2
         chama lechar
         armm lechtmp
         sub ch_a
         ; < 'a', lê de novo
         desvn lechute2
         cargm lechtmp
         sub ch_z
         ; > 'z', lê de novo
         desvp lechute2
         ; Retorna o caractere lido
         cargm lechtmp
         ret lechute
ch_a     valor 'a'
ch_z     valor 'z'
lechtmp  espaco 1

; Le um caractere do teclado
lechar   espaco 1
         ; Espera o teclado ter alguma tecla
lechar1  le teclOK
         desvz lechar1
         ; Lê a tecla e retorna
         le teclado
         ret lechar

; Verifica se chute em A é bom, escreve mensagem, retorna 1 se for
vechute  espaco 1
         ; Coloca o chute no meio da msg e imprime
         armm chute
         cargi msg_chut
         chama impstr
         ; Recupera o chute, compara com o segredo
         cargm chute
         sub segredo
         ; Se for igual, acertou
         desvz chuteok
         ; Senão, pode ser grande ou pequeno
         desvp chuteg
         cargi msg_peq
         desv vechute1
chuteg   cargi msg_gr
vechute1 chama impstr
         ; Retorna false
         cargi 0
         ret vechute
chuteok  cargi msg_ok
         chama impstr
         ; Retorna true
         cargi 1
         ret vechute
msg_peq  string 'muito pequeno, tente novamente '
msg_gr   string 'muito grande, tente novamente '
msg_ok   string 'parabéns, você acertou!!'
msg_chut valor 10
         valor "'"
chute    espaco 1
         string "' "
segredo  valor 'a'   ; Espaço para armazenar o caractere secreto

; Função para gerar um número aleatório de 0 a 25
geranumero espaco 1
         ; Gera um número aleatório entre 0 e 25
         LE NUM_ALERT       ; Lê um número aleatório (ex: de 0 a 25)
         ESCR 2             ; Exibe o número gerado
         armm chute          ; Armazena o número gerado em 'chute'
         ret geranumero      ; Retorna para a chamada anterior


impstr   espaco 1
         trax
impstr1
         cargx 0
         desvz impstrf
         chama impch
         incx
         desV impstr1
impstrf  ret impstr

; Imprime o caractere em A (não altera X)
impch    espaco 1
         ; Salva o caractere
         armm impcht
         ; Espera o terminal ficar livre
impch1   le telaOK
         desvz impch1
         ; Imprime o caractere salvo
         cargm impcht
         escr tela
         ret impch
impcht   espaco 1
