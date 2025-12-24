#include "windowadmin.h"
#include <QApplication>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int idQ;

int main(int argc, char *argv[])
{
    // Recuperation de l'identifiant de la file de messages
    fprintf(stderr,"(ADMINISTRATEUR %d) Recuperation de l'id de la file de messages\n",getpid());

    idQ = msgget(CLE, 0);

    if (idQ == -1)
    {
        perror("Erreur de récupération de la file de message (ADMIN)");
        exit(1);
    }

    // Envoi d'une requete de connexion au serveur
    MESSAGE m;

    m.type = 1;
    m.requete = LOGIN_ADMIN;
    m.expediteur = getpid();

    if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
        perror("(ADMINISTRATEUR) Erreur de send LOGIN_ADMIN");
        exit(1);
    }

    // Attente de la réponse
    fprintf(stderr,"(ADMINISTRATEUR %d) Attente reponse\n",getpid());
    if (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(), 0) == -1)
    {
        perror("Erreur de receive LOGIN_ADMIN");
        exit(1);
    }

    if (strcmp(m.data1, "OK") == 0)
    {
        QApplication a(argc, argv);
        WindowAdmin w;
        w.show();
        return a.exec();
    }
    else
    {
        fprintf(stderr, "Une fenetre administrateur est déjà en cours\n");
        return -1;
    }


}
