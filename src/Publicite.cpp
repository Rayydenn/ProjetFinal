#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "protocole.h" // contient la cle et la structure d'un message

int idQ, idShm;
int fd;

void handlerSIGUSR1(int sig);

int main()
{
  // Armement des signaux

  // Masquage de SIGINT
  sigset_t mask;
  sigfillset(&mask);
  sigaddset(&mask,SIGINT);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"(PUBLICITE %d) Recuperation de l'id de la file de messages\n",getpid());
  idQ = msgget(CLE, 0);
  if (idQ == -1)
  {
    perror("Erreur de la récuperation de l'id de la file");
    exit(1);
  }

  // Recuperation de l'identifiant de la mémoire partagée
  fprintf(stderr,"(PUBLICITE %d) Recuperation de l'id de la mémoire partagée\n",getpid());
  idShm = shmget(CLE, 200, 0);  // Acceder segmentmem
  if (idShm == -1)
  {
    perror("Erreur de la récuperation de l'id de la mémoire partagée");
    exit(1);
  }

  // Attachement à la mémoire partagée
  char* memPub = (char*)shmat(idShm, NULL, 0);
  if (memPub == (char*)-1)
  {
    perror("Erreur d'attachement a la mémoire partagée");
    exit(1);
  }

  // Ouverture du fichier de publicité
  fd = open("publicites.dat", O_RDONLY);
  if (fd == -1)
  {
    perror("Erreur d'ouverture du fichier publicites.dat");
    exit(1);
  }

  while(1)
  {
  	PUBLICITE pub;
    // Lecture d'une publicité dans le fichier
    if (read(fd, &pub, sizeof(PUBLICITE)) == 0)
    {
      // Fin du fichier
      lseek(fd,0,SEEK_SET); // Revenir au début
    }
    if (read(fd, &pub, sizeof(PUBLICITE)) < 0)
    {
      perror("Erreur lors de la lecture du fichier");
      exit(1);
    }

    // Ecriture en mémoire partagée
    strncpy(memPub, pub.texte, 199);
    memPub[199] = '\0';
    fprintf(stderr, "(PUBLICITE %d) Nouvelle publicite: %s (%d secondes)\n", getpid(), memPub, pub.nbSecondes);

    // Envoi d'une requete UPDATE_PUB au serveur

    MESSAGE m;
    m.type = 1;
    m.expediteur = getpid();
    m.requete = UPDATE_PUB;

    if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1) // Envoyer un message
    {
      perror("Erreur lors de l'envoie de la requete UPDATE_PUB");
      exit(1);
    }

    // Attente du nombre de secondes indiqué

    if (pub.nbSecondes > 0)
      sleep(pub.nbSecondes);

  }

  return 0;
}


void handlerSIGUSR1(int sig)
{
  fprintf(stderr, "(PUBLICITE %d) Nouvelle publicite !\n", getpid());

}