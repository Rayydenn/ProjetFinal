#include "FichierUtilisateur.h"
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
using namespace std;

int estPresent(const char* nom)
{
  UTILISATEUR temp;
  int fichier, pos = 1;
  if ((fichier = open(FICHIER_UTILISATEURS, O_RDONLY)) == -1)
    return -1;

  while (read(fichier, &temp, sizeof(UTILISATEUR)) > 0)
  {
    if (strcmp(temp.nom, nom) == 0)
    {
      close(fichier);
      cout << "Position: " << pos << endl;
      return pos;
    }
    pos++;
  }

  close(fichier);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int Hash(const char* motDePasse)
{
  int i = 0, hasher = 0;

  while (motDePasse[i] != '\0')
  {
    hasher = hasher + (i+1) * motDePasse[i];
    i++;
  }

  return hasher % 97;
}

////////////////////////////////////////////////////////////////////////////////////
void ajouteUtilisateur(const char* nom, const char* motDePasse)
{
  UTILISATEUR temp;

  int fichier;

  if ((fichier = open(FICHIER_UTILISATEURS, O_WRONLY | O_CREAT | O_APPEND, 0777)) == -1)
  {
    cerr << "Erreur pendant l'ouverture du fichier.." << endl;
    return;
  }

  strcpy(temp.nom, nom);
  temp.hash = Hash(motDePasse);
  write(fichier, &temp, sizeof(UTILISATEUR));
  close(fichier);
}

////////////////////////////////////////////////////////////////////////////////////
void supprimerUtilisateur(const char* nom)
{
  UTILISATEUR tmp;
  int fd = open(FICHIER_UTILISATEURS, O_RDONLY);

  if (fd == -1)
  {
    perror("Erreur d'ouverture du fichier");
    return;
  }

  int fdtmp = open("temp.dat", O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (fdtmp == -1)
  {
    perror("Erreur d'ouverture du fichier temp.dat");
    close(fd);
    return;
  }

  UTILISATEUR user;
  int trouve = 0;

  while (read(fd, &user, sizeof(UTILISATEUR)) == sizeof(UTILISATEUR))
  {
    if (strcmp(user.nom, nom) == 0)
    {
      trouve = 1;
      continue; // Skip
    }
    write(fdtmp, &user, sizeof(UTILISATEUR));
  }

  close(fd);
  close(fdtmp);

  if (!trouve)
  {
    unlink("temp.dat");
    return;
  }

  unlink(FICHIER_UTILISATEURS);
  rename("temp.dat", FICHIER_UTILISATEURS);

}

////////////////////////////////////////////////////////////////////////////////////
int verifieMotDePasse(int pos, const char* motDePasse)
{
  UTILISATEUR temp;

  int fichier;

  if ((fichier = open(FICHIER_UTILISATEURS, O_RDONLY)) == -1)
  {
    cerr << "Erreur pendant l'ouverture du fichier.." << endl;
    return -1;
  }
 
  lseek(fichier, (pos - 1) * sizeof(UTILISATEUR), SEEK_SET);
  read(fichier, &temp, sizeof(UTILISATEUR));
  cout << "Le nom d'utilisateur du mot de passe entré est: " << temp.nom << endl;
  close(fichier);
  if (temp.hash == Hash(motDePasse))
  {
    return 1;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////////
int listeUtilisateurs(UTILISATEUR *vecteur) // le vecteur doit etre suffisamment grand
{
  int fichier, cpt = 0;

  if ((fichier = open(FICHIER_UTILISATEURS, O_RDONLY)) == -1)
  {
    cerr << "Erreur de l'ouverture du fichier" << endl;
    return -1;
  }

  while (cpt < 50 && read(fichier, &vecteur[cpt], sizeof(UTILISATEUR)) > 0)
  {
    cpt++;
  }
  close(fichier);
  return cpt;
}
