#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <mysql.h>
#include <setjmp.h>
#include <cerrno>
#include "protocole.h" // contient la cle et la structure d'un message
#include "FichierUtilisateur.h" // contient estPresent pour le login

int idQ,idShm,idSem;
TAB_CONNEXIONS *tab;
int idFilsPub, idFilsAccesBD, idFilsConsult, pidmodif;
sigjmp_buf jumpBuffer;
int fdPipe[2];
int temp;

void afficheTab();
void HandlerSIGINT(int sig);
void HandlerSIGCHLD(int sig);

MYSQL* connexion;


int main()
{
  // Connection à la BD
  connexion = mysql_init(NULL);
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }

  // Armement des signaux
  fprintf(stderr,"(Traitement %d) Armement du signal SIGALRM\n",getpid());

  struct sigaction A;

  A.sa_handler = HandlerSIGINT;
  sigemptyset(&A.sa_mask);
  A.sa_flags = 0;

  if ((sigaction(SIGINT, &A, NULL)) == -1)
  {
    perror("(SERVEUR) Erreur lors de l'armement de SIGINT\n");
    exit(1);
  }
  printf("(SERVEUR) Le signal SINGINT à bien été armé \n");

  struct sigaction C;

  C.sa_handler = HandlerSIGCHLD;
  sigemptyset(&C.sa_mask);
  C.sa_flags = 0;

  if ((sigaction(SIGCHLD, &C, NULL)) == -1)
  {
    perror("(SERVEUR) Erreur lors de l'armement de SIGCHLD1\n");
    exit(1);
  }
  printf("(SERVEUR) Le signal SIGCHLD à bien été armé \n");

  // Creation des ressources
   fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());
  if ((idQ = msgget(CLE, IPC_CREAT | 0600)) == -1)  // CLE definie dans protocole.h
  {
    perror("(SERVEUR) Erreur de msgget");
    exit(EXIT_FAILURE);
  }
  ;
  if ((idShm = shmget(CLE, 200, IPC_CREAT | 0600)) == -1) {
    perror("(SERVEUR) Erreur de shmget");
    msgctl(idQ, IPC_RMID, NULL);
    exit(EXIT_FAILURE);
  }
  fprintf(stderr,"(SERVEUR %d) memoire partagee cree idShm=%d\n", getpid(), idShm);

  idSem = semget(CLE, 1, IPC_CREAT | 0600);
  if (idSem == -1)
  {
    perror("(SERVEUR) Erreur semget");
    exit(1);
  }
  union semun
  {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
  };

  union semun arg;
  arg.val = 1;

  if (semctl(idSem, 0, SETVAL, arg) == -1)
  {
      perror("(SERVEUR) Erreur semctl SETVAL");
      exit(1);
  }

  // Initialisation du tableau de connexions
  fprintf(stderr,"(SERVEUR %d) Initialisation de la table des connexions\n",getpid());
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    for (int j=0 ; j<5 ; j++) tab->connexions[i].autres[j] = 0;
    tab->connexions[i].pidModification = 0;
  }
  tab->pidServeur1 = getpid();
  tab->pidServeur2 = 0;
  tab->pidAdmin = 0;
  tab->pidPublicite = 0;

  afficheTab();

  // Creation du processus Publicite

  int i,k,j, pid;
  MESSAGE m;
  MESSAGE reponse;
  char requete[200];
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  PUBLICITE pub;

  pid = fork();

  if (pid == 0)
  {
    execl("./Publicite", "./Publicite", NULL);
  }
  tab->pidPublicite = pid;

  while(1)
  {
    int temp = -1;
    int absent = 0;
    i = 0;
    int pos;
    const char* nvclient = "1";
    char deconnecte[50];
    int position;

  	fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      if (errno == EINTR)
      {
          // Interruption par signal : PAS une erreur
          continue;   // boucle
      }
      perror("(SERVEUR) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }

    switch(m.requete)
    {
      case CONNECT :  
                      fprintf(stderr,"(SERVEUR %d) Requete CONNECT reçue de %d\n",getpid(),m.expediteur);
                      for (i = 0;i<6;i++)
                      {
                        if (tab->connexions[i].pidFenetre == 0)
                        {
                          tab->connexions[i].pidFenetre = m.expediteur;
                          break;
                        }
                      }
                      break; 

      case DECONNECT :  
                      fprintf(stderr,"(SERVEUR %d) Requete DECONNECT reçue de %d\n",getpid(),m.expediteur);
                      while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur)
                      {
                        i++;
                      }
                      tab->connexions[i].pidFenetre = 0;
                      strcpy(tab->connexions[i].nom, "");
                      break; 

      case LOGIN :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN reçue de %d : --%s--%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2,m.texte);

                      MESSAGE reponse;
                      position = estPresent(m.data2);

                      if (strlen(m.data2) == 0 || strlen(m.texte) == 0)
                      {
                        strcpy(reponse.data1, "KO");
                        strcpy(reponse.texte, "Veuillez entrer un utilisateur et un mot de passe !\n");
                        reponse.type = m.expediteur;
                        reponse.expediteur = getpid();
                        reponse.requete = LOGIN;
                        if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1) {
                            perror("(SERVEUR) Envoi du login");
                            msgctl(idQ, IPC_RMID, NULL);
                            exit(1);
                        }
                        kill(m.expediteur, SIGUSR1);
                        break;
                      }

                      if (strcmp(m.data1, nvclient) == 0) // Absent
                      {
                        if (position == 0)
                        {
                          ajouteUtilisateur(m.data2, m.texte);

                          char requete[256];

                          sprintf(requete, 
                                      "INSERT INTO UNIX_FINAL VALUES (NULL,'%s', '---', '---');", m.data2);
                          
                          if (mysql_query(connexion, requete) == 0)
                          {
                            fprintf(stderr, "INSERT OK\n");
                          }

                          strcpy(reponse.data1, "OK");
                          strcpy(reponse.texte, "Utilisateur ajouté\n");
                          i = 0;
                          while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur)
                            i++;
                          tab->connexions[i].nom[0] = '\0';
                          strcpy(tab->connexions[i].nom, m.data2);

                          // Notifier
                          for (int k = 0; k < 6;k++)
                          {
                            if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                            {
                              MESSAGE add;
                              add.type = tab->connexions[k].pidFenetre;
                              add.expediteur = getpid();
                              add.requete = ADD_USER;
                              strcpy(add.data1, m.data2); // Nouveau nom
                              msgsnd(idQ, &add, sizeof(MESSAGE) - sizeof(long), 0);
                              kill(tab->connexions[k].pidFenetre, SIGUSR1);
                            }
                          }
                          for (int k = 0;k<6;k++)
                          {
                            if (tab->connexions[k].pidFenetre != 0; tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                            {
                              MESSAGE add;
                              add.type = m.expediteur;
                              add.expediteur = getpid();
                              add.requete = ADD_USER;
                              strcpy(add.data1, tab->connexions[k].nom);
                              msgsnd(idQ, &add, sizeof(MESSAGE) - sizeof(long), 0);
                              kill(m.expediteur, SIGUSR1);
                            }
                          }
                        }

                        else
                        {
                          strcpy(reponse.texte, "L'Utilisateur existe déjà!\n");
                          strcpy(reponse.data1, "KO");
                        }
                      }

                      else // Connexion
                      {
                        bool memenom = false;
                        for (int k = 0; k < 6; k++)
                        {
                          if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strcmp(tab->connexions[k].nom, m.data2) == 0)
                          {
                            strcpy(reponse.data1, "KO");
                            strcpy(reponse.texte, "L'Utilisateur est déjà connecté dans un autre client\n");
                            memenom = true;
                            break;
                          }
                        }


                        if (!memenom)
                        {
                          if (position == 0)
                          {
                            strcpy(reponse.texte, "L'Utilisateur inexistant!\n");
                            strcpy(reponse.data1, "KO");
                          }
                          else if (!verifieMotDePasse(position, m.texte))
                          {
                            strcpy(reponse.data1, "KO");
                            strcpy(reponse.texte, "Mot de passe incorrecte!\n");
                          }
                          else
                          {
                            int i = 0;
                            while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                            pos = i;

                            strcpy(tab->connexions[pos].nom, m.data2);
                            strcpy(reponse.data1, "OK");
                            strcpy(reponse.texte, "Vous êtes connectés!\n");
                            i = 0;
                            while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                            tab->connexions[i].nom[0] = '\0';
                            strcpy(tab->connexions[i].nom, m.data2);

                            for (int k = 0; k < 6;k++)
                            {
                              if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                              {
                                MESSAGE add;
                                add.type = tab->connexions[k].pidFenetre;
                                add.expediteur = getpid();
                                add.requete = ADD_USER;
                                strcpy(add.data1, m.data2); // Nouveau nom
                                msgsnd(idQ, &add, sizeof(MESSAGE) - sizeof(long), 0);
                                kill(tab->connexions[k].pidFenetre, SIGUSR1);
                              }
                            }
                            for (int k = 0;k<6;k++)
                            {
                              if (tab->connexions[k].pidFenetre != 0; tab->connexions[k].pidFenetre != m.expediteur && strlen(tab->connexions[k].nom) > 0)
                              {
                                MESSAGE add;
                                add.type = m.expediteur;
                                add.expediteur = getpid();
                                add.requete = ADD_USER;
                                strcpy(add.data1, tab->connexions[k].nom);
                                msgsnd(idQ, &add, sizeof(MESSAGE) - sizeof(long), 0);
                                kill(m.expediteur, SIGUSR1);
                              }
                            }
                          
                          }
                        }
                      }

                      
                      reponse.type = m.expediteur;
                      reponse.expediteur = getpid();
                      reponse.requete = LOGIN;
                      strcpy(reponse.data2, m.data2);
                      if (msgsnd(idQ, &reponse, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                        perror("(CLIENT) Envoi du login");
                        msgctl(idQ, IPC_RMID, NULL);
                        exit(1);
                      }
                      kill(m.expediteur, SIGUSR1);
                      break; 

      case LOGOUT :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      i = 0;
                      while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                      strcpy(deconnecte, tab->connexions[i].nom);
                      strcpy(tab->connexions[i].nom, "");

                      for (int k = 0;k<6;k++)
                      {
                        if (tab->connexions[k].pidFenetre != 0 && tab->connexions[k].pidFenetre != m.expediteur)
                        {
                          MESSAGE notif;
                          notif.type = tab->connexions[k].pidFenetre;
                          notif.expediteur = getpid();
                          notif.requete = REMOVE_USER;
                          strcpy(notif.data1, deconnecte);
                          if (msgsnd(idQ, &notif, sizeof(MESSAGE)- sizeof(long),0) == -1)
                          {
                            perror("(SERVEUR) Erreur d'envoi REMOVE_USER");
                          }
                          kill(tab->connexions[k].pidFenetre, SIGUSR1);
                        }
                        for (int j = 0;j<5;j++)
                        {
                          if (tab->connexions[k].autres[j] != 0)
                          {
                            tab->connexions[k].autres[j] = 0;
                          }
                        }
                      }
                      break;

      case ACCEPT_USER :  
                      fprintf(stderr,"(SERVEUR %d) Requete ACCEPT_USER reçue de %d\n",getpid(),m.expediteur);
                      MESSAGE r;
                      int i, j;

                      for (i = 0;i<6;i++)
                      {
                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                        {
                          r.expediteur = tab->connexions[i].pidFenetre;
                          break;
                        }
                      }

                      if (strlen(m.data1) > 0)
                      {
                        for (i = 0; i < 6;i++)
                        {
                          if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur)
                          {
                            for (j = 0;j<5;j++)
                            {
                              if (tab->connexions[i].autres[j] == 0)
                              {
                                tab->connexions[i].autres[j] = r.expediteur;
                                fprintf(stderr,"(SERVEUR) PID %d ajouté à tab->connexions[%d].autres[%d]\n",
                                              m.expediteur, i, j);
                                break;
                              }
                            }
                          }
                        }
                      }

                      break;

      case REFUSE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete REFUSE_USER reçue de %d\n",getpid(),m.expediteur);
                      

                      for (i = 0; i < 6; i++)
                      {
                        if (strcmp(tab->connexions[i].nom, m.data1) == 0)
                        {
                          r.expediteur = tab->connexions[i].pidFenetre;
                          break;
                        }
                      }

                      for (i = 0;i<6;i++)
                      {
                        if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          for (j = 0;j<5;j++)
                          {
                            if (tab->connexions[i].autres[j] == r.expediteur)
                            {
                              tab->connexions[i].autres[j] = 0;
                              break;
                            }
                          }
                        }
                      }

                      break;

      case SEND :  
                      fprintf(stderr,"(SERVEUR %d) Requete SEND reçue de %d\n",getpid(),m.expediteur);
                      MESSAGE tmp;
                      for (i = 0; i < 6;i++)
                      {
                        if (tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre == m.expediteur)
                        {
                          for (j = 0; j < 5; j++)
                          {
                            m.type = tab->connexions[i].autres[j];

                            if (m.type != 0)
                            {
                              temp = 1;

                              for (k = 0;k<6;k++)
                              {
                                if (tab->connexions[k].pidFenetre == m.type)
                                {
                                  strcpy(m.data1, tab->connexions[i].nom);

                                  m.requete = SEND;

                                  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                                  {
                                    perror("(CLIENT) Erreur d'envoie de la requete SEND");
                                    exit(1);
                                  }
                                  kill(m.type, SIGUSR1);
                                  break;
                                }
                              }
                            }
                          }
                          
                          break;
                        }
                      }

                      if (temp == 1)
                      {
                        tmp = m;
                        tmp.requete = SEND;
                        tmp.type = m.expediteur;
                        if (msgsnd(idQ, &tmp, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        {
                          perror("(CLIENT) Erreur d'envoie de la requete SEND");
                          exit(1);
                        }
                        kill(tmp.type, SIGUSR1);
                      }

                      break; 

      case UPDATE_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete UPDATE_PUB reçue de %d\n",getpid(),m.expediteur);
                      
                      for (int i=0;i<6;i++)
                      {
                        if (tab->connexions[i].pidFenetre != 0)
                        {
                          kill(tab->connexions[i].pidFenetre, SIGUSR2);
                        }
                      }

                      break;

      case CONSULT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      
                      idFilsConsult = fork();
                      if(idFilsConsult == 0)
                      {
                        execl("./Consultation", "Consultation", NULL);
                        perror("execl Consultation");
                        exit(1);
                      }

                      m.type = idFilsConsult;
                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                          perror("(SERVEUR) Erreur envoi CONSULT");
                          exit(1);
                      }

                      break;

      case MODIF1 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",getpid(),m.expediteur);
                      i = 0;

                      while (i < 6 && tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre != m.expediteur)
                        i++;

                      if (i == 6)
                        break;
                      if (tab->connexions[i].pidModification != 0)
                      {
                        fprintf(stderr, "(SERVEUR) Modif1 ignorée: déjà en cours\n");
                        break;
                      }

                      pidmodif = fork();
                      m.type = pidmodif;
                      if (pidmodif == 0)
                      {
                        execl("./Modification", "Modification", NULL);
                        perror("execl Modification");
                        exit(1);
                      }

                      tab->connexions[i].pidModification = pidmodif;
                      strcpy(m.data1, tab->connexions[i].nom);

                      // Modification
                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                        perror("(SERVEUR) Erreur de send Modif1");
                        exit(1);
                      }

                      break;

      case MODIF2 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",getpid(),m.expediteur);
                      
                      i = 0;

                      while (i < 6 && tab->connexions[i].pidFenetre != 0 && tab->connexions[i].pidFenetre != m.expediteur)
                        i++;

                      if (i == 6)
                        break;

                      pidmodif = tab->connexions[i].pidModification;
                      if (pidmodif == 0)
                      {
                        fprintf(stderr, "(SERVEUR) MODIF2 sans MODIF1");
                        break;
                      }

                      m.type = pidmodif;
                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                        perror("(SERVEUR) Erreur de send MODIF2");
                        exit(1);
                      }

                      break;

      case LOGIN_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN_ADMIN reçue de %d\n",getpid(),m.expediteur);

                      if (tab->pidAdmin == 0)
                      {
                        tab->pidAdmin = m.expediteur;
                        strcpy(m.data1, "OK");
                        m.type = m.expediteur;
                        m.expediteur = getpid();

                        if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        {
                          perror("(SERVEUR) Erreur de send LOGIN_ADMIN");
                          exit(1);
                        }
                      }
                      else
                      {
                        strcpy(m.data1, "KO");
                        m.type = m.expediteur;
                        m.expediteur = getpid();

                        if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                        {
                          perror("(SERVEUR) Erreur de send LOGIN_ADMIN");
                          exit(1);
                        }
                      }
                      
                      break;

      case LOGOUT_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      
                      if (m.expediteur == tab->pidAdmin)
                      {
                        tab->pidAdmin = 0;
                      }

                      break;

      case NEW_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_USER reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      
                      position = estPresent(m.data1);

                      if (position == 0)
                      {
                        ajouteUtilisateur(m.data1, m.data2);
                        sprintf(requete, 
                          "INSERT INTO UNIX_FINAL values (NULL, '%s', '%s', '%s'",
                                                             m.data1, "---", "---");
                        mysql_query(connexion, requete);
                        strcpy(m.data1, "OK");
                        strcpy(m.texte, "Utilisateur ajouté a la base de données");
                        m.type = m.expediteur;
                        m.expediteur = getpid();
                      }
                      else
                      {
                        strcpy(m.data1, "KO");
                        strcpy(m.texte, "Utilisateur déjà présent dans la base de données");
                        m.type = m.expediteur;
                        m.expediteur = getpid();
                      }

                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                        perror("(SERVER) Erreur d'envoie requete NEW_USER");
                        exit(1);
                      }

                      break;

      case DELETE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete DELETE_USER reçue de %d : --%s--\n",getpid(),m.expediteur,m.data1);
                      
                      position = estPresent(m.data1);
                      if (position == 0)
                      {
                        strcpy(m.data1, "KO");
                        strcpy(m.texte, "L'Utilisateur n'est pas présent dans le fichier utilisateurs.dat");
                        m.type = m.expediteur;
                        m.expediteur = getpid();
                      }
                      else
                      {
                        supprimerUtilisateur(m.data1);

                        sprintf(requete, "DELETE FROM UNIX_FINAL where nom = '%s'", m.data1);
                        mysql_query(connexion, requete);
                        m.type = m.expediteur;
                        m.expediteur = getpid();

                        strcpy(m.data1, "OK");
                        strcpy(m.texte, "L'Utilisateur a bien été supprimé du fichier et de la base de données");
                      }

                      if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
                      {
                        perror("(ADMIN) Erreur d'envoie de la requete LOGOUT_ADMIN");
                        exit(1);
                      }

                      break;

      case NEW_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
                      
                      int fd = open("publicites.dat", O_APPEND | O_WRONLY | O_CREAT, 0644);
                      if (fd == -1)
                      {
                        perror("Erreur d'ouverture du fichier");
                        exit(1);
                      }

                      PUBLICITE p;
                      strcpy(p.texte, m.texte);
                      p.nbSecondes = atoi(m.data1);

                      if (write(fd, &p, sizeof(PUBLICITE)) != sizeof(PUBLICITE))
                      {
                        perror("Erreur de write");
                        exit(1);
                      }
                      close(fd);

                      if (tab->pidPublicite > 0)
                        kill(tab->pidPublicite, SIGUSR1);


                      break;
    }
    afficheTab();
  }
}

void afficheTab()
{
  fprintf(stderr,"Pid Serveur 1 : %d\n",tab->pidServeur1);
  fprintf(stderr,"Pid Serveur 2 : %d\n",tab->pidServeur2);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid Admin     : %d\n",tab->pidAdmin);
  for (int i=0 ; i<6 ; i++)
    fprintf(stderr,"%6d -%20s- %6d %6d %6d %6d %6d - %6d\n",tab->connexions[i].pidFenetre,
                                                      tab->connexions[i].nom,
                                                      tab->connexions[i].autres[0],
                                                      tab->connexions[i].autres[1],
                                                      tab->connexions[i].autres[2],
                                                      tab->connexions[i].autres[3],
                                                      tab->connexions[i].autres[4],
                                                      tab->connexions[i].pidModification);
  fprintf(stderr,"\n");
}

void HandlerSIGCHLD(int sig)
{
  int status, pid, i = 0;
  pid = wait(&status);
  fprintf(stderr, "(SERVEUR) Suppression du fils zombi %d\n", pid);
  while (i<6 && tab->connexions[i].pidModification != pid) i++;
  if (tab->connexions[i].pidModification == pid)
    tab->connexions[i].pidModification = 0;
  // siglongjmp(jumpBuffer, 1);
}

void HandlerSIGINT(int sig)
{
  fprintf(stderr, "\n(SERVEUR) CTRL C activé, on ferme tout les processus 0-6\n");

  if (msgctl(idQ, IPC_RMID, NULL) == -1)
  {
    fprintf(stderr, "(SERVEUR) Le serveur n'a pas bien été fermé\n");
  }
  else
    fprintf(stderr, "(SERVEUR) Le serveur a bien été fermé\n");

  if (shmctl(idShm, IPC_RMID, NULL) == -1)
  {
    perror("Erreur de la fermeture de la mémoire partagée\n");
    exit(1);
  }

  if (semctl(idSem, IPC_RMID, NULL) == -1)
  {
    perror("Erreur de la fermeture du sémaphore\n");
    exit(1);
  }

  kill(idFilsPub, SIGKILL);
  fprintf(stderr, "(SERVEUR) Le serveur a bien été fermé\n");

  if (close(fdPipe[0]) == -1)
  {
    perror("Erreur lors de la fermeture de la sortie du pipe\n");
    exit(0);
  }
  fprintf(stderr, "(SERVEUR) La sortie du pipe a bien été fermée\n");

  if (close(fdPipe[1]) == -1)
  {
    perror("Erreur lors de la fermeture de l'entrée du pipe\n");
    exit(0);
  }
  fprintf(stderr, "(SERVEUR) l'entrée du pipe a bien été fermée\n");

  kill(idFilsAccesBD, SIGKILL);
  exit(0);
}