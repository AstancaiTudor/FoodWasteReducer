#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <ctype.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

int port;
int sd;                    // descriptorul de socket
struct sockaddr_in server; // structura folosita pentru conectare
char com[256],cerere[256],cantitate[10],id_mancare[10],id_alergeni[10];
int cod,copie_cod,verif_id;

int main(int argc, char *argv[])
{
  
  if (argc != 3)   //verificare argumente linie de comanda
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  port = atoi(argv[2]);

  if((sd=socket(AF_INET,SOCK_STREAM,0))==-1) //creare socket
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  server.sin_family = AF_INET;   // familia socket-ului 
  server.sin_addr.s_addr = inet_addr(argv[1]);   // adresa IP a serverului 
  server.sin_port = htons(port);  // portul de conectare 

  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)   // conectare la server 
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  printf("[client]Tastati 'help' pentru a vedea comenzile:");
  fflush(stdout);
  while(1)
  {
    memset(com,0,256);
    memset(cerere,0,256);
    fgets(com,sizeof(com),stdin);
    com[strlen(com)-1]='\0'; 

    strncpy(cerere,com,5);  //iau cuvantul 'vreau'
    strcpy(id_mancare,com+6);  //iau id-ul produsului
    verif_id=atoi(id_mancare);

    if(strcmp(com,"quit")==0)   //identificare a comenzilor
      cod=1;
    else if(strcmp(com,"help")==0)
      cod=2;
    else if(strcmp(cerere,"vreau")==0)
      cod=3;
    else if(strcmp(com,"meniu")==0)
      cod=4;
    else if(strcmp(com,"alergeni")==0)
      cod=5;
    else cod=0;

    copie_cod=htons(cod);
    if(write(sd,&copie_cod,sizeof(int))<=0)   //transmit codul de comanda spre client
    {
      perror("[client]Eroare la write() spre server.\n");
      return errno;
    }

    if(cod==0)
    {
      printf("Nu ati introdus o comanda valida\n");
    }
    else if(cod==1)  //quit
    {
      break;
    }
    else if(cod==2)   //help
    {
      size_t length;
     
      read(sd,&length,sizeof(size_t));   //citesc dimensiunea mesajului
      length=ntohs(length);
      if (read(sd, com, length) < 0)   //citesc mesajul propriu-zis ( help )
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
      printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
      printf("%s\n",com); 
    }
    else if(cod==3)  //cerere
    {
      int copie_verif_id=htons(verif_id);
        if(write(sd,&copie_verif_id,sizeof(int))<=0)   //transmit id spre server
          {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
          }

      if(verif_id>10)
      {
        printf("Introdu un id_mancare valid\n");   
      }
      else
      {
        size_t length,copie_length;
        length=strlen(id_mancare)+1;
        copie_length=htons(length);
        if(write(sd,&copie_length,sizeof(size_t))<=0) //trimit dimensiune id_mancare
        {
          perror("[client]Eroare la write() spre server.\n");
          return errno;
        }
        if(write(sd,id_mancare,length)<=0) //trimit id_mancare
        {
          perror("[client]Eroare la write() spre server.\n");
          return errno;
        }

        read(sd,&length,sizeof(size_t));   //primesc dimensiune mesaj 
        length=ntohs(length);
        if (read(sd, com, length) < 0)          //primesc mesajul "introdu cantitate"
        {
          perror("[client]Eroare la read() de la server.\n");
          return errno;
        }
        printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
        printf("%s\n",com);

        fgets(cantitate,sizeof(cantitate),stdin);
        cantitate[strlen(cantitate)-1]='\0'; 

        int verif_cantitate=1;
        for(int i=0;i<strlen(cantitate);i++)
          if(isdigit(cantitate[i])==0)
            verif_cantitate=0;
            
        int copie_verif_cantitate=htons(verif_cantitate);
        if(write(sd,&copie_verif_cantitate,sizeof(int))<=0)   //transmit cantitatea (spre validare) spre server
          {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
          }

        if(verif_cantitate==0)
        {
          printf("Nu ai introdus o cantitate valida. Trebuie sa refaci cererea (vreau + id):\n");
        } 

        else 
        {
          length=strlen(cantitate)+1;
          copie_length=htonl(length);     
          if(write(sd,&copie_length,sizeof(size_t))<=0)   //trimit dimensiunea mesajului
          {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
          }
          if(write(sd,cantitate,length)<=0)     //trimit cantitate
          {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
          }

          read(sd,&length,sizeof(size_t));    //citesc dimensiunea mesajului
          length=ntohs(length);
          if (read(sd, com, length) < 0)   //citesc mesajul cu primirea confirmarii
          {
            perror("[client]Eroare la read() de la server.\n");
            return errno;
          }
          printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
          printf("%s\n",com);

          read(sd,&length,sizeof(size_t));   //citesc dimensiunea mesajului
          length=ntohs(length);   
          if (read(sd, com, length) < 0)   //citesc concluzia legata de comanda
          {
            perror("[client]Eroare la read() de la server.\n");
            return errno;
          }
          printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
          printf("%s\n",com);
        }
      }
    }
    else if(cod==4)   //meniu
    {
      size_t length;

      read(sd,&length,sizeof(size_t));     //citesc dimensiunea mesajului pe care il va primi clientul
      length=ntohs(length);
      if (read(sd, com, length) < 0)     //citesc mesajul propriu-zis (meniul)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
      printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
      printf("%s\n",com);
    }
    else if(cod==5)   //alergeni
    {
      size_t length;

      printf("Introdu id-ul mancarii pentru care vrei sa afli alergenii:\n");
      fgets(id_alergeni,sizeof(id_alergeni),stdin);    //citesc de la tastatura un id 
      id_alergeni[strlen(id_alergeni)-1]='\0';

      int alergeni;
      alergeni=atoi(id_alergeni);
          
  
      int copie_alergeni=htons(alergeni);
      if(write(sd,&copie_alergeni,sizeof(int))<=0)   //transmit id-ul mancarii pentru care se cer alergenii spre client
      {
        perror("[client]Eroare la write() spre server.\n");          
        return errno;
      }
      if(alergeni>10)    // verificare validitate id_mancare pentru alergeni
      {
        printf("Introdu un id_mancare valid\n");
      }
      else
      {
        read(sd,&length,sizeof(size_t));   //citesc dimensiunea mesajului pe care il va primi clientul
        length=ntohs(length);
        if (read(sd, com, length) < 0)     //citesc mesajul propriu-zis
        {
          perror("[client]Eroare la read() de la server.\n");
          return errno;
        }
        printf("[client]Mesajul primit este:\n");   // afisez mesajul primit 
        printf("%s\n",com);
      }
    }
  }
  close(sd);
}