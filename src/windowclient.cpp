#include "windowclient.h"
#include "ui_windowclient.h"
#include <QMessageBox>
#include "dialogmodification.h"
#include <unistd.h>

extern WindowClient *w;

#include "protocole.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <cstring>

int idQ, idShm;
#define TIME_OUT 120
int timeOut = TIME_OUT;
int SERVEUR = 1;
char* pShm;

void handlerSIGUSR1(int sig);
void handlerSIGUSR2(int sig);
void handlerSIGALRM(int sig);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
WindowClient::WindowClient(QWidget *parent):QMainWindow(parent),ui(new Ui::WindowClient)
{
    ui->setupUi(this);
    ::close(2);
    logoutOK();

    // Recuperation de l'identifiant de la file de messages
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la file de messages\n",getpid());
    idQ = msgget(CLE, IPC_CREAT | 0666);
    if (idQ == -1)
    {
      perror("(CLIENT) Erreur msgget");
      exit(EXIT_FAILURE);
    }

    // Recuperation de l'identifiant de la mémoire partagée
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la mémoire partagée\n",getpid());
    if ((idShm = shmget(CLE, 0, 0)) == -1)
    {
      perror("(PUBLICITE) Erreur lors de la récupération de l'id de la mémoire partagée");
      exit(1);
    }

    // Attachement à la mémoire partagée
    if ((pShm = (char *)shmat(idShm, NULL, 0)) == (char *) - 1)
    {
      perror("(PUBLICITE) Erreur lors de l'attachement de la mémoire partagée");
      exit(1);
    }

    // Armement des signaux
    struct sigaction A;
    A.sa_handler = handlerSIGUSR1;
    sigemptyset(&A.sa_mask);
    A.sa_flags = 0;
    if (sigaction(SIGUSR1, &A, NULL) == -1)
    {
      perror("Erreur d'armement de SIGUSR1");
      exit(1);
    }

    struct sigaction B;
    B.sa_handler = handlerSIGUSR2;
    sigemptyset(&B.sa_mask);
    B.sa_flags = 0;
    if (sigaction(SIGUSR2, &B, NULL) == -1)
    {
      perror("Erreur d'armement de SIGUSR2");
      exit(1);
    }

    struct sigaction C;

    C.sa_handler = handlerSIGALRM;
    sigemptyset(&C.sa_mask);
    C.sa_flags = 0;

    if (sigaction((SIGALRM), &C, NULL) == -1)
    {
      perror("Erreur d'armement de SIGALRM");
      exit(1);
    }

    // Envoi d'une requete de connexion au serveur

    MESSAGE connexion;
    connexion.type = SERVEUR;
    connexion.requete = CONNECT;
    connexion.expediteur = getpid();

    fprintf(stderr, "(CLIENT %d) Requete CONNECT envoyée\n", getpid());
    if (msgsnd(idQ, &connexion, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
      perror("(CLIENT) Erreur de send");
      msgctl(idQ, IPC_RMID, NULL);  // Suppression si erreur
      exit(1);
    }
}

WindowClient::~WindowClient()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNom(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNom->clear();
    return;
  }
  ui->lineEditNom->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNom()
{
  strcpy(connectes[0],ui->lineEditNom->text().toStdString().c_str());
  return connectes[0];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setMotDePasse(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditMotDePasse->clear();
    return;
  }
  ui->lineEditMotDePasse->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getMotDePasse()
{
  strcpy(motDePasse,ui->lineEditMotDePasse->text().toStdString().c_str());
  return motDePasse;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WindowClient::isNouveauChecked()
{
  if (ui->checkBoxNouveau->isChecked()) return 1;
  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPublicite(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditPublicite->clear();
    return;
  }
  ui->lineEditPublicite->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setTimeOut(int nb)
{
  ui->lcdNumberTimeOut->display(nb);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setAEnvoyer(const char* Text)
{
  //fprintf(stderr,"---%s---\n",Text);
  if (strlen(Text) == 0 )
  {
    ui->lineEditAEnvoyer->clear();
    return;
  }
  ui->lineEditAEnvoyer->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getAEnvoyer()
{
  strcpy(aEnvoyer,ui->lineEditAEnvoyer->text().toStdString().c_str());
  return aEnvoyer;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPersonneConnectee(int i,const char* Text)
{
  if (strlen(Text) == 0 )
  {
    switch(i)
    {
        case 1 : ui->lineEditConnecte1->clear(); break;
        case 2 : ui->lineEditConnecte2->clear(); break;
        case 3 : ui->lineEditConnecte3->clear(); break;
        case 4 : ui->lineEditConnecte4->clear(); break;
        case 5 : ui->lineEditConnecte5->clear(); break;
    }
    return;
  }
  switch(i)
  {
      case 1 : ui->lineEditConnecte1->setText(Text); break;
      case 2 : ui->lineEditConnecte2->setText(Text); break;
      case 3 : ui->lineEditConnecte3->setText(Text); break;
      case 4 : ui->lineEditConnecte4->setText(Text); break;
      case 5 : ui->lineEditConnecte5->setText(Text); break;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getPersonneConnectee(int i)
{
  QLineEdit *tmp;
  switch(i)
  {
    case 1 : tmp = ui->lineEditConnecte1; break;
    case 2 : tmp = ui->lineEditConnecte2; break;
    case 3 : tmp = ui->lineEditConnecte3; break;
    case 4 : tmp = ui->lineEditConnecte4; break;
    case 5 : tmp = ui->lineEditConnecte5; break;
    default : return NULL;
  }

  strcpy(connectes[i],tmp->text().toStdString().c_str());
  return connectes[i];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::ajouteMessage(const char* personne,const char* message)
{
  // Choix de la couleur en fonction de la position
  int i=1;
  bool trouve=false;
  while (i<=5 && !trouve)
  {
      if (getPersonneConnectee(i) != NULL && strcmp(getPersonneConnectee(i),personne) == 0) trouve = true;
      else i++;
  }
  char couleur[40];
  if (trouve)
  {
      switch(i)
      {
        case 1 : strcpy(couleur,"<font color=\"red\">"); break;
        case 2 : strcpy(couleur,"<font color=\"blue\">"); break;
        case 3 : strcpy(couleur,"<font color=\"green\">"); break;
        case 4 : strcpy(couleur,"<font color=\"darkcyan\">"); break;
        case 5 : strcpy(couleur,"<font color=\"orange\">"); break;
      }
  }
  else strcpy(couleur,"<font color=\"black\">");
  if (strcmp(getNom(),personne) == 0) strcpy(couleur,"<font color=\"purple\">");

  // ajout du message dans la conversation
  char buffer[300];
  sprintf(buffer,"%s(%s)</font> %s",couleur,personne,message);
  ui->textEditConversations->append(buffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNomRenseignements(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNomRenseignements->clear();
    return;
  }
  ui->lineEditNomRenseignements->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNomRenseignements()
{
  strcpy(nomR,ui->lineEditNomRenseignements->text().toStdString().c_str());
  return nomR;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setGsm(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditGsm->clear();
    return;
  }
  ui->lineEditGsm->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setEmail(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditEmail->clear();
    return;
  }
  ui->lineEditEmail->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setCheckbox(int i,bool b)
{
  QCheckBox *tmp;
  switch(i)
  {
    case 1 : tmp = ui->checkBox1; break;
    case 2 : tmp = ui->checkBox2; break;
    case 3 : tmp = ui->checkBox3; break;
    case 4 : tmp = ui->checkBox4; break;
    case 5 : tmp = ui->checkBox5; break;
    default : return;
  }
  tmp->setChecked(b);
  if (b) tmp->setText("Accepté");
  else tmp->setText("Refusé");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::loginOK()
{
  ui->pushButtonLogin->setEnabled(false);
  ui->pushButtonLogout->setEnabled(true);
  ui->lineEditNom->setReadOnly(true);
  ui->lineEditMotDePasse->setReadOnly(true);
  ui->checkBoxNouveau->setEnabled(false);
  ui->pushButtonEnvoyer->setEnabled(true);
  ui->pushButtonConsulter->setEnabled(true);
  ui->pushButtonModifier->setEnabled(true);
  ui->checkBox1->setEnabled(true);
  ui->checkBox2->setEnabled(true);
  ui->checkBox3->setEnabled(true);
  ui->checkBox4->setEnabled(true);
  ui->checkBox5->setEnabled(true);
  ui->lineEditAEnvoyer->setEnabled(true);
  ui->lineEditNomRenseignements->setEnabled(true);
  setTimeOut(TIME_OUT);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::logoutOK()
{
  ui->pushButtonLogin->setEnabled(true);
  ui->pushButtonLogout->setEnabled(false);
  ui->lineEditNom->setReadOnly(false);
  ui->lineEditNom->setText("");
  ui->lineEditMotDePasse->setReadOnly(false);
  ui->lineEditMotDePasse->setText("");
  ui->checkBoxNouveau->setEnabled(true);
  ui->pushButtonEnvoyer->setEnabled(false);
  ui->pushButtonConsulter->setEnabled(false);
  ui->pushButtonModifier->setEnabled(false);
  for (int i=1 ; i<=5 ; i++)
  {
      setCheckbox(i,false);
      setPersonneConnectee(i,"");
  }
  ui->checkBox1->setEnabled(false);
  ui->checkBox2->setEnabled(false);
  ui->checkBox3->setEnabled(false);
  ui->checkBox4->setEnabled(false);
  ui->checkBox5->setEnabled(false);
  setNomRenseignements("");
  setGsm("");
  setEmail("");
  ui->textEditConversations->clear();
  setAEnvoyer("");
  ui->lineEditAEnvoyer->setEnabled(false);
  ui->lineEditNomRenseignements->setEnabled(false);
  setTimeOut(TIME_OUT);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions permettant d'afficher des boites de dialogue /////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueMessage(const char* titre,const char* message)
{
   QMessageBox::information(this,titre,message);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueErreur(const char* titre,const char* message)
{
   QMessageBox::critical(this,titre,message);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Clic sur la croix de la fenêtre ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::closeEvent(QCloseEvent *event)
{
    // TO DO
    fprintf(stderr, "Arret du programme..");
    MESSAGE l;
    l.type = 1;
    l.expediteur = getpid();
    l.requete = LOGOUT;

    if (msgsnd(idQ, &l, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
      perror("(CLIENT) Erreur de send Deconnexion");
      msgctl(idQ, IPC_RMID, NULL);
      exit(1);
    }

    MESSAGE m;
    m.type = 1;
    m.expediteur = getpid();
    m.requete = DECONNECT;

    if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
      perror("(CLIENT) Erreur de send Deconnexion");
      msgctl(idQ, IPC_RMID, NULL);
      exit(1);
    }

    QApplication::exit();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonLogin_clicked()
{
    MESSAGE login;
    int nvch = isNouveauChecked();
    login.type = SERVEUR;
    login.expediteur = getpid();
    login.requete = LOGIN;
    strcpy(login.data2, getNom());
    strcpy(login.texte, getMotDePasse());

    if (nvch == 1)
      strcpy(login.data1, "1");
    else
      strcpy(login.data1, "0");

    if (msgsnd(idQ, &login, sizeof(MESSAGE) - sizeof(long), 0) == -1)
    {
      perror("(CLIENT) Erreur de send Login");
      msgctl(idQ, IPC_RMID, NULL);
      exit(1);
    }
    fprintf(stderr, "(CLIENT %d) Requete login envoyée\n", getpid());

}

void WindowClient::on_pushButtonLogout_clicked()
{
  // TO DO
  alarm(0);                         // Deconnexion
  MESSAGE logout;
  logout.type = SERVEUR;
  logout.expediteur = getpid();
  logout.requete = LOGOUT;
  if (msgsnd(idQ, &logout, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) Erreur de send Logout");
    msgctl(idQ, IPC_RMID, NULL);
    exit(1);
  }
  fprintf(stderr, "Deconnection du pid : %d, Utilisateur: %s", getpid(), getNom());
  logoutOK();
}

void WindowClient::on_pushButtonEnvoyer_clicked()
{
  // TO DO
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);

  MESSAGE snd;

  snd.type = SERVEUR;
  snd.requete = SEND;
  snd.expediteur = getpid();

  if (strlen(getAEnvoyer()) == 0)
  {
    dialogueErreur("Erreur Message", "Veuillez ecrire quelque chose");
    return;
  }

  strcpy(snd.texte, getAEnvoyer());
  if (msgsnd(idQ, &snd, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) Erreur de send message");
    msgctl(idQ, IPC_RMID, NULL);
    exit(1);
  }

  fprintf(stderr, "%d a envoyé un message", getpid());
  setAEnvoyer("");
}

void WindowClient::on_pushButtonConsulter_clicked()
{
  alarm(0);
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  const char* nomRens = getNomRenseignements();
  if (nomRens == nullptr || strlen(nomRens) == 0) {
      fprintf(stderr, "(CLIENT) Aucun nom renseigné pour CONSULT\n");
      return;
  }

  MESSAGE r;
  strcpy(r.data1, nomRens);

  r.type = SERVEUR;
  r.requete = CONSULT;
  r.expediteur = getpid();

  fprintf(stderr, "(CLIENT %d) Requete CONSULT envoyée au Serveur\n", r.expediteur);

  if (msgsnd(idQ, &r, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) Erreur d'envoie requete CONSULT");
    msgctl(idQ, IPC_RMID, NULL);
    exit(1);
  }

}

void WindowClient::on_pushButtonModifier_clicked()
{
  // TO DO
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  // Envoi d'une requete MODIF1 au serveur
  MESSAGE m;

  m.type = SERVEUR;
  m.requete = MODIF1;
  m.expediteur = getpid();

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) Erreur d'envoie requete CONSULT");
    msgctl(idQ, IPC_RMID, NULL);
    exit(1);
  }

  // Attente d'une reponse en provenance de Modification
  fprintf(stderr,"(CLIENT %d) Attente reponse MODIF1\n",getpid());

  if (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(), 0) == -1)
  {
    perror("(CLIENT) Erreur de receive requete MODIF1");
    exit(1);
  }

  // Verification si la modification est possible
  if (strcmp(m.data1,"KO") == 0 && strcmp(m.data2,"KO") == 0 && strcmp(m.texte,"KO") == 0)
  {
    QMessageBox::critical(w,"Problème...","Modification déjà en cours...");
    return;
  }

  // Modification des données par utilisateur
  DialogModification dialogue(this,getNom(),"",m.data2,m.texte);
  dialogue.exec();
  char motDePasse[40];
  char gsm[40];
  char email[40];
  strcpy(motDePasse,dialogue.getMotDePasse());
  strcpy(gsm,dialogue.getGsm());
  strcpy(email,dialogue.getEmail());

  // Envoi des données modifiées au serveur

  strcpy(m.data2, gsm);
  strcpy(m.texte, email);
  strcpy(m.data1, motDePasse);

  m.type = SERVEUR;
  m.expediteur = getpid();
  m.requete = MODIF2;

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) Erreur de send MODIF2");
    exit(1);
  }
  
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les checkbox ///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_checkBox1_clicked(bool checked)
{
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  MESSAGE m;
  if (checked)
  {
      ui->checkBox1->setText("Accepté");
      m.type = SERVEUR;
      m.requete = ACCEPT_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(1));
  }
  else
  {
      ui->checkBox1->setText("Refusé");
      m.type = SERVEUR;
      m.requete = REFUSE_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(1));
  }

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) error d'envoi ACCEPT");
    exit(1);
  }
}

void WindowClient::on_checkBox2_clicked(bool checked)
{
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  MESSAGE m;
  if (checked)
  {
      ui->checkBox2->setText("Accepté");
      m.type = SERVEUR;
      m.requete = ACCEPT_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(2));
  }
  else
  {
      ui->checkBox2->setText("Refusé");
      m.type = SERVEUR;
      m.requete = REFUSE_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(2));
  }

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) error d'envoi ACCEPT");
    exit(1);
  }
}

void WindowClient::on_checkBox3_clicked(bool checked)
{
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  MESSAGE m;
  if (checked)
  {
      ui->checkBox3->setText("Accepté");
      m.type = SERVEUR;
      m.requete = ACCEPT_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(3));
  }
  else
  {
      ui->checkBox3->setText("Refusé");
      m.type = SERVEUR;
      m.requete = REFUSE_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(3));
  }

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) error d'envoi ACCEPT");
    exit(1);
  }
}

void WindowClient::on_checkBox4_clicked(bool checked)
{
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  MESSAGE m;
  if (checked)
  {
      ui->checkBox4->setText("Accepté");
      m.type = SERVEUR;
      m.requete = ACCEPT_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(4));
  }
  else
  {
      ui->checkBox4->setText("Refusé");
      m.type = SERVEUR;
      m.requete = REFUSE_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(4));
  }

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) error d'envoi ACCEPT");
    exit(1);
  }
}

void WindowClient::on_checkBox5_clicked(bool checked)
{
  timeOut = TIME_OUT;
  setTimeOut(timeOut);
  alarm(1);
  MESSAGE m;
  if (checked)
  {
      ui->checkBox5->setText("Accepté");
      m.type = SERVEUR;
      m.requete = ACCEPT_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(5));
  }
  else
  {
      ui->checkBox5->setText("Refusé");
      m.type = SERVEUR;
      m.requete = REFUSE_USER;
      m.expediteur = getpid();
      strcpy(m.data1, getPersonneConnectee(5));
  }

  if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long), 0) == -1)
  {
    perror("(CLIENT) error d'envoi ACCEPT");
    exit(1);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Handlers de signaux ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGUSR1(int sig)
{
    MESSAGE m, login, r;
    const char* nv;
    const char* p;
    int i;    

    while (msgrcv(idQ, &m, sizeof(MESSAGE) - sizeof(long), getpid(),IPC_NOWAIT) != -1)
    {
      fprintf(stderr, " ---------------------\n");
      fprintf(stderr, "Requete Recu : \n");
      fprintf(stderr, " - Type de requète : %d \n", m.requete);
      fprintf(stderr, " - Expediteur      : %d \n", m.expediteur); // pid
      fprintf(stderr, " - Destinataire      : %ld \n", m.type);
      fprintf(stderr, " - data1           : %s \n", m.data1); // 1 ou 0
      fprintf(stderr, " - data2           : %s \n", m.data2); // nom
      switch(m.requete)
      {
        case LOGIN :
                    if (strcmp(m.data1,"1") == 0)
                    {
                      timeOut = TIME_OUT;
                      w->setTimeOut(timeOut);
                      alarm(1);
                      fprintf(stderr,"(CLIENT %d) Login OK\n",getpid());
                      w->loginOK();
                      w->dialogueMessage("Login...",m.texte);
                      
                    }
                    else w->dialogueMessage("Login...",m.texte);
                    break;

        case ADD_USER :
                    nv = m.data1;
                    for (i = 1; i <= 5; i++)
                    {
                      p = w->getPersonneConnectee(i);
                      if (strlen(p) == 0)
                        break;
                    }

                    if (i <= 5)
                    {
                       w->setPersonneConnectee(i, nv);
                       w->ajouteMessage(nv, "s'est connecté");
                    }

                    break;

        case REMOVE_USER :
                    nv = m.data1;
                    for (i = 1; i <=5;i++)
                    {
                      p = w->getPersonneConnectee(i);
                      if (strcmp(p, nv) == 0)
                      {
                        w->setPersonneConnectee(i, "");
                        w->ajouteMessage(nv, "s'est déconnecté");
                        break;
                      }
                    }
                    break;

        case SEND :
                    w->ajouteMessage(m.data1, m.texte);
                    break;

        case CONSULT :
                    fprintf(stderr, "(CLIENT) Consult reçu\n");
                    if (strcmp(m.data1,"OK") == 0)
                    {
                      w->setGsm(m.data2);
                      w->setEmail(m.texte);
                    }
                    else
                    {
                      w->setGsm("NON TROUVE");
                      w->setEmail("NON TROUVE");
                    }
                  break;
      }
    }
}

void handlerSIGUSR2(int sig)
{
  w->setPublicite(pShm);
}

void handlerSIGALRM(int sig)
{
  if (timeOut > 0) timeOut--;
  w->setTimeOut(timeOut);
  if (timeOut == 0)
  {
    MESSAGE m;
    m.requete = LOGOUT;
    m.expediteur = getpid();
    m.type = SERVEUR;
    if (msgsnd(idQ, &m, sizeof(MESSAGE) - sizeof(long),0) == -1)
    {
      perror("(CLIENT) Erreur envoi requete LOGOUT (TIMEOUT)");
      exit(1);
    }
    w->logoutOK();
    alarm(0);
    return;
  }
  alarm(1);
}