#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_USUARIOS 100
#define NUM_MOEDAS   3
#define ARQ_USUARIOS "usuarios.txt"
#define ARQ_COTACOES "cotacoes.txt"
#define ARQ_EXTRATO  "extrato.txt"

static const char  *SIGLAS[NUM_MOEDAS]       = {"BTC", "ETH", "XRP"};
static const char  *NOMES[NUM_MOEDAS]        = {"Bitcoin", "Ethereum", "Ripple"};
static const double TAXA_COMPRA[NUM_MOEDAS]  = {0.02, 0.01, 0.01};
static const double TAXA_VENDA[NUM_MOEDAS]   = {0.03, 0.02, 0.01};

typedef struct {
    char   cpf[12];
    char   senha[7];
    char   nome[50];
    double reais;
    double cripto[NUM_MOEDAS];
} Usuario;

static Usuario usuarios[MAX_USUARIOS];
static int     numUsuarios = 0;
static double  cotacoes[NUM_MOEDAS];

void lerTexto(const char *msg, char *dest, int tam) {
    printf("%s", msg);
    if (fgets(dest, tam, stdin) == NULL) {
        printf("\nEntrada encerrada.\n");
        exit(0);
    }
    size_t n = strlen(dest);
    if (n > 0 && dest[n - 1] == '\n') {
        dest[n - 1] = '\0';
    } else {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

double lerValor(const char *msg) {
    char buf[50];
    lerTexto(msg, buf, sizeof buf);
    for (char *p = buf; *p; p++) {
        if (*p == ',') *p = '.';
    }
    char *fim;
    double v = strtod(buf, &fim);
    if (fim == buf || *fim != '\0' || !(v >= 0.01) || v > 1e12) {
        return -1;
    }
    return v;
}

int lerInteiro(const char *msg) {
    char buf[20];
    lerTexto(msg, buf, sizeof buf);
    char *fim;
    long v = strtol(buf, &fim, 10);
    if (fim == buf || *fim != '\0') {
        return -1;
    }
    return (int)v;
}

int confirmar(const char *msg) {
    char buf[10];
    lerTexto(msg, buf, sizeof buf);
    return (buf[0] == 's' || buf[0] == 'S');
}

int soDigitos(const char *s, int n) {
    if ((int)strlen(s) != n) return 0;
    for (int i = 0; i < n; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

double arredondar(double v) {
    return (double)(long long)(v * 100.0 + 0.5) / 100.0;
}

int arquivoExiste(const char *nome) {
    FILE *f = fopen(nome, "r");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

void criarArquivosIniciais(void) {
    FILE *f;

    if (!arquivoExiste(ARQ_USUARIOS)) {
        f = fopen(ARQ_USUARIOS, "w");
        if (f != NULL) {
            fprintf(f, "12345678900;123456;Investidor Teste;1000.00;0.00000000;0.00000000;0.00000000\n");
            fprintf(f, "98765432100;654321;Maria Silva;500.00;0.00000000;0.00000000;0.00000000\n");
            fclose(f);
        }
    }
    if (!arquivoExiste(ARQ_COTACOES)) {
        f = fopen(ARQ_COTACOES, "w");
        if (f != NULL) {
            fprintf(f, "400000.000000 16108.000000 2.570000\n");
            fclose(f);
        }
    }
    if (!arquivoExiste(ARQ_EXTRATO)) {
        f = fopen(ARQ_EXTRATO, "w");
        if (f != NULL) fclose(f);
    }
}

void carregarUsuarios(void) {
    FILE *f = fopen(ARQ_USUARIOS, "r");
    char linha[256];
    numUsuarios = 0;
    if (f == NULL) return;

    while (numUsuarios < MAX_USUARIOS && fgets(linha, sizeof linha, f) != NULL) {
        Usuario u;
        int lidos = sscanf(linha, "%11[^;];%6[^;];%49[^;];%lf;%lf;%lf;%lf",
                           u.cpf, u.senha, u.nome, &u.reais,
                           &u.cripto[0], &u.cripto[1], &u.cripto[2]);
        if (lidos == 7) {
            usuarios[numUsuarios++] = u;
        }
    }
    fclose(f);
}

void salvarUsuarios(void) {
    FILE *f = fopen(ARQ_USUARIOS, "w");
    if (f == NULL) {
        printf("ERRO: nao foi possivel gravar %s\n", ARQ_USUARIOS);
        return;
    }
    for (int i = 0; i < numUsuarios; i++) {
        Usuario *u = &usuarios[i];
        fprintf(f, "%s;%s;%s;%.2f;%.8f;%.8f;%.8f\n", u->cpf, u->senha, u->nome,
                u->reais, u->cripto[0], u->cripto[1], u->cripto[2]);
    }
    fclose(f);
}

void carregarCotacoes(void) {
    FILE *f = fopen(ARQ_COTACOES, "r");
    if (f == NULL) return;
    if (fscanf(f, "%lf %lf %lf", &cotacoes[0], &cotacoes[1], &cotacoes[2]) != 3) {
        printf("ERRO: arquivo %s corrompido.\n", ARQ_COTACOES);
        exit(1);
    }
    fclose(f);
}

void salvarCotacoes(void) {
    FILE *f = fopen(ARQ_COTACOES, "w");
    if (f == NULL) {
        printf("ERRO: nao foi possivel gravar %s\n", ARQ_COTACOES);
        return;
    }
    fprintf(f, "%.10f %.10f %.10f\n", cotacoes[0], cotacoes[1], cotacoes[2]);
    fclose(f);
}

void registrarExtrato(const Usuario *u, char sinal, double valor,
                      const char *moeda, double ct, double tx) {
    FILE *f = fopen(ARQ_EXTRATO, "a");
    if (f == NULL) {
        printf("ERRO: nao foi possivel gravar %s\n", ARQ_EXTRATO);
        return;
    }
    time_t agora = time(NULL);
    struct tm *d = localtime(&agora);

    fprintf(f, "%s;%02d-%02d-%04d %02d:%02d %c %.2f %s CT: %.2f TX: %.2f "
               "REAL: %.2f BTC: %.6f ETH: %.6f XRP: %.6f\n",
            u->cpf, d->tm_mday, d->tm_mon + 1, d->tm_year + 1900,
            d->tm_hour, d->tm_min, sinal, valor, moeda, ct, tx,
            u->reais, u->cripto[0], u->cripto[1], u->cripto[2]);
    fclose(f);
}

void formatarCPF(const char *cpf, char *out) {
    sprintf(out, "%.3s.%.3s.%.3s-%.2s", cpf, cpf + 3, cpf + 6, cpf + 9);
}

void mostrarCabecalho(const Usuario *u) {
    char cpfFmt[15];
    formatarCPF(u->cpf, cpfFmt);
    printf("\nNome: %s\nCPF: %s\n", u->nome, cpfFmt);
}

void mostrarSaldos(const Usuario *u) {
    mostrarCabecalho(u);
    printf("Reais: %.2f\n", u->reais);
    for (int i = 0; i < NUM_MOEDAS; i++) {
        printf("%s: %.6f\n", NOMES[i], u->cripto[i]);
    }
}

void mostrarCotacoes(void) {
    printf("\n--- Cotações atuais ---\n");
    for (int i = 0; i < NUM_MOEDAS; i++) {
        printf("%d. %-9s R$ %.2f  (taxa compra %.0f%% | venda %.0f%%)\n",
               i + 1, NOMES[i], cotacoes[i],
               TAXA_COMPRA[i] * 100, TAXA_VENDA[i] * 100);
    }
}

int fazerLogin(void) {
    for (int tentativa = 1; tentativa <= 3; tentativa++) {
        char cpf[50], senha[50];
        lerTexto("\nCPF (sem pontos e sem traços): ", cpf, sizeof cpf);
        lerTexto("Senha (6 dígitos): ", senha, sizeof senha);

        if (soDigitos(cpf, 11) && soDigitos(senha, 6)) {
            for (int i = 0; i < numUsuarios; i++) {
                if (strcmp(usuarios[i].cpf, cpf) == 0 &&
                    strcmp(usuarios[i].senha, senha) == 0) {
                    return i;
                }
            }
        }
        printf("CPF ou senha incorretos (tentativa %d de 3).\n", tentativa);
    }
    return -1;
}

int confirmarSenha(const Usuario *u) {
    for (int tentativa = 1; tentativa <= 3; tentativa++) {
        char senha[50];
        lerTexto("Senha (6 dígitos): ", senha, sizeof senha);
        if (soDigitos(senha, 6) && strcmp(senha, u->senha) == 0) {
            return 1;
        }
        printf("Senha incorreta (tentativa %d de 3).\n", tentativa);
    }
    printf("Operação cancelada.\n");
    return 0;
}

void consultarSaldo(const Usuario *u) {
    if (!confirmarSenha(u)) return;
    mostrarSaldos(u);
}

void consultarExtrato(const Usuario *u) {
    if (!confirmarSenha(u)) return;
    mostrarCabecalho(u);
    printf("\n");

    FILE *f = fopen(ARQ_EXTRATO, "r");
    if (f == NULL) {
        printf("Nenhuma operação registrada.\n");
        return;
    }
    char linha[300];
    int achou = 0;
    while (fgets(linha, sizeof linha, f) != NULL) {

        if (strncmp(linha, u->cpf, 11) == 0 && linha[11] == ';') {
            printf("%s", linha + 12);
            achou = 1;
        }
    }
    fclose(f);
    if (!achou) printf("Nenhuma operação registrada.\n");
}

void depositar(Usuario *u) {
    double valor = lerValor("Valor do depósito: R$ ");
    if (valor < 0) {
        printf("Valor inválido.\n");
        return;
    }
    valor = arredondar(valor);
    u->reais = arredondar(u->reais + valor);

    registrarExtrato(u, '+', valor, "REAL", 0.0, 0.0);
    salvarUsuarios();
    printf("\nDepósito realizado!");
    mostrarSaldos(u);
}

void sacar(Usuario *u) {
    double valor = lerValor("Valor do saque: R$ ");
    if (valor < 0) {
        printf("Valor inválido.\n");
        return;
    }
    valor = arredondar(valor);
    if (valor > u->reais) {
        printf("Saldo insuficiente. Você tem R$ %.2f.\n", u->reais);
        return;
    }
    u->reais = arredondar(u->reais - valor);

    registrarExtrato(u, '-', valor, "REAL", 0.0, 0.0);
    salvarUsuarios();
    printf("\nSaque realizado!");
    mostrarSaldos(u);
}

int escolherMoeda(const char *acao) {
    char msg[60];
    sprintf(msg, "Qual criptomoeda deseja %s (1-3)? ", acao);
    int op = lerInteiro(msg);
    if (op < 1 || op > NUM_MOEDAS) {
        printf("Opção inválida.\n");
        return -1;
    }
    return op - 1;
}

void comprar(Usuario *u) {
    if (!confirmarSenha(u)) return;
    mostrarCotacoes();

    int m = escolherMoeda("comprar");
    if (m < 0) return;
    if (cotacoes[m] <= 0) {
        printf("Cotação indisponível para esta moeda.\n");
        return;
    }

    double valor = lerValor("Quanto deseja comprar (em reais, sem a taxa): R$ ");
    if (valor < 0) {
        printf("Valor inválido.\n");
        return;
    }
    valor = arredondar(valor);

    double taxa  = arredondar(valor * TAXA_COMPRA[m]);
    double total = valor + taxa;
    double qtd   = valor / cotacoes[m];

    if (total > u->reais) {
        printf("Saldo insuficiente. Total com taxa: R$ %.2f | Seu saldo: R$ %.2f.\n",
               total, u->reais);
        return;
    }

    printf("\nResumo: %.6f %s | taxa R$ %.2f | total debitado R$ %.2f\n",
           qtd, SIGLAS[m], taxa, total);
    if (!confirmar("Confirmar compra (s/n)? ")) {
        printf("Compra cancelada.\n");
        return;
    }

    u->reais = arredondar(u->reais - total);
    u->cripto[m] += qtd;

    registrarExtrato(u, '+', valor, SIGLAS[m], cotacoes[m], TAXA_COMPRA[m]);
    salvarUsuarios();
    printf("\nCompra realizada!");
    mostrarSaldos(u);
}

void vender(Usuario *u) {
    if (!confirmarSenha(u)) return;
    mostrarCotacoes();

    int m = escolherMoeda("vender");
    if (m < 0) return;
    if (cotacoes[m] <= 0) {
        printf("Cotação indisponível para esta moeda.\n");
        return;
    }

    printf("Seu saldo em %s: %.6f (≈ R$ %.2f)\n", SIGLAS[m], u->cripto[m],
           u->cripto[m] * cotacoes[m]);
    double valor = lerValor("Quanto deseja vender (em reais, antes da taxa): R$ ");
    if (valor < 0) {
        printf("Valor inválido.\n");
        return;
    }
    valor = arredondar(valor);

    double maxVenda = arredondar(u->cripto[m] * cotacoes[m]);
    if (valor > maxVenda + 1e-9) {
        printf("Saldo de %s insuficiente.\n", NOMES[m]);
        return;
    }
    double qtd = valor / cotacoes[m];
    if (qtd > u->cripto[m]) qtd = u->cripto[m];

    double taxa    = arredondar(valor * TAXA_VENDA[m]);
    double recebe  = valor - taxa;

    printf("\nResumo: venda de %.6f %s | taxa R$ %.2f | você recebe R$ %.2f\n",
           qtd, SIGLAS[m], taxa, recebe);
    if (!confirmar("Confirmar venda (s/n)? ")) {
        printf("Venda cancelada.\n");
        return;
    }

    u->cripto[m] -= qtd;
    if (u->cripto[m] < 0) u->cripto[m] = 0;
    u->reais = arredondar(u->reais + recebe);

    registrarExtrato(u, '-', valor, SIGLAS[m], cotacoes[m], TAXA_VENDA[m]);
    salvarUsuarios();
    printf("\nVenda realizada!");
    mostrarSaldos(u);
}

void atualizarCotacao(void) {
    printf("\n--- Atualizando cotações ---\n");
    for (int i = 0; i < NUM_MOEDAS; i++) {
        double antiga = cotacoes[i];
        double variacao = ((double)rand() / RAND_MAX) * 0.10 - 0.05;
        cotacoes[i] = antiga * (1.0 + variacao);
        printf("%-9s R$ %.2f -> R$ %.2f (%+.2f%%)\n",
               NOMES[i], antiga, cotacoes[i], variacao * 100);
    }
    salvarCotacoes();
}

void mostrarMenu(void) {
    printf("\n===== MENU =====\n");
    printf("1. Consultar saldo\n");
    printf("2. Consultar extrato\n");
    printf("3. Depositar\n");
    printf("4. Sacar\n");
    printf("5. Comprar criptomoedas\n");
    printf("6. Vender criptomoedas\n");
    printf("7. Atualizar cotação\n");
    printf("8. Sair\n");
}

int main(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    srand((unsigned)time(NULL));

    criarArquivosIniciais();
    carregarUsuarios();
    carregarCotacoes();

    printf("=== EXCHANGE DE CRIPTOMOEDAS ===\n");

    int idx = fazerLogin();
    if (idx < 0) {
        printf("Acesso bloqueado após 3 tentativas.\n");
        return 1;
    }
    Usuario *u = &usuarios[idx];
    printf("\nBem-vindo(a), %s!\n", u->nome);

    int opcao;
    do {
        mostrarMenu();
        opcao = lerInteiro("Escolha uma opção: ");

        switch (opcao) {
            case 1: consultarSaldo(u);   break;
            case 2: consultarExtrato(u); break;
            case 3: depositar(u);        break;
            case 4: sacar(u);            break;
            case 5: comprar(u);          break;
            case 6: vender(u);           break;
            case 7: atualizarCotacao();  break;
            case 8: printf("\nAté logo!\n"); break;
            default: printf("Opção inválida.\n");
        }
    } while (opcao != 8);

    return 0;
}