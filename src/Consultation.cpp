#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <mysql.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "protocole.h"

int idQ,idSem;

int main()
{
  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"(CONSULTATION %d) Recuperation de l'id de la file de messages\n",getpid());

  if ((idQ = msgget(CLE, 0)) == -1)
  {
    perror("Erreur de la récuperation de la file de messages");
    exit(1);
  }

  // Recuperation de l'identifiant du sémaphore
  idSem = semget(CLE, 1, 0);
  if (idSem == -1)
  {
    perror("Erreur de récupération de l'identifiant du sémaphore");
    exit(1);
  }

  MESSAGE m;
  // Lecture de la requête CONSULT
  fprintf(stderr,"(CONSULTATION %d) Lecture requete CONSULT\n",getpid());
  msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(),0);

  // Tentative de prise bloquante du semaphore 0
  fprintf(stderr,"(CONSULTATION %d) Prise bloquante du sémaphore 0\n",getpid());

  struct sembuf op;
  op.sem_num = 0;
  op.sem_op  = -1;
  op.sem_flg = 0;

  if (semop(idSem, &op, 1) == -1)
  {
      perror("(CONSULTATION) semop take");
      exit(1);
  }

  fprintf(stderr,"(CLIENT %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      

  // Connexion à la base de donnée
  MYSQL *connexion = mysql_init(NULL);
  fprintf(stderr,"(CONSULTATION %d) Connexion à la BD\n",getpid());
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(CONSULTATION) Erreur de connexion à la base de données...\n");
    fprintf(stderr,"(CONSULTATION) mysql_error: %s\n", mysql_error(connexion));
    exit(1);  
  }

  // Recherche des infos dans la base de données
  fprintf(stderr,"(CONSULTATION %d) Consultation en BD (%s)\n",getpid(),m.data1);
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  char requete[200];

  sprintf(requete, 
            "SELECT gsm, email FROM UNIX_FINAL where nom = '%s'", m.data1);

  if(mysql_query(connexion,requete) != 0)
  {
    fprintf(stderr,"(CONSULTATION) mysql_error: %s\n", mysql_error(connexion));
    exit(1);
  }

  resultat = mysql_store_result(connexion);

  if ((tuple = mysql_fetch_row(resultat)) != NULL)
  {
    strcpy(m.data1, "OK");
    for (int i = 0; i < 2;i++)
    {
      if (tuple[i] == NULL)
      {
        strcpy(tuple[i], "NON TROUVE");
      }
    }
    strcpy(m.data2, tuple[0]); // gsm
    strcpy(m.texte, tuple[1]); // email
  }
  else
  {
    strcpy(m.data1, "KO");
    strcpy(m.texte, "NON TROUVE");
    strcpy(m.data2, "NON TROUVE");
  }

  // Construction et envoi de la reponse
  m.requete = CONSULT;
  m.type = m.expediteur;
  m.expediteur = getpid();

  msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0);
  kill(m.type, SIGUSR1);

  // Deconnexion BD
  if (resultat) mysql_free_result(resultat);
  mysql_close(connexion);

  // Libération du semaphore 0
  fprintf(stderr,"(CONSULTATION %d) Libération du sémaphore 0\n",getpid());
  op.sem_op = 1;
  
  if (semop(idSem, &op, 1) == -1)
  {
    perror("(CONSULTATION) semop release");
  }

  exit(0);
}