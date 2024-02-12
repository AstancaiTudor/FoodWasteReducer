#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sqlite3.h>


#define PORT 3000
extern int errno;  /* codul de eroare returnat de anumite apeluri */

typedef struct
{
  pthread_t idThread; // id-ul thread-ului
  int nr_conexiuni;        // nr de conexiuni servite
} Thread;

Thread *multime_threads; // un array de structuri Thread

size_t dim,copie_dim;
int decod,cantitate_ceruta;
int disp;   //variabila pentru disponibilitatea mancarii
int sd;        // descriptorul de socket de ascultare
int nr_threads;          // numarul de threaduri
pthread_mutex_t lacat = PTHREAD_MUTEX_INITIALIZER; // variabila mutex ce va fi partajata de threaduri
sqlite3 *db;

void raspuns_spre_client(int cl, int idThread);
void creare_thread(int);
static void *treat(void *); // functia executata de fiecare thread ce realizeaza comunicarea cu clientii 
static void *timer(void *);  // functie pentru actualizat baza de date

int main(int argc, char *argv[])
{
  struct sockaddr_in server; // structura folosita de server 

  if (argc < 2)
  {
    fprintf(stderr, "Eroare: Primul argument este numarul de fire de executie...");
    exit(1);
  }
  nr_threads = atoi(argv[1]); //iau nr de threaduri de la terminal
  if (nr_threads <= 0)
  {
    fprintf(stderr, "Eroare: Numar de fire invalid...");
    exit(1);
  }

  multime_threads = calloc(sizeof(Thread), nr_threads); //alocarea spatiu pentru threaduri, zona de memorie initializata cu 0

  int cod = sqlite3_open_v2("database.db", &db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);   //deschiderea bazei de date
  if (cod) 
  {
    fprintf(stderr, "Eroare la deschiderea/crearea bazei de date: %s\n", sqlite3_errmsg(db));
    exit(1);
  } 
  else 
    fprintf(stdout, "Baza de date deschisa cu succes\n");

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)  // crearea unui socket 
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }

  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); //optiune care permite reutilizarea socketului imediat dupa inchiderea serverului

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;   /* stabilirea familiei de socket-uri */
  server.sin_addr.s_addr = htonl(INADDR_ANY);    /* accepta orice adresa */
  server.sin_port = htons(PORT);   /* utilizez un port utilizator */

  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) //leg socketul de adresa si portul stocate in structura server
  {
    perror("[server]Eroare la bind().\n");
    return errno;
  }

  if (listen(sd, 3) == -1)   // serverul incepe sa asculte daca vin clienti sa se conecteze 
  {
    perror("[server]Eroare la listen().\n");
    return errno;
  }

  printf("Nr threaduri %d \n", nr_threads);
  
  pthread_create(&multime_threads[0].idThread, NULL, &timer, db); //thread separat pentru functia de timer
  
  for (int i = 1; i <= nr_threads; i++)
    creare_thread(i);

  for (;;)  // se servesc in mod concurent clientii,folosind thread-uri 
  {
    printf("[server]Asteptam la portul %d...\n", PORT);
    pause();
  }
};

void actualizare_baza_date(sqlite3 *db) 
{
  char *errMsg = 0;

  char *update_query_produse = "UPDATE meniu SET cantitate = cantitate + CASE WHEN id <= 5 THEN 10 ELSE 4 END;";

  int cod = sqlite3_exec(db, update_query_produse, 0, 0, &errMsg);

  if (cod != SQLITE_OK) 
    fprintf(stderr, "Eroare la actualizare: %s\n", sqlite3_errmsg(db));
  else 
    printf("Baza de date actualizata cu succes\n");
  
}
void *timer(void *arg)
{
  sqlite3 *db = (sqlite3 *)arg;
  while(1)
  {
    sleep(40);
    actualizare_baza_date(db);  //actualizam baza de date
  }
}

void creare_thread(int i)
{
  int* j = malloc(sizeof(int));
  *j = i;
  pthread_create(&multime_threads[i].idThread, NULL, &treat, (void *)(j));
  return; /* threadul principal returneaza */
}

void *treat(void *arg)
{
  int client;

  struct sockaddr_in from;
  memset(&from,0, sizeof(from));
  printf("[thread]- %d - pornit...\n", *(int*)arg);

  for (;;)
  {
    int length = sizeof(from);
    pthread_mutex_lock(&lacat);
    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[thread]Eroare la accept().\n");
    }
    pthread_mutex_unlock(&lacat);
    multime_threads[*((int*)arg)].nr_conexiuni++;

    raspuns_spre_client(client, *((int*)arg)); // procesarea cererii

    close(client); //inchid conexiunea
  }
}

void raspuns_spre_client(int cl, int idThread)
{
    while(1)
    {
      size_t dim;  //dimensiunea mesajelor trimise spre client
      int cod,id_mancare,id_alergeni;
      char com[256]={0}; // mesajul primit de la client
      char rasp_com[4096]={0};    //mesajul de trimis spre client
      memset(com, 0, 256);
      memset(rasp_com,0,4096);

      read(cl,&cod,sizeof(int));   //citesc codul de comanda primit de la client
      cod=ntohs(cod);
      printf("[server]Am primit codul de comanda %d de la client\n",cod);
      if(cod==0)
      {
        printf("Era doar o eroare\n");
      }
      else if(cod==1)  //quit
      {
          printf("Inchid conexiunea cu clientul %d\n",idThread);
          break;
      }
      else if(cod==2)  //help
      {
          strcat(rasp_com,"Tasteaza 'meniu' pentru a vedea optiunile de mancare\n");
          strcat(rasp_com,"Tasteaza 'vreau' + id-ul mancarii pentru a comanda\n");
          strcat(rasp_com,"Tasteaza 'alergeni' pentru a vedea lista alergenilor din mancare\n");
          strcat(rasp_com,"Tasteaza 'quit' pentru a iesi");

          dim=strlen(rasp_com)+1;

          copie_dim=htons(dim);
          if(write(cl,&copie_dim,sizeof(dim))<=0)   //transmit dimensiunea mesajului 
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          if(write(cl,&rasp_com,dim)<=0)     //transmit mesajul propriu-zis(help)
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          printf("[Thread %d]Trimit mesajul inapoi...:\n",idThread);
          printf("%s\n", rasp_com);
      }
      else if(cod==3) //cerere
      {
        int verif_id;
        read(cl,&verif_id,sizeof(verif_id));   //citesc verificarea id_ului
        verif_id=ntohs(verif_id);

        if(verif_id>10)
        {
          printf("ID-ul nu era valid , se reia cererea\n");
        }
        else
        {
          int cantitate;
          read(cl,&dim,sizeof(size_t)); //primesc dimensiune id_mancare
          dim=ntohs(dim);
          if (read(cl, com, dim) < 0)  //primesc id_mancare
          {
            perror("[server]Eroare la read() de la client.\n");
          }

          id_mancare=atoi(com);
          
          printf("[server]Mesajul primit este:%d \n",id_mancare);  

          memset(rasp_com,0,4096);
          strcpy(rasp_com,"Introdu cantitate: ");
          dim=strlen(rasp_com)+1;
          
          copie_dim=htons(dim);
          if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          if (write(cl, rasp_com,dim) <= 0)   // transmit mesajul propriu-zis(introdu cantitate) 
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          printf("[Thread %d]Trimit mesajul inapoi...:\n",idThread);
          printf("%s\n", rasp_com);

          int verif_cantitate;
          read(cl,&verif_cantitate,sizeof(verif_cantitate));   //citesc verificarea_cantitatii
          verif_cantitate=ntohs(verif_cantitate);

          if(verif_cantitate==0)   //verificare cantitate valida/invalida
          {
            printf("Cantitatea nu era valida , se reia cererea\n");     
          }
          else
          {
            read(cl,&dim,sizeof(size_t));    //citesc dimensiunea mesajului
            dim=ntohl(dim);
            if (read(cl, com, dim) < 0)   //citesc mesajul propriu-zis (cantitatea primita)
            {
              perror("[server]Eroare la read() de la client.\n");
            }
            printf("[server]Mesajul primit este:\n");

            cantitate_ceruta=atoi(com);       
            printf("Cantitatea ceruta este:%d \n",cantitate_ceruta);

            memset(rasp_com,0,4096);
            sprintf(rasp_com, "Se incearca onorarea comenzii: %d", id_mancare);       
            strcat(rasp_com," (asteptati confirmare)");
            dim=strlen(rasp_com)+1;
            
            copie_dim=htons(dim);
            if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
            {
              printf("[Thread %d] ", idThread);
              perror("[Thread]Eroare la write() catre client.\n");
            }
            if (write(cl, rasp_com,dim) <= 0)   // transmit mesajul propriu-zis(onorarea comenzii)
            {
              printf("[Thread %d] ", idThread);
              perror("[Thread]Eroare la write() catre client.\n");
            }
            printf("[Thread %d]Trimit mesajul inapoi...:\n",idThread);
            printf("%s\n", rasp_com);

            sleep(2);
            
            sqlite3 *db;
            sqlite3_stmt *stmt;

            int cod_return = sqlite3_open_v2("database.db", &db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);   // Deschide conexiunea la baza de date 
            if (cod_return != SQLITE_OK)
                fprintf(stderr, "Eroare la deschiderea bazei de date: %s\n", sqlite3_errmsg(db));

            // Construieste si pregateste interogarea SQL cu un parametru de tip int
            const char *comanda_sql = "SELECT cantitate FROM meniu WHERE id = ?";
            cod_return = sqlite3_prepare_v2(db, comanda_sql, -1, &stmt, 0); 
            if (cod_return != SQLITE_OK) {
                fprintf(stderr, "Eroare la pregatirea interogarii: %s\n", sqlite3_errmsg(db));
                sqlite3_close(db);
                
            }

            sqlite3_bind_int(stmt, 1, id_mancare);        // Leaga valoarea variabilei id_mancare la parametrul din interogare

            cod_return = sqlite3_step(stmt);           // Executa interogarea
            if (cod_return == SQLITE_ROW) 
                cantitate = sqlite3_column_int(stmt, 0);  //iau cantitatea din interogare
          
            sqlite3_finalize(stmt);  //eliberez resursele utilizate in timpul interogarii

            if(cantitate_ceruta<=cantitate)
              disp=1;
            else disp=0;

            if(disp==0)  
            {
              memset(rasp_com,0,4096);
              
              if(cantitate==0)
                strcat(rasp_com,"Stocul este epuizat,reveniti mai tarziu");
              else
                strcat(rasp_com,"Comanda nu poate fi onorata..Introdu o alta cerere mai mica (vreau + id)"); //modificare mesaj pentru raspuns

              printf("[Thread %d]Trimit mesajul inapoi...%s\n", idThread, rasp_com);

              dim=strlen(rasp_com)+1;
            
              copie_dim=htons(dim);
              if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
              {
                printf("[Thread %d] ", idThread);
                perror("[Thread]Eroare la write() catre client.\n");
              }
              if (write(cl, rasp_com, dim) <= 0)   // returnam mesajul clientului 
              {
                printf("[Thread %d] ", idThread);
                perror("[Thread]Eroare la write() catre client.\n");
              }
              else
                printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", idThread); 
            }
            else
            {
              sqlite3_stmt *stmt;
              const char *comanda_sql = "UPDATE meniu SET cantitate = cantitate - ? WHERE id = ?"; //construiesc interogarea pentru a da update la cantitate;
              cod_return = sqlite3_prepare_v2(db, comanda_sql, -1, &stmt, 0);
              if (cod_return != SQLITE_OK) {
                  fprintf(stderr, "Eroare la pregatirea interogarii: %s\n", sqlite3_errmsg(db));
                  sqlite3_close(db);
              }
            
              sqlite3_bind_int(stmt, 1, cantitate_ceruta);  // Leaga valorile variabilelor la parametrii din interogare
              sqlite3_bind_int(stmt, 2, id_mancare);

              sqlite3_step(stmt);  //execut interogarea
        
              sqlite3_finalize(stmt);
              sqlite3_close(db);

              memset(rasp_com,0,4096);
              strcat(rasp_com,"Comanda va fi onorata"); //modificare mesaj pentru raspuns

              printf("[Thread %d]Trimit mesajul inapoi...%s\n", idThread, rasp_com);
              dim=strlen(rasp_com)+1;
            
              copie_dim=htons(dim);
              if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
              {
                printf("[Thread %d] ", idThread);
                perror("[Thread]Eroare la write() catre client.\n");
              }
              if (write(cl, rasp_com, dim) <= 0)   // returnam mesajul clientului 
              {
                printf("[Thread %d] ", idThread);
                perror("[Thread]Eroare la write() catre client.\n");
              }
              else
              printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", idThread);
            } 
          }
        }
      }
      else if(cod==4)  //meniu
      {
        char temp[4096]={0};
        memset(rasp_com,0,4096);
        
        sqlite3 *db;
        sqlite3_stmt *stmt;

        int cod_return = sqlite3_open_v2("database.db", &db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX,NULL);   // Deschid conexiunea la baza de date 
        if (cod_return != SQLITE_OK)
          fprintf(stderr, "Eroare la deschiderea bazei de date: %s\n", sqlite3_errmsg(db));

        const char *comanda_sql = "SELECT id,denumire_produs,cantitate FROM meniu";
        cod_return = sqlite3_prepare_v2(db, comanda_sql, -1, &stmt, 0);
          
        while ((cod_return = sqlite3_step(stmt)) == SQLITE_ROW) 
        {
          //obtin datele din interogare
          int id = sqlite3_column_int(stmt, 0);     
          const char *denumire = (const char *)sqlite3_column_text(stmt, 1);
          int cantitate = sqlite3_column_int(stmt, 2);

          snprintf(temp, sizeof(temp), "ID: %d, Denumire: %s, Cantitate: %d\n", id, denumire, cantitate);  //creez rezultatul interogarii
          strcat(rasp_com,temp);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        printf("[Thread %d]Trimit mesajul inapoi...\n", idThread);
        printf("%s\n",rasp_com);

        dim=strlen(rasp_com)+1;
        
        copie_dim=htons(dim);
        if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
        {
          printf("[Thread %d] ", idThread);
          perror("[Thread]Eroare la write() catre client.\n");
        }
        if (write(cl, rasp_com, dim) <= 0)   // returnam mesajul clientului 
        {
          printf("[Thread %d] ", idThread);
          perror("[Thread]Eroare la write() catre client.\n");
        }
        else
        printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", idThread); 
      }
      else if(cod==5)   //alergeni
      {
        memset(rasp_com,0,4096);

        read(cl,&id_alergeni,sizeof(int));   //citesc id_mancare primit de la client
        id_alergeni=ntohs(id_alergeni);

        if(id_alergeni>10)
        {
          printf("ID-ul nu era valid , se reia cererea\n");
        }
        else
        {
          sqlite3 *db;
          sqlite3_stmt *stmt;

          int cod_return = sqlite3_open_v2("database.db", &db,SQLITE_OPEN_READONLY|SQLITE_OPEN_FULLMUTEX,NULL);   // Deschid conexiunea la baza de date 
          if (cod_return != SQLITE_OK)
            fprintf(stderr, "Eroare la deschiderea bazei de date: %s\n", sqlite3_errmsg(db));

          const char *comanda_sql = "SELECT alergeni FROM meniu where id=?";  
          cod_return = sqlite3_prepare_v2(db, comanda_sql, -1, &stmt, 0);    //pregatesc interogarea
            
          sqlite3_bind_int(stmt, 1, id_alergeni);   //leg id_alergeni de ? din interogare

          sqlite3_step(stmt);    //execut interogarea
          const char *denumire = (const char *)sqlite3_column_text(stmt, 0);  //iau coloana de interes 

          sprintf(rasp_com,"Alergenii sunt:%s\n",denumire);

          sqlite3_finalize(stmt);
          sqlite3_close(db);

          printf("[Thread %d]Trimit mesajul inapoi...\n", idThread);
          printf("%s\n",rasp_com);

          dim=strlen(rasp_com)+1;
          
          copie_dim=htons(dim);
          if(write(cl,&copie_dim,sizeof(dim))<=0)    //transmit dimensiunea mesajului
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          if (write(cl, rasp_com, dim) <= 0)   // returnam mesajul clientului 
          {
            printf("[Thread %d] ", idThread);
            perror("[Thread]Eroare la write() catre client.\n");
          }
          else
          printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", idThread); 
        }
      }
    }
}