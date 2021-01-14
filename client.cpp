/* cliTCPIt.c - Exemplu de client TCP
   Trimite un numar la server; primeste de la server numarul incrementat.
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <netdb.h>
#include <cstring>

typedef struct thData
{
    int idThread;
    int cl;
}thData;

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;
bool loggedIn=false;

static void *treat(void *);


int main (int argc, char *argv[])
{
    int sd;			// descriptorul de socket
    struct sockaddr_in server;	// structura folosita pentru conectare

    char buf[1000]; //buffer pentru mesaje transmise/primite
    char username[256]; //stocam numele utilizatorului
    char *p;
    int quit=0;

    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi (argv[2]);

    /* cream socketul */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons (port);

    /* ne conectam la server */
    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client]: Eroare la connect().\n");
        return errno;
    }

    //Login/Register loop
    while(!loggedIn)
    {
        bzero(buf, sizeof (buf));
        printf ("\n--------------------\n[client]: Please use login:<user>:<pass> or register:<user>:<pass>.\n--------------------\n ");
        fflush (stdout);
        read (0, buf, sizeof(buf));

        if(write(sd, &buf, sizeof(buf)) <= 0)
        {
            perror ("[client]: Eroare la write() spre server.\n");
            return errno;
        }

        p=strtok(buf, " :\n");
        if(strcmp(p, "quit")==0)
        {
            close(sd);
            exit(0);
        }
        else
        {

            strcpy(username, strtok(NULL, ":"));
            bzero(buf, sizeof(buf));
            /* citirea raspunsului dat de server
            (apel blocant pana cand serverul raspunde) */
            if (read (sd, &buf, sizeof(buf)) < 0)
            {
                perror ("[client]: Eroare la read() de la server.\n");
                return errno;
            }
            /* afisam mesajul primit */
            printf ("\n--------------------\n[Server]: %s\n--------------------\n", buf);
            if(strcmp(buf, "Login successful!")==0 || strcmp(buf, "Register successful!")==0)
                loggedIn=true;
            if(strcmp(buf, "Login failed!")==0 || strcmp(buf, "Register failed!")==0)
            {
                printf("\n--------------------\n!Use another username!\n--------------------\n");
                loggedIn=false;
            }
        }
    }
    if(loggedIn)
    {
        printf("Logged in as [%s]!\n", username);
        pthread_t th;
        thData *td;
        td=(struct thData*)malloc(sizeof (struct thData));
        td->idThread=1;
        td->cl=sd;

        pthread_create(&th, NULL, &treat, td);
    }
    while(loggedIn)
    {
        char aux[1000];
        bzero(aux, sizeof (aux));
        sleep(1);
        bzero(buf, sizeof (buf));
        bzero(p, sizeof (p));
        fflush (stdout);
        read (0, buf, sizeof(buf));
        strcpy(aux, buf);

        p=strtok(aux, " :\n");

        if(strcmp(p, "msg")==0 || strcmp(p, "reply")==0 || strcmp(p, "history")==0 || strcmp(p, "quit")==0)
        {
            if(write(sd, &buf, sizeof(buf)) <= 0)
            {
                perror ("[client]: Eroare la write() spre server.\n");
                return errno;
            }
            printf("--------------------\n");
        }
        else
        {
            printf("--------------------\n[client] Commands: msg:<user> <message>, reply:<date time> <message>\n[client] history:<all / user>, quit.\n--------------------\n");
            continue;
        }

        if(strcmp(p, "quit")==0)
        {
            loggedIn=false;
            close(sd);
            exit(0);
        }

    }
}

static void *treat(void * arg)
{
    struct thData tdL;
    tdL=*((struct thData*)arg);
    char buf[1000];
    fflush (stdout);
    pthread_detach(pthread_self());
    while(loggedIn)
    {

        bzero(buf, sizeof(buf));
        /* citirea raspunsului dat de server
        (apel blocant pana cand serverul raspunde) */
        if (read (tdL.cl, &buf, sizeof(buf)) < 0)
        {
            perror ("[client]: Eroare la read() de la server.\n");
        }
        /* afisam mesajul primit */
        printf ("%s\n--------------------\n", buf);

    }
    close((intptr_t)arg);
    return(NULL);
}