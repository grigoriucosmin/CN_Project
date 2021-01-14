#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <pthread.h>
#include <sqlite3.h>
#include <poll.h>

/* portul folosit */
#define PORT 2908

/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData{
    int idThread; //id-ul thread-ului tinut in evidenta de acest program
    int cl; //descriptorul intors de accept
}thData;

int threadsNumber=0;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);


static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for(i = 0; i<argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}



int main ()
{

    pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
    int i=0;
    struct sockaddr_in server;	// structura folosita de server
    struct sockaddr_in from;
    int nr;		//mesajul primit de trimis la client
    int sd;		//descriptorul de socket
    int pid;


    /* crearea unui socket */
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("[server]Eroare la socket().\n");
        return errno;
    }
    /* utilizarea optiunii SO_REUSEADDR */
    int on=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /* pregatirea structurilor de date */
    bzero (&server, sizeof (server));
    bzero (&from, sizeof (from));

    /* umplem structura folosita de server */
    /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;
    /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    /* utilizam un port utilizator */
    server.sin_port = htons (PORT);

    /* atasam socketul */
    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
    {
        perror ("[server]Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen (sd, 2) == -1)
    {
        perror ("[server]Eroare la listen().\n");
        return errno;
    }

    /* servim in mod concurent clientii...folosind thread-uri */
    while (1)
    {
        int client;
        thData *td; //parametru functia executata de thread
        int length = sizeof (from);

        printf ("[server]Asteptam la portul %d...\n", PORT);
        fflush (stdout);

        /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
        if ((client = accept (sd, (struct sockaddr *) &from, reinterpret_cast<socklen_t *>(&length))) < 0)
        {
            perror ("[server]Eroare la accept().\n");
            continue;
        }

        /* s-a realizat conexiunea */

        // int idThread; //id-ul threadului
        // int cl; //descriptorul intors de accept

        td=(struct thData*)malloc(sizeof(struct thData));
        td->idThread=i++;
        td->cl=client;

        pthread_create(&th[i], NULL, &treat, td);

    }//while
}

static void *treat(void * arg)
{
    struct thData tdL;
    tdL= *((struct thData*)arg);
    printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush (stdout);
    pthread_detach(pthread_self());
    raspunde((struct thData*)arg);
    /* am terminat cu acest client, inchidem conexiunea */
    close ((intptr_t)arg);
    return(NULL);

}


void raspunde(void *arg)
{
    char user[256], pass[256];
    int loggedIn=0;

    int i=0;
    struct thData tdL;
    tdL= *((struct thData*)arg);

    while(loggedIn==0)
    {
        char buf[1000];
        char *p;

        bzero(buf, sizeof (buf));
        fflush(stdout);
        if (read (tdL.cl, &buf, sizeof(buf)) <= 0)
        {
            printf("[Thread %d]\n", tdL.idThread);
            perror ("Eroare la read() de la client.\n");
        }

        printf ("[Thread %d] Mesajul a fost receptionat...%s\n", tdL.idThread, buf);
        fflush(stdout);
        p=strtok(buf, " :\n");
        printf("Comanda este %s\n", p);

        if(strcmp(p, "quit")==0)
            break;
        if(strcmp(p, "login")==0)
        {
            bool userOk=0;
            bool passOk=0;

            strcpy(user, strtok(NULL, " :\n"));
            strcpy(pass, strtok(NULL, " :\n"));

            //pregatim mesajul de raspuns
            // * Cautam in baza de date


            sqlite3 *db;
            sqlite3_stmt * stmt;
            int nrecs;
            char *zErrMsg = 0;
            int i;
            int rc;
            char sql[256];
            int row = 0;

            // Open database
            rc = sqlite3_open("database", &db);


            if(rc)
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            else
                fprintf(stderr, "Opened database successfully\n");

            // Create SQL statement
            strcpy(sql, "SELECT user, pass FROM Clients WHERE user='");
            strcat(sql, user);
            strcat(sql, "' and pass='");
            strcat(sql, pass);
            strcat(sql, "';");
            printf("%s\n", sql);

            // Execute SQL statement
            rc=sqlite3_prepare_v2(db, sql, strlen(sql)+1, &stmt, NULL);
            if(rc!=SQLITE_OK)
            {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                userOk=0;
            }
            else
            {
                fprintf(stdout, "Record search successful\n");
            }

            while (1) {
                int s;
                s = sqlite3_step(stmt);
                if (s == SQLITE_ROW) {
                    const unsigned char *text, *text2;
                    text = sqlite3_column_text(stmt, 0);
                    text2=sqlite3_column_text(stmt, 1);
                    printf("%d: %s %s\n", row, text, text2);
                    if(strcmp((const char *)text, user)==0 && strcmp((const char *)text2, pass)==0)
                    {
                        userOk=1;
                        passOk=1;
                    }
                } else if (s == SQLITE_DONE) {
                    printf("DONE\n");
                    break;
                } else {
                    fprintf(stderr, "Search failed\n");
                    userOk = 0;
                    break;
                }
            }


            sqlite3_close(db);


            if(userOk && passOk)
            {
                bzero(buf, sizeof (buf));
                strcpy(buf, "Login successful!");
                printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, buf);


                // returnam mesajul clientului
                if (write (tdL.cl, &buf, sizeof (buf)) <= 0)
                {
                    printf("[Thread %d] ",tdL.idThread);
                    perror ("[Thread]Eroare la write() catre client.\n");
                }
                else
                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                loggedIn=1;

                /* Open database */
                rc = sqlite3_open("database", &db);

                if(rc)
                    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                else
                    fprintf(stderr, "Opened database successfully\n");


                /* Create merged SQL statement */
                bzero(sql, sizeof (sql));
                strcpy(sql, "UPDATE Clients SET online=1 WHERE user='");
                strcat(sql, user);
                strcat(sql, "' and pass='");
                strcat(sql, pass);
                strcat(sql, "';");
                printf("%s\n", sql);

                /* Execute SQL statement */
                rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                if( rc != SQLITE_OK ) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                } else {
                    fprintf(stdout, "Operation done successfully\n");
                }
                sqlite3_close(db);

            }
            else
            {
                bzero(buf, sizeof (buf));
                strcpy(buf, "Login failed!");
                printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, buf);

                // returnam mesajul clientului
                if (write (tdL.cl, &buf, sizeof(buf)) <= 0)
                {
                    printf("[Thread %d] ",tdL.idThread);
                    perror ("[Thread]Eroare la write() catre client.\n");
                }
                else
                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                loggedIn=0;
            }
        }

        if(strcmp(p, "register")==0)
        {
            int userExists;

            strcpy(user, strtok(NULL, " :\n"));
            strcpy(pass, strtok(NULL, " :\n"));
            //pregatim mesajul de raspuns
            // * Cautam in baza de date


            sqlite3 *db;
            char *zErrMsg = 0;
            int rc;
            char sql[256];

            // Open database
            rc = sqlite3_open("database", &db);

            if(rc)
                fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            else
                fprintf(stderr, "Opened database successfully\n");

            // Create SQL statement
            strcpy(sql, "INSERT INTO Clients (user, pass, online) VALUES('");
            strcat(sql, user);
            strcat(sql, "', '");
            strcat(sql, pass);
            strcat(sql,"', 0);");

            // Execute SQL statement
            rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

            if(rc!=SQLITE_OK)
            {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
                userExists=1;
            }
            else
            {
                fprintf(stdout, "Record inseration successful\n");
                userExists=0;
            }

            sqlite3_close(db);


            if(userExists==0)
            {
                bzero(buf, sizeof (buf));
                strcpy(buf, "Register successful!");
                printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, buf);

                // returnam mesajul clientului
                if (write (tdL.cl, &buf, sizeof(buf)) <= 0)
                {
                    printf("[Thread %d] ",tdL.idThread);
                    perror ("[Thread]Eroare la write() catre client.\n");
                }
                else
                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                loggedIn=1;

                /* Open database */
                rc = sqlite3_open("database", &db);

                if(rc)
                    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                else
                    fprintf(stderr, "Opened database successfully\n");


                /* Create merged SQL statement */
                bzero(sql, sizeof (sql));
                strcpy(sql, "UPDATE Clients SET online=1 WHERE user='");
                strcat(sql, user);
                strcat(sql, "' and pass='");
                strcat(sql, pass);
                strcat(sql, "';");
                printf("%s\n", sql);

                /* Execute SQL statement */
                rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                if( rc != SQLITE_OK ) {
                    fprintf(stderr, "SQL error: %s\n", zErrMsg);
                    sqlite3_free(zErrMsg);
                } else {
                    fprintf(stdout, "Operation done successfully\n");
                }
                sqlite3_close(db);
            }
            if(userExists==1)
            {
                bzero(buf, sizeof (buf));
                strcpy(buf, "Register failed!");
                printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, buf);

                // returnam mesajul clientului
                if (write (tdL.cl, &buf, sizeof(buf)) <= 0)
                {
                    printf("[Thread %d] ",tdL.idThread);
                    perror ("[Thread]Eroare la write() catre client.\n");
                }
                else
                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                loggedIn=0;
            }
        }

    }
    int selectStat;
    struct timeval tv;
    fd_set sdread;
    while(loggedIn)
    {
        char buf[1000];
        char *p;


        sqlite3 *db;
        sqlite3_stmt * stmt;
        int nrecs;
        char *zErrMsg = 0;
        int i;
        int rc;
        char sql[256];
        int row = 0;

        // Open database
        rc = sqlite3_open("database", &db);

        if(rc)
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        else
            fprintf(stderr, "Opened database successfully\n");

        // Create SQL statement
        strcpy(sql, "SELECT sender, message, sent_at FROM Messages WHERE receiver='");
        strcat(sql, user);
        strcat(sql, "' AND seen=0;");
        printf("%s\n", sql);

        // Execute SQL statement
        rc=sqlite3_prepare_v2(db, sql, strlen(sql)+1, &stmt, NULL);
        if(rc!=SQLITE_OK)
        {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else
        {
            fprintf(stdout, "Record search successful\n");
        }

        while (1) {
            int s;
            s = sqlite3_step(stmt);
            if (s == SQLITE_ROW) {
                const unsigned char *snd, *msg, *sent_at;
                snd = sqlite3_column_text(stmt, 0);
                msg = sqlite3_column_text(stmt, 1);
                sent_at = sqlite3_column_text(stmt, 2);
                printf("%d: %s %s %s\n", row, snd, msg, sent_at);

                char text[1000];
                bzero(text, sizeof (text));
                strcat(text, "[");
                strcat(text, (const char*)sent_at);
                strcat(text, "][");
                strcat(text, (const char*)snd);
                strcat(text, "] ");
                strcat(text, (const char*)msg);
                printf("%s\n", text);

                if (write (tdL.cl, &text, sizeof(text)) <= 0)
                {
                    printf("[Thread %d] ",tdL.idThread);
                    perror ("[Thread]Eroare la write() catre client.\n");
                }
                else
                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

            } else if (s == SQLITE_DONE) {
                printf("DONE\n");
                break;
            } else {
                fprintf(stderr, "Search failed\n");
                break;
            }
        }
        sqlite3_close(db);

        /* Open database */
        rc = sqlite3_open("database", &db);

        if(rc)
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        else
            fprintf(stderr, "Opened database successfully\n");


        /* Create merged SQL statement */
        bzero(sql, sizeof (sql));
        strcpy(sql, "UPDATE Messages SET seen=1 WHERE receiver='");
        strcat(sql, user);
        strcat(sql, "' and seen=0;");
        printf("%s\n", sql);

        /* Execute SQL statement */
        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

        if( rc != SQLITE_OK ) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Operation done successfully\n");
        }
        sqlite3_close(db);

        bzero(buf, sizeof (buf));
        fflush(stdout);


        tv.tv_sec=1;
        tv.tv_usec=0;
        FD_ZERO(&sdread);
        FD_SET(tdL.cl, &sdread);
        selectStat=select(tdL.cl+1, &sdread, NULL, NULL, &tv);
        switch (selectStat)
        {
            case -1:
                perror("Eroare la select().\n");
                break;
            case 0:
                break;
            default:
                if (read (tdL.cl, &buf, sizeof(buf)) <= 0)
                {
                    printf("[Thread %d]\n", tdL.idThread);
                    perror ("Eroare la read() de la client.\n");
                }

                printf ("[Thread %d] Mesajul a fost receptionat...%s\n", tdL.idThread, buf);
                fflush(stdout);
                p=strtok(buf, " :\n");
                printf("Comanda este %s\n", p);


                if(strcmp(p, "quit")==0)
                {
                    loggedIn=0;
                    sqlite3 *db;
                    char *zErrMsg = 0;
                    int rc;
                    char sql[256];
                    /* Open database */
                    rc = sqlite3_open("database", &db);

                    if(rc)
                        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                    else
                        fprintf(stderr, "Opened database successfully\n");


                    /* Create merged SQL statement */
                    bzero(sql, sizeof (sql));
                    strcpy(sql, "UPDATE Clients SET online=0 WHERE user='");
                    strcat(sql, user);
                    strcat(sql, "' and pass='");
                    strcat(sql, pass);
                    strcat(sql, "';");
                    printf("%s\n", sql);

                    /* Execute SQL statement */
                    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                    if( rc != SQLITE_OK ) {
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    } else {
                        fprintf(stdout, "Operation done successfully\n");
                    }
                    sqlite3_close(db);
                    break;
                }


                if(strcmp(p, "msg")==0)
                {
                    char receiver[256], message[500];
                    strcpy(receiver, strtok(NULL, " :\n"));
                    strcpy(message, strtok(NULL, "\n"));

                    sqlite3 *db;
                    char *zErrMsg = 0;
                    int rc;
                    char sql[256];

                    // Open database
                    rc = sqlite3_open("database", &db);

                    if(rc)
                        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                    else
                        fprintf(stderr, "Opened database successfully\n");

                    // Create SQL statement
                    strcpy(sql, "INSERT INTO Messages (sender, receiver, message, seen, sent_at) VALUES('");
                    strcat(sql, user);
                    strcat(sql, "', '");
                    strcat(sql, receiver);
                    strcat(sql,"', '");
                    strcat(sql, message);
                    strcat(sql, "', 0, datetime('now'));");

                    // Execute SQL statement
                    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                    if(rc!=SQLITE_OK)
                    {
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    }
                    else
                        fprintf(stdout, "Record inseration successful\n");

                    sqlite3_close(db);
                }

                if(strcmp(p, "history")==0)
                {
                    char user2[256];
                    strcpy(user2, strtok(NULL, " :\n"));

                    if(strcmp(user2, "all")==0)
                    {
                        sqlite3 *db;
                        sqlite3_stmt * stmt;
                        int nrecs;
                        char *zErrMsg = 0;
                        int i;
                        int rc;
                        char sql[256];
                        int row = 0;

                        // Open database
                        rc = sqlite3_open("database", &db);

                        if(rc)
                            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                        else
                            fprintf(stderr, "Opened database successfully\n");

                        // Create SQL statement
                        strcpy(sql, "SELECT sender, receiver, message, sent_at FROM Messages WHERE receiver='");
                        strcat(sql, user);
                        strcat(sql, "' OR sender='");
                        strcat(sql, user);
                        strcat(sql, "';");
                        printf("%s\n", sql);

                        // Execute SQL statement
                        rc=sqlite3_prepare_v2(db, sql, strlen(sql)+1, &stmt, NULL);
                        if(rc!=SQLITE_OK)
                        {
                            fprintf(stderr, "SQL error: %s\n", zErrMsg);
                            sqlite3_free(zErrMsg);
                        }
                        else
                        {
                            fprintf(stdout, "Record search successful\n");
                        }

                        while (1) {
                            int s;
                            s = sqlite3_step(stmt);
                            if (s == SQLITE_ROW) {
                                const unsigned char *snd, *rec, *msg, *sent_at;
                                snd = sqlite3_column_text(stmt, 0);
                                rec = sqlite3_column_text(stmt, 1);
                                msg = sqlite3_column_text(stmt, 2);
                                sent_at = sqlite3_column_text(stmt, 3);
                                printf("%d: %s %s %s %s\n", row, snd, rec, msg, sent_at);

                                char text[1000];
                                bzero(text, sizeof (text));
                                strcat(text, "[");
                                strcat(text, (const char*)sent_at);
                                strcat(text, "][");
                                strcat(text, (const char*)snd);
                                strcat(text, "] -> [");
                                strcat(text, (const char*)rec);
                                strcat(text, "] ");
                                strcat(text, (const char*)msg);
                                printf("%s\n", text);

                                if (write (tdL.cl, &text, sizeof(text)) <= 0)
                                {
                                    printf("[Thread %d] ",tdL.idThread);
                                    perror ("[Thread]Eroare la write() catre client.\n");
                                }
                                else
                                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                            } else if (s == SQLITE_DONE) {
                                printf("DONE\n");
                                break;
                            } else {
                                fprintf(stderr, "Search failed\n");
                                break;
                            }
                        }
                        sqlite3_close(db);
                    }
                    else
                    {
                        sqlite3 *db;
                        sqlite3_stmt * stmt;
                        int nrecs;
                        char *zErrMsg = 0;
                        int i;
                        int rc;
                        char sql[256];
                        int row = 0;

                        // Open database
                        rc = sqlite3_open("database", &db);

                        if(rc)
                            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                        else
                            fprintf(stderr, "Opened database successfully\n");

                        // Create SQL statement
                        strcpy(sql, "SELECT sender, receiver, message, sent_at FROM Messages WHERE sender='");
                        strcat(sql, user);
                        strcat(sql, "' AND receiver='");
                        strcat(sql, user2);
                        strcat(sql, "' OR sender='");
                        strcat(sql, user2);
                        strcat(sql, "' AND receiver='");
                        strcat(sql, user);
                        strcat(sql, "';");
                        printf("%s\n", sql);

                        // Execute SQL statement
                        rc=sqlite3_prepare_v2(db, sql, strlen(sql)+1, &stmt, NULL);
                        if(rc!=SQLITE_OK)
                        {
                            fprintf(stderr, "SQL error: %s\n", zErrMsg);
                            sqlite3_free(zErrMsg);
                        }
                        else
                        {
                            fprintf(stdout, "Record search successful\n");
                        }

                        while (1) {
                            int s;
                            s = sqlite3_step(stmt);
                            if (s == SQLITE_ROW) {
                                const unsigned char *snd, *rec, *msg, *sent_at;
                                snd = sqlite3_column_text(stmt, 0);
                                rec = sqlite3_column_text(stmt, 1);
                                msg = sqlite3_column_text(stmt, 2);
                                sent_at = sqlite3_column_text(stmt, 3);
                                printf("%d: %s %s %s %s\n", row, snd, rec, msg, sent_at);

                                char text[1000];
                                bzero(text, sizeof (text));
                                strcat(text, "[");
                                strcat(text, (const char*)sent_at);
                                strcat(text, "][");
                                strcat(text, (const char*)snd);
                                strcat(text, "] -> [");
                                strcat(text, (const char*)rec);
                                strcat(text, "] ");
                                strcat(text, (const char*)msg);
                                printf("%s\n", text);

                                if (write (tdL.cl, &text, sizeof(text)) <= 0)
                                {
                                    printf("[Thread %d] ",tdL.idThread);
                                    perror ("[Thread]Eroare la write() catre client.\n");
                                }
                                else
                                    printf ("[Thread %d]Mesajul a fost trasmis cu succes.\n",tdL.idThread);

                            } else if (s == SQLITE_DONE) {
                                printf("DONE\n");
                                break;
                            } else {
                                fprintf(stderr, "Search failed\n");
                                break;
                            }
                        }
                        sqlite3_close(db);
                    }
                }
                if(strcmp(p, "reply")==0)
                {
                    char timp[256], mesaj[500], mesajFinal[500], sndr[256], mesajPrimit[500];
                    bzero(timp, sizeof (timp));
                    bzero(mesaj, sizeof (mesaj));
                    bzero(mesajFinal, sizeof (mesajFinal));
                    strcpy(timp, strtok(NULL, "]")+1);
                    strcpy(mesaj, strtok(NULL, "\n")+1);


                    sqlite3 *db;
                    sqlite3_stmt * stmt;
                    int nrecs;
                    char *zErrMsg = 0;
                    int i;
                    int rc;
                    char sql[256];
                    int row = 0;

                    // Open database
                    rc = sqlite3_open("database", &db);

                    if(rc)
                        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                    else
                        fprintf(stderr, "Opened database successfully\n");

                    // Create SQL statement
                    strcpy(sql, "SELECT sender, message FROM Messages WHERE receiver='");
                    strcat(sql, user);
                    strcat(sql, "' AND sent_at='");
                    strcat(sql, timp);
                    strcat(sql, "';");
                    printf("%s\n", sql);

                    // Execute SQL statement
                    rc=sqlite3_prepare_v2(db, sql, strlen(sql)+1, &stmt, NULL);
                    if(rc!=SQLITE_OK)
                    {
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    }
                    else
                    {
                        fprintf(stdout, "Record search successful\n");
                    }

                    while (1) {
                        int s;
                        s = sqlite3_step(stmt);
                        if (s == SQLITE_ROW) {
                            const unsigned char *snd;
                            const unsigned char *msj;
                            snd = sqlite3_column_text(stmt, 0);
                            msj = sqlite3_column_text(stmt, 1);
                            printf("%d: %s %s\n", row, snd, msj);
                            strcpy(mesajPrimit, (const char*)msj);
                            strcpy(sndr, (const char*)snd);
                        } else if (s == SQLITE_DONE) {
                            printf("DONE\n");
                            break;
                        } else {
                            fprintf(stderr, "Search failed\n");
                            break;
                        }
                    }
                    sqlite3_close(db);


                    strcpy(mesajFinal, "[Reply to ([");
                    strcat(mesajFinal, timp);
                    strcat(mesajFinal, "][");
                    strcat(mesajFinal, sndr);
                    strcat(mesajFinal,"] ");
                    strcat(mesajFinal, mesajPrimit);
                    strcat(mesajFinal, ")]\t");
                    strcat(mesajFinal, mesaj);


                    // Open database
                    rc = sqlite3_open("database", &db);

                    if(rc)
                        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
                    else
                        fprintf(stderr, "Opened database successfully\n");

                    // Create SQL statement
                    strcpy(sql, "INSERT INTO Messages (sender, receiver, message, seen, sent_at) VALUES('");
                    strcat(sql, user);
                    strcat(sql, "', '");
                    strcat(sql, sndr);
                    strcat(sql,"', '");
                    strcat(sql, mesajFinal);
                    strcat(sql, "', 0, datetime('now'));");

                    // Execute SQL statement
                    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

                    if(rc!=SQLITE_OK)
                    {
                        fprintf(stderr, "SQL error: %s\n", zErrMsg);
                        sqlite3_free(zErrMsg);
                    }
                    else
                        fprintf(stdout, "Record inseration successful\n");

                    sqlite3_close(db);
                }
                break;
        }


    }
}


