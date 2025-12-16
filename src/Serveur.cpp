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
#include "protocole.h" // contient la cle et la structure d'un message
#include "FichierUtilisateur.h" // contient estPresent pour le login

int idQ,idShm,idSem;
TAB_CONNEXIONS *tab;
int idFilsPub, idFilsAccesBD;
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
  if ((idQ = msgget(CLE,IPC_CREAT | 0600)) == -1)  // CLE definie dans protocole.h
  {
    perror("(SERVEUR) Erreur de msgget");
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

  int i,k,j;
  MESSAGE m;
  MESSAGE reponse;
  char requete[200];
  MYSQL_RES  *resultat;
  MYSQL_ROW  tuple;
  PUBLICITE pub;

  while(1)
  {
    bool isFalse = false;
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
                      fprintf(stderr, "(SERVEUR) Trop de client");
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

                      if (strlen(m.data1) == 0 || strlen(m.texte) == 0) // si rien n'est entré
                      {
                        strcpy(reponse.data1, "0");
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
                        break;  // on sort directement du case LOGIN
                      }

                      if (strcmp(m.data1, nvclient) == 0) // Utilisateur absent
                      {
                        if (position == 0)
                        {
                          ajouteUtilisateur(m.data2, m.texte);
                          strcpy(reponse.texte, "Utilisateur ajouté\n");
                          i = 0;
                          while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur)
                            i++;
                          tab->connexions[i].nom[0] = '\0';
                          strcpy(tab->connexions[i].nom, m.data2);

                          // Notifier les utilisateurs
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
                          strcpy(reponse.data1, "0");
                        }
                      }

                      else // l'utilisateur veut se connecter
                      {
                        if (position == 0)
                        {
                          strcpy(reponse.texte, "L'Utilisateur inexistant!\n");
                          strcpy(reponse.data1, "0");
                        }
                        else if (!verifieMotDePasse(position, m.texte))
                        {
                          strcpy(reponse.data1, "0");
                          strcpy(reponse.texte, "Mot de passe incorrecte!\n");
                        }
                        else
                        {
                          int i = 0;
                          while (i < 6 && tab->connexions[i].pidFenetre != m.expediteur) i++;
                          pos = i;
                          i=0;
                          while (i < 6 && tab->connexions[i].nom != m.data2) i++;
                          if (i < 6 && strcmp(tab->connexions[i].nom, m.data2) == 0)
                            isFalse = true;

                          if (!isFalse)
                          {
                            strcpy(tab->connexions[pos].nom, m.data2);
                            strcpy(reponse.data1, "1"); // OK
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

                          else
                          {
                            strcpy(reponse.data1, "0");
                            strcpy(reponse.texte, "L'Utilisateur est deja connecté\n");
                          }
                        }
                      }

                      // Envoyer reponse LOGIN au client

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
                          notif.requete = REMOVE_USER;  // Requete pour notifier qu'un client s'est deconnecté
                          strcpy(notif.data1, deconnecte); // Nom du client qui s'est déconnecté
                          if (msgsnd(idQ, &notif, sizeof(MESSAGE)- sizeof(long),0) == -1)
                          {
                            perror("(SERVEUR) Erreur d'envoi REMOVE_USER");
                          }
                          kill(tab->connexions[k].pidFenetre, SIGUSR1); // Notifier client
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
                                  strcpy(tmp.data1, tab->connexions[i].nom);

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
                      break;

      case CONSULT :
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF1 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF1 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case MODIF2 :
                      fprintf(stderr,"(SERVEUR %d) Requete MODIF2 reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGIN_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case LOGOUT_ADMIN :
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT_ADMIN reçue de %d\n",getpid(),m.expediteur);
                      break;

      case NEW_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_USER reçue de %d : --%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2);
                      break;

      case DELETE_USER :
                      fprintf(stderr,"(SERVEUR %d) Requete DELETE_USER reçue de %d : --%s--\n",getpid(),m.expediteur,m.data1);
                      break;

      case NEW_PUB :
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);
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
  printf("(SERVEUR) SIGCHLD reçu");
  int status, pid, i = 0;
  pid = wait(&status);
  while (i<6 && tab->connexions[i].pidModification != pid) i++;
  tab->connexions[i].pidModification = 0;
  siglongjmp(jumpBuffer, 1);
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