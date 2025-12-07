# SRC: .cpp | INC: .h | OBJ: .o | BIN: .exe
SRC = ./src
INC = ./include
COMP = g++ -Wall -D DEBUG -I $(INC) -I $(SRC)

all: Publicite Modification CreationBD BidonFichierPub Consultation Serveur Administrateur Client

FichierUtilisateur.o: $(SRC)/FichierUtilisateur.cpp $(INC)/FichierUtilisateur.h
	$(COMP) -c $(SRC)/FichierUtilisateur.cpp -o FichierUtilisateur.o

mainAdmin.o: $(SRC)/mainAdmin.cpp
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../Administrateur -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o mainAdmin.o $(SRC)/mainAdmin.cpp

windowadmin.o: $(SRC)/windowadmin.cpp $(INC)/protocole.h $(INC)/windowadmin.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../Administrateur -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o windowadmin.o $(SRC)/windowadmin.cpp

moc_windowadmin.o: $(SRC)/moc_windowadmin.cpp $(INC)/windowadmin.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../Administrateur -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o moc_windowadmin.o $(SRC)/moc_windowadmin.cpp

Administrateur: mainAdmin.o windowadmin.o moc_windowadmin.o
	$(COMP)  -o Administrateur mainAdmin.o windowadmin.o moc_windowadmin.o   /usr/lib64/libQt5Widgets.so /usr/lib64/libQt5Gui.so /usr/lib64/libQt5Core.so /usr/lib64/libGL.so -lpthread

dialogmodification.o: $(SRC)/dialogmodification.cpp $(INC)/dialogmodification.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../UNIX_DOSSIER_FINAL -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o dialogmodification.o $(SRC)/dialogmodification.cpp

mainClient.o: $(SRC)/mainClient.cpp $(INC)/windowclient.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../UNIX_DOSSIER_FINAL -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o mainClient.o $(SRC)/mainClient.cpp

windowclient.o: $(SRC)/windowclient.cpp $(INC)/windowclient.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../UNIX_DOSSIER_FINAL -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o windowclient.o $(SRC)/windowclient.cpp

moc_dialogmodification.o: $(SRC)/moc_dialogmodification.cpp $(INC)/dialogmodification.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../UNIX_DOSSIER_FINAL -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o moc_dialogmodification.o $(SRC)/moc_dialogmodification.cpp

moc_windowclient.o: $(SRC)/moc_windowclient.cpp $(INC)/windowclient.h
	$(COMP) -c -pipe -g -std=gnu++11 -Wall -W -D_REENTRANT -fPIC -DQT_DEPRECATED_WARNINGS -DQT_QML_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -I../UNIX_DOSSIER_FINAL -I. -isystem /usr/include/qt5 -isystem /usr/include/qt5/QtWidgets -isystem /usr/include/qt5/QtGui -isystem /usr/include/qt5/QtCore -I. -I. -I/usr/lib64/qt5/mkspecs/linux-g++ -o moc_windowclient.o $(SRC)/moc_windowclient.cpp

Client: dialogmodification.o mainClient.o windowclient.o moc_dialogmodification.o moc_windowclient.o FichierUtilisateur.o
	$(COMP)  -o Client dialogmodification.o mainClient.o windowclient.o moc_dialogmodification.o moc_windowclient.o FichierUtilisateur.o   /usr/lib64/libQt5Widgets.so /usr/lib64/libQt5Gui.so /usr/lib64/libQt5Core.so /usr/lib64/libGL.so -lpthread

Serveur: $(SRC)/Serveur.cpp FichierUtilisateur.o
	$(COMP) $(SRC)/Serveur.cpp FichierUtilisateur.o -o Serveur -I/usr/include/mysql -m64 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl

CreationBD: $(SRC)/CreationBD.cpp
	$(COMP) -o CreationBD $(SRC)/CreationBD.cpp -I/usr/include/mysql -m64 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl

BidonFichierPub: $(SRC)/BidonFichierPub.cpp
	$(COMP) -o BidonFichierPub $(SRC)/BidonFichierPub.cpp

Publicite: $(SRC)/Publicite.cpp $(INC)/protocole.h
	$(COMP) -o Publicite $(SRC)/Publicite.cpp

Consultation: $(SRC)/Consultation.cpp
	$(COMP) $(SRC)/Consultation.cpp -o Consultation -I/usr/include/mysql -m64 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl

Modification: $(SRC)/Modification.cpp
	$(COMP) $(SRC)/Modification.cpp -o Modification -I/usr/include/mysql -m64 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl


# AccesBD: AccesBD.cpp
# 	$(COMP) -o AccesBD AccesBD.cpp -I/usr/include/mysql -m64 -L/usr/lib64/mysql -lmysqlclient -lpthread -lz -lm -lrt -lssl -lcrypto -ldl
clean:
	rm -f *.o

clobber: clean
	rm -f Administrateur Client Serveur CreationBD BidonFichierPub Publicite Consultation Modification
	rm -f *.o