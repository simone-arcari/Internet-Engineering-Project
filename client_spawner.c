#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int num_clients = atoi(argv[1]); // Numero di processi client da avviare

    for (int i = 0; i < num_clients; i++) {
        pid_t pid = fork(); // Crea un nuovo processo

        if (pid == 0) {
            // Questo è il processo figlio

            // Parametri da passare al client
            char *args[] = {"gnome-terminal", "--", "./client", "get", "test.pdf", NULL};

            // Esegue il comando gnome-terminal con i parametri specificati
            execvp("gnome-terminal", args);

            // Se execvp fallisce, stampa un messaggio di errore
            perror("Errore nell'esecuzione di gnome-terminal");
            exit(EXIT_FAILURE);

        } else if (pid < 0) {
            // La fork() ha fallito
            perror("Errore nella fork");
            exit(EXIT_FAILURE);

        }
    }

    printf("Avviati %d processi client.\n", num_clients);

    return 0;
}
