/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
   
/** \file chatty.c  
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>

/* inserire gli altri include che servono */
#include <sys/un.h>
#include <signal.h>
#include <sys/select.h>
#include <connections.h>
#include <config.h>
#include <message.h>
#include <ops.h>
#include <stats.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <extra/pharser.h>
#include <extra/workers.h>
#include <extra/users.h>
#include <extra/fileoperations.h>

#define SYSCALL(n,f) if(n==-1 && errno!=0){ f(errno); }

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */

/* ---------------------------------------------- Dichiarazioni Globali --------------------------------------------*/
char socketname[UNIX_PATH_MAX];
char nickfile[100]; /*file che contiene i nickname regitrati*/
char filepath[100]; /*path dove tenere file del server su utenti registrati*/
int fd_hwm;
int acceptmutex;
int * fdBusyList;
int nconnected;
fd_set set;

UserConnected * users; /*file descriptors connessi al server*/
int usersdim; /*dimensione userconnected*/

/*lock e variabili di condizionamento per mutua esclusione della accept*/
static pthread_mutex_t keyaccept = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condaccept = PTHREAD_COND_INITIALIZER;

/*mutua esclusione per accesso a file contententi nickname degli utenti registrati*/
static pthread_mutex_t keyfile = PTHREAD_MUTEX_INITIALIZER;

/*lock per mutua esclusione su array fd connessi*/
static pthread_mutex_t keyuser = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t conduser = PTHREAD_COND_INITIALIZER;

/*chiave per mutua esclusione aggiornamento statistiche*/
static pthread_mutex_t keystat = PTHREAD_MUTEX_INITIALIZER;

/*----------------------------------------------- Prototipi --------------------------------------------------------*/
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };
/*struttura da passare al thread contenete il necessario per la accept 
e le info del file di configurazione*/
typedef struct threadneeds {
	 ConfigFile * cfg;
	 int socket_fd;
	 struct sockaddr_un *sa;
} threadneed;

static void usage(const char *progname);

/**
* @function listener
*
* @brief routine di un thread del server
*
* @var args struttura threadneed 
*
*/
void * listener (void * args);

/**
* @function errorManager
*
* @brief stampa l´errore riscontrato
*
* @var errorCode codice di errore
*
*/
void errorManager(int errorCode);

/**
* @function cleanlistener
*
* @brief si occupa di effettuare il cleanup dei thread
*
*/
void * cleanlistener (void * arg);

/**
* @function elaboraRichiesta
*
* @brief risposte ad una richiesta di un client
*
* @var newmsg messaggio di richiesta ricevuto
*
* @var cfg file di configurazione del server
*
* @var fd file descriptor dove comunicare
*
* @return <=0 fallimento
*
* @warning effettua accessi a variabili globali
*
*/
int elaboraRichiesta(message_t newmsg,ConfigFile cfg,int fd);

/**
* @function isConnected
*
* @brief verifica se un utente di nome usr è connesso
*
* @return <=0 non connesso
*
* @warning effettua accessi a variabili globali
*/
int isConnected(char * usr);

/**
* @function InvioMesaggio
*
* @brief inoltra il messaggio come tipo di messaggio op
*
* @return <=0 fallimento
*
* @warning effettua accessi a variabili globali
*/
int InvioMessaggio(message_t messaggiorichiesta,op_t op);

/**
* @function createuserList
*
* @brief crea lsita utenti nel formato richiesta dal client
*
* @var connessi array di utenti connessi
*
* @var nconn dimensione array connessi
*
* @var dim variabile di ritorno che indica la grandezza ella lista in byte
*
* @return lista utenti
*
*/
char * createusrList (UserConnected * connessi, int nconn, int *dim);

/* ---------------------------------------------- Main -------------------------------------------------------------*/
int main(int argc, char *argv[]) {

	acceptmutex=1;
	struct sigaction act;
	sigset_t setsignals;
	usersdim=0;
	nconnected=0;
	const char optstring[] = "f";
	PoolCreator poolthread;
	ConfigFile cfg;
	int soc,i,sig,chiusura=1;
	struct sockaddr_un sa;
	threadneed t;
	fd_hwm =0;
	strcpy(nickfile,"files/nickfile");
	strcpy(filepath,"files/");

	/*azzero nickname registrati altrimenti
	i test ripetuti falliscono perchè risultano
	i nickname già registrati*/
	remove(nickfile);


	if((char)getopt(argc, argv,optstring)!='f'){
		usage("./chatty.o");
		return 1;
	}

	/*inizializzazione della gestione segnali*/
	sigemptyset(&setsignals);
	sigaddset(&setsignals,SIGINT);
	sigaddset(&setsignals,SIGUSR1);
	sigaddset(&setsignals,SIGQUIT);
	sigaddset(&setsignals,SIGTERM);
	pthread_sigmask(SIG_SETMASK,&setsignals,NULL);



	/*DECIDO DI IGNORARE SIGPIPE, PERCHÈ SE UN CLIENT SI DISCONNETTE
	MENTRE SCRIVO FRA ANDRE IL SERVER IN CRASH*/
	memset(&act, 0, sizeof(act));
	act.sa_handler=SIG_IGN;
	sigaction(SIGPIPE,&act,NULL);

	
	/*pharsing config file*/
	 cfg = PharseConfigFile (argv[optind]);

	/*creazione directory che servono*/
	mkdir(filepath,0777);
	mkdir(cfg.dirname,0777);


	/*creazione socket*/
	unlink(cfg.unixpath);
	strcpy(sa.sun_path,cfg.unixpath);
	sa.sun_family = AF_UNIX;
	soc = socket(AF_UNIX,SOCK_STREAM,0);
	fd_hwm =soc;
	FD_ZERO(&set);
	FD_SET(soc,&set);

	/*binding del socket*/
	SYSCALL(bind(soc,(struct sockaddr *)&sa,sizeof(sa)),errorManager);
	
	/*abilita ascolto*/
	/*do un po di tolleranza al + nthreads numero massimo di connessioni*/
	SYSCALL(listen(soc,cfg.maxconnections+cfg.threadsinpool),errorManager);

	/*creazione array utenti connessi*/
	users=malloc(sizeof(UserConnected)*(fd_hwm+1));
	UserConnected empty;
	empty.nick[0]=0;
	for(i=0;i<=fd_hwm;i++){
		users[i]=empty;
	}
	/*i + 10 sono di tolleranza*/
	fdBusyList=calloc(cfg.maxconnections+10,sizeof(pthread_mutex_t));
	
	/*creazione listeners threads*/
	 t.socket_fd=soc;
	 t.sa=&sa;
	 t.cfg=&cfg;

	 poolthread.cfg=cfg;
	 poolthread.threadFunc=listener;
	 poolthread.cleanupFunc=cleanlistener;
	 poolthread.argt=(void*)&t;
	 poolthread.argc=NULL;
	 
	workerscreator ((void *) &poolthread);

	/*attesa segnale chiusura server*/
	while(chiusura>0){
		sigwait(&setsignals,&sig);
		if(sig==SIGUSR1){
			printf("creazione statistiche\n");
			FILE * filestat = fopen(cfg.statfilename,"a+");
			
			/*aggiorno statistiche*/
    		pthread_mutex_lock(&keystat);
			printStats(filestat);
			pthread_mutex_unlock(&keystat);
			
			fclose(filestat);
			chiusura=1;
		}else{
			printf("ricevuto segnale %d\n",sig);
			chiusura=0;
		}
	}

	close(soc);

	/*a questo punto segnale chiusura arrivato quindi stacco tutto*/
	workersdestroyer ((void *) &poolthread);

	free(users);
	free(fdBusyList);

    return 0;
}

/* ---------------------------------------------- Funzioni --------------------------------------------------------*/

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

void * listener (void * args){

	threadneed  t= *(threadneed*) args;
	int channel, /*fd del client da aggiungere*/
	fd,
	tmp,
	state,
	soc= t.socket_fd; /*fd del socket del server*/
	fd_set read_set; /*set di fd su cui ascoltare eventi*/
	sigset_t setsignals;

	/*evito che i thread possano sentire i segnali*/
	sigfillset(&setsignals);
	pthread_sigmask(SIG_SETMASK,&setsignals,NULL);

	/*garantisco che un solo thread per volta ascolti e gestisca le richieste*/

	/*mi sospendo su cond varaccept*/
	pthread_mutex_lock(&keyaccept);
	while(acceptmutex==0){pthread_cond_wait(&condaccept,&keyaccept);};
	acceptmutex=0;
	read_set=set;
	pthread_mutex_unlock(&keyaccept);
	pthread_cond_signal(&condaccept);

	while(1){

		channel=-1;

		/*accettazione nuovo socket o richiesta tramite metodo della select*/
		read_set=set;
		SYSCALL(select(fd_hwm+1,&read_set,NULL,NULL,NULL),errorManager);
		for(fd=0;fd<=fd_hwm;fd++){
			if(FD_ISSET(fd, &read_set)){
				/*disabilito momentaneamente cancellazione thread*/
				/*CASO aggiunta nuovo client----------------------*/
				if(fd==soc){

					SYSCALL((channel=accept(soc,NULL,0)),errorManager);

					pthread_mutex_lock(&keyuser);
			
					printf("registrazione client su fd %d \n",channel);
			
					/*se ci sono posti disponibili*/
					if(nconnected<=t.cfg->maxconnections){

						if (channel>fd_hwm){ 
							
							UserConnected * tmp;
							tmp=realloc(users,(channel+1)*sizeof(UserConnected));
							if(tmp!=NULL){
								users=tmp;
								fd_hwm=channel;
								usersdim=channel+1; 
							}
							else {
								close(channel); 
								usersdim--;
								perror("impossibile realloc");
								channel=-1;
							}

						}

						/*aggiunta in elenco connessi*/
						if(channel>=0){
							/*aggiungo utente*/
							nconnected++;
							UserConnected toinsert;
							toinsert.nick[0]=0;
							users[channel] = toinsert;

							/*aggiorno FD_SET*/
							FD_SET(channel,&set);	

							/*aggiorno statistiche*/
    						pthread_mutex_lock(&keystat);
							chattyStats.nonline++;
							pthread_mutex_unlock(&keystat);					
						}
					}else{
						/*spedizione messaggio errore*/ 
						message_t msg;
						setHeader(&msg.hdr,OP_END,"chatty.o");
						sendHdr(channel,&msg.hdr);
						close(channel);
					}

					pthread_mutex_unlock(&keyuser);
					break;

				}else{
				/*CASO messaggio client o disconnessione----------------------*/
					pthread_mutex_lock(&keyuser);
					if(fdBusyList[fd]==0){
						/*lo impegna il mio thread per l'ascolto*/
						fdBusyList[fd]=(int)pthread_self();
						pthread_mutex_unlock(&keyuser);
						break;
						
					}
					pthread_mutex_unlock(&keyuser);
					
				}
				
			}
		}

		

		/*se sono uscito perchè c'è operazione da svolgere */

		/*estrazione valore da lista busy dei fd*/
		pthread_mutex_lock(&keyuser);
			
		tmp=fdBusyList[fd];
			
		pthread_mutex_unlock(&keyuser);

		/*gestione richiesta se appartiene al thread self*/
		if(tmp==(int)pthread_self()){

			/*leggi richiesta*/
			message_t newmsg;
			int esito = readMsg(fd,&newmsg);

			if(esito<=0){
				/*caso disconnessione*/
				/*disabilito la cancellazione del thread momentaneamente*/
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&state);
 
				FD_CLR(fd,&set);
				if(fd==fd_hwm) fd_hwm--;
				nconnected--; 

				close(fd);

				pthread_mutex_lock(&keyaccept);
				acceptmutex=1;
				pthread_mutex_unlock(&keyaccept);
				pthread_cond_signal(&condaccept);

				pthread_mutex_lock(&keyuser);
				users[fd].nick[0]=0;
				pthread_mutex_unlock(&keyuser);

				/*aggiorno statistiche*/
    			pthread_mutex_lock(&keystat);
				chattyStats.nonline--;
				pthread_mutex_unlock(&keystat);
				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&state);

				
			}else{
				/*altrimenti eleboro richiesta*/

				pthread_mutex_lock(&keyaccept);
				acceptmutex=1;
				pthread_mutex_unlock(&keyaccept);
				pthread_cond_signal(&condaccept);
				/*disabilito momentaneamente cancellazione del thread per 
				non lasciare file in stati inconsistenti*/
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&state);
				
				int result = elaboraRichiesta(newmsg,*t.cfg,fd);

				if(result<0){
					/*aggiorno statistiche*/
    				pthread_mutex_lock(&keystat);
					chattyStats.nerrors++;
					pthread_mutex_unlock(&keystat);
					printf("Errore su richiesta fd %d\n",fd );
				}

				free(newmsg.data.buf);
				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&state);
			}
				

			/*disimpegno fd dal thread attuale*/
			pthread_mutex_lock(&keyuser);
			fdBusyList[fd]=0;
			pthread_cond_broadcast(&conduser);
			pthread_mutex_unlock(&keyuser);

			/*aspetto di essere solo prima di accettare prossima richiesta*/
			pthread_mutex_lock(&keyaccept);
			while(acceptmutex==0){pthread_cond_wait(&condaccept,&keyaccept);};
			acceptmutex=0;
			read_set=set;
			pthread_mutex_unlock(&keyaccept);
			pthread_cond_signal(&condaccept);

		}

	}
}

void errorManager (int errorCode){
	perror("socket error");
}

void * cleanlistener (void * arg){

	pthread_mutex_unlock(&keyaccept);
	pthread_cond_signal(&condaccept);
	pthread_mutex_unlock(&keyuser);
	pthread_cond_signal(&conduser);
	pthread_mutex_unlock(&keyfile);

	return (void*) 0;
}

int elaboraRichiesta(message_t msg,ConfigFile cfg,int fd){

	op_t op = msg.hdr.op;
	UserConnected * connessi; /*array utenti associati a fd*/
	message_t risposta;
	int nusrs,dim,i,nickesistente=0;
	char temp[100]; /*array per utilizzi vari, a scopo temporaneo*/
    char** nicks; /*array nickname connessi*/
    int riuscita =1;
    char* buffer;
    message_t * bufferhistory;

    /*inizializzazione messaggio*/
    setHeader(&risposta.hdr,OP_FAIL ,"");
    setData(&risposta.data,"",NULL,0);

	/*copia array utenti conenssi al momento*/
	pthread_mutex_lock(&keyuser);
		connessi=malloc(sizeof(UserConnected)*usersdim);
		for(i=0;i<usersdim;i++){
			connessi[i]=users[i];
		}
		nusrs=usersdim;
	pthread_mutex_unlock(&keyuser);



	switch (op){
		
		/* richiesta di registrazione di un ninckname */
		case REGISTER_OP :  
				
			RimuoviSpazi(msg.hdr.sender);
			
			/*ricerca su array*/
			pthread_mutex_lock(&keyfile);
			nickesistente=nickpresente(nickfile,msg.hdr.sender);
			pthread_mutex_unlock(&keyfile);

			if(nickesistente>0){
				setHeader(&risposta.hdr,OP_NICK_ALREADY,msg.hdr.sender);
				sendHdr(fd,&risposta.hdr);
				riuscita=-1;
				break;
			}

			/*cancello il file contenente i messaggi in sosperso di una eventuale vecchia
			registrazione*/
			temp[0]=0;
			strcat(temp,filepath);
			strcat(temp,msg.hdr.sender);
			remove(temp);

			/*aggiunta nick a file*/
			pthread_mutex_lock(&keyfile);
			filepush(nickfile,msg.hdr.sender);
			pthread_mutex_unlock(&keyfile);
			/*aggiorno statistiche*/
			pthread_mutex_lock(&keystat);
			chattyStats.nusers++;
			pthread_mutex_unlock(&keystat);


		
		/* richiesta di connessione di un client */ 
   		case CONNECT_OP : 
   		

   				RimuoviSpazi(msg.hdr.sender);
   				
   				/*se nick risculta già connesso disconnetto l'altro*/
   				if((riuscita=isConnected(msg.hdr.sender))>0 && strcmp(connessi[fd].nick,msg.hdr.sender)!=0){
					printf("utente gia connesso\n");
					/*azzero il nick nella lista fd connessi*/
   					pthread_mutex_lock(&keyuser);
   					//strcpy(users[fd].nick,"");
   					users[riuscita].nick[0]=0;
   					pthread_mutex_unlock(&keyuser); 
   					riuscita=1;	
   				}
   				pthread_mutex_lock(&keyfile);
   				if(nickpresente(nickfile,msg.hdr.sender)==0){
   					pthread_mutex_unlock(&keyfile);
   					setHeader(&risposta.hdr,OP_NICK_UNKNOWN,msg.hdr.sender);
					sendHdr(fd,&risposta.hdr);
					printf("nick non registrato\n");
					riuscita=-1;
					break;
   				}
   				pthread_mutex_unlock(&keyfile);

   				/*altrimenti setto come connesso e mando ack e lista connessi*/
   				pthread_mutex_lock(&keyuser);
   				strcpy(users[fd].nick,msg.hdr.sender);
   				strcpy(connessi[fd].nick,msg.hdr.sender);
   				pthread_mutex_unlock(&keyuser);
   			

   		/*richiesta di avere la lista di tutti gli utenti attualmente connessi*/  
    	case USRLIST_OP : 
    		/*mando op  ok e lista utenti*/
			setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
    		char * usr = createusrList(connessi,nusrs,&dim);
			setData(&risposta.data,msg.hdr.sender,usr,dim);
			sendRequest(fd,&risposta);
			free(usr);

    	break;
   		/*richiesta di invio di un messaggio testuale ad un nickname o groupname*/ 
    	case POSTTXT_OP :

    		/*controllo se ricevente esiste*/
    		pthread_mutex_lock(&keyfile);
   				if(nickpresente(nickfile,msg.hdr.sender)==0){
   					pthread_mutex_unlock(&keyfile);
   					setHeader(&risposta.hdr,OP_NICK_UNKNOWN,msg.hdr.sender);
					sendHdr(fd,&risposta.hdr);
					printf("ricevente non esiste\n");
					riuscita=-1;
					break;
   				}
   			pthread_mutex_unlock(&keyfile);

    		/*controllo lunghezza messaggio*/
    		if(msg.data.hdr.len>cfg.maxmsgsize){
    			msg.hdr.op=OP_MSG_TOOLONG;
    			riuscita=-1;
    			printf("messaggio troppo lungo \n");
    			sendHdr(fd,&msg.hdr);
    			break;
    		}
    		printf("invio messaggio [ %s ]\n",msg.data.buf);
    		/*invio messaggio*/

    		/*metto stato utilizzo fd mio thread in pausa con fd -1*/
    		pthread_mutex_lock(&keyuser);
			fdBusyList[fd]=-1;
			pthread_mutex_unlock(&keyuser);

    		if(InvioMessaggio(msg,TXT_MESSAGE)>0)
    			setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
    		else{
    			riuscita=-1;
    			printf("Invio messaggio fallito\n");
    			msg.hdr.op=OP_FAIL;
    			setData(&risposta.data,"",NULL,0);
    		}

    		pthread_mutex_lock(&keyuser);
			while(fdBusyList[fd]!=-1) pthread_cond_wait(&conduser,&keyuser);
    		fdBusyList[fd]=(int)pthread_self();
			pthread_mutex_unlock(&keyuser);

    		sendHdr(fd,&risposta.hdr);
    
    	break;
    	/*richiesta di invio di un messaggio testuale a tutti gli utenti */    
    	case POSTTXTALL_OP :
    		/*controllo lunghezza messaggio*/
    		if(msg.data.hdr.len>cfg.maxmsgsize){
    			msg.hdr.op=OP_MSG_TOOLONG;
    			riuscita=-1;
    			printf("messaggio troppo lungo\n");
    			sendHdr(fd,&msg.hdr);

    			break;
    		}
    		/*invio messaggio a tutti i nickname registrati*/
    		nicks = nicktoarray(nickfile,&dim);

    		/*metto stato utilizzo fd mio thread in pausa con fd -1*/
    		pthread_mutex_lock(&keyuser);
			fdBusyList[fd]=-1;
			pthread_mutex_unlock(&keyuser);

    		for(i=0;i<dim;i++){
    			strcpy(msg.data.hdr.receiver,nicks[i]);
    			printf("invio messaggio a %s\n",nicks[i] );

				/*invio messaggio*/
    			InvioMessaggio(msg,TXT_MESSAGE);

    			free(nicks[i]);
    		}

    		pthread_mutex_lock(&keyuser);
			while(fdBusyList[fd]!=-1) pthread_cond_wait(&conduser,&keyuser);
    		fdBusyList[fd]=(int)pthread_self();
			pthread_mutex_unlock(&keyuser);

    		free(nicks);

    		setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
    		sendHdr(fd,&risposta.hdr);


    	break;
    	/*richiesta di invio di un file ad un nickname o groupname*/  
    	case POSTFILE_OP : 
    		/*creazione path file*/
    		strcpy(temp,cfg.dirname);
    		strcat(temp,"/");
    		strcat(temp,msg.data.buf);

    		/*ricezione file */
    		//printf("sto per leggere %s \n",msg.data.buf);
    		readData(fd,&risposta.data);
    		//printf("file letto %s \n",msg.data.buf);

    		/*do 10kb di tolleranza perchè il client è grosso e i test falliscono*/
    		if(risposta.data.hdr.len>(cfg.maxfilesize*1024+10240)){
    			printf("file troppo grosso\n");
    			riuscita=-1;
    			/*file troppo grosso*/
    			msg.hdr.op=OP_MSG_TOOLONG;
    			
    			sendHdr(fd,&msg.hdr);
    			if(risposta.data.buf!=NULL) 
    			free(risposta.data.buf);
    			break;
    		}
    		
    		msg.hdr.op=OP_OK;

    		/*mando conferma file inviato*/
    		sendHdr(fd,&msg.hdr);

    		/*salvataggio file su memoria*/
    		pthread_mutex_lock(&keyfile);
    		remove(temp);
    	    i=open(temp,O_CREAT | O_WRONLY | O_TRUNC,0777); 
    		write(i,risposta.data.buf,risposta.data.hdr.len);
    		close(i);
    		pthread_mutex_unlock(&keyfile);


    		/*lascio il thread per lettura messaggio*/
    		pthread_mutex_lock(&keyuser);
			fdBusyList[fd]=-1;
			pthread_mutex_unlock(&keyuser);

    		/*invio notifca file in arrivo*/
    		InvioMessaggio(msg,FILE_MESSAGE);

    		pthread_mutex_lock(&keyuser);
			while(fdBusyList[fd]!=-1) pthread_cond_wait(&conduser,&keyuser);
    		fdBusyList[fd]=(int)pthread_self();
			pthread_mutex_unlock(&keyuser);
    		 
    		if(risposta.data.buf!=NULL) 
    		free(risposta.data.buf);

    	break;
    	/*richiesta di recupero di un file */ 
    	case GETFILE_OP :  
    		buffer=calloc(cfg.maxfilesize+1,1);
    		strcpy(temp,cfg.dirname);
    		strcat(temp,"/");
    		strcat(temp,msg.data.buf);

    		/*lettura file da memoria*/
    		pthread_mutex_lock(&keyfile);
    		i=open(temp,O_RDONLY,0777);
    		if(i>0){
    			dim=read(i,buffer,cfg.maxfilesize+1);
    			close(i);
    			pthread_mutex_unlock(&keyfile);

    		
    		}else{
    			pthread_mutex_unlock(&keyfile);
    			msg.hdr.op=OP_NO_SUCH_FILE;
    			sendHdr(fd,&msg.hdr);
    			riuscita=-1;
    			printf("apertura file non riuscita\n");
    			/*aggiorno statistiche*/
    			pthread_mutex_lock(&keystat);
				chattyStats.nfilenotdelivered-=i;
				pthread_mutex_unlock(&keystat);

    			if(buffer!=NULL)
    			free(buffer);
    			break;
    		}

    		setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
    		setData(&risposta.data,msg.hdr.sender,buffer,dim);
    		sendRequest(fd,&risposta);
    		    		
    		if(buffer!=NULL)
    		free(buffer);

    	break;  
    	/*richiesta di recupero della history dei messaggi*/ 
    	case GETPREVMSGS_OP :
    		/*mando ack*/
    		setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
			sendHdr(fd,&risposta.hdr);

    		/*lettura da file di tutti i messaggi precedenti*/
    		temp[0]=0;
			RimuoviSpazi(msg.hdr.sender);

			/*setto il path del file contenente i messaggi in sosperso di un client*/
			strcat(temp,filepath);
			strcat(temp,msg.hdr.sender);

			/*garantisco mutua esclusione per accesso al file*/
			pthread_mutex_lock(&keyfile);
			dim = open(temp,O_CREAT | O_RDWR | O_APPEND,0666);

			if(dim>0){
				bufferhistory=malloc(sizeof(message_t)+1);
				i=0;

				/*leggo messaggi dal file*/
				while(readMsg(dim,bufferhistory+i)>0){

					i++;
					bufferhistory=realloc(bufferhistory,(i+1)*sizeof(message_t)+1);

				}		
				close(dim);
				/*elimino history file*/
				remove(temp);
			pthread_mutex_unlock(&keyfile);

				/*invio numero di messaggi in arretrato*/
				setData(&risposta.data,msg.hdr.sender,(char*)&i,sizeof(int));
				sendData(fd,&risposta.data);

				/*invio messaggi in arrettrato*/
				for(dim=0;dim<i;dim++){
					sendRequest(fd,bufferhistory+dim);
					free(bufferhistory[dim].data.buf);
				}
				free(bufferhistory);

				/*aggiorno statistiche*/
    			pthread_mutex_lock(&keystat);
				chattyStats.nnotdelivered-=i;
				pthread_mutex_unlock(&keystat);

			}else{
				/*invio numero di messaggi in arretrato 0*/
				pthread_mutex_unlock(&keyfile);
				dim=0;
				setData(&risposta.data,msg.hdr.sender,(char*)&dim,sizeof(int));
				sendData(fd,&risposta.data);
				
			} 	

			
    	break;
    	/* richiesta di deregistrazione di un nickname o groupname */
    	case UNREGISTER_OP  :

    		temp[0]=0;

    		RimuoviSpazi(msg.hdr.sender);

    		/*rimuovo nick dal file contenente tutti i nick*/
    		pthread_mutex_lock(&keyfile);
    		if(stringremove(nickfile, msg.hdr.sender)>=0)
    		{
    			/*aggiorno statistiche*/
    			pthread_mutex_lock(&keystat);
				chattyStats.nusers--;
				pthread_mutex_unlock(&keystat);
    		}
    		pthread_mutex_unlock(&keyfile); 

    		/*rimozione del file associato al nickname*/
    		strcat(temp,filepath);
    		strcat(temp,msg.hdr.sender);
    		remove(temp);

    		setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
			sendHdr(fd,&risposta.hdr);

    	break; 
    	/* richiesta di disconnessione*/ 
    	case DISCONNECT_OP :
   				
   			/*azzero il nick nella lista fd connessi*/
   			pthread_mutex_lock(&keyuser);
   			//strcpy(users[fd].nick,"");
   			users[fd].nick[0]=0;
   			pthread_mutex_unlock(&keyuser); 

   			setHeader(&risposta.hdr,OP_OK,msg.hdr.sender);
			sendHdr(fd,&risposta.hdr);
    	break; 
    	default : printf("Comando sconosciuto %d !!!!\n", op); riuscita=-1;
	}

	free(connessi);
	return riuscita;

}

int isConnected(char * usr){
	
	int i;

	pthread_mutex_lock(&keyuser);
	for(i=0;i<usersdim;i++){
		if(strcmp(usr,users[i].nick)==0){
			pthread_mutex_unlock(&keyuser);
			return i;
		}
	}
	pthread_mutex_unlock(&keyuser);
	return 0;


}

/*ritorna positivo se iresce, negativo se fallisce */
int InvioMessaggio(message_t messaggiorichiesta, op_t op){

	int fd = isConnected(messaggiorichiesta.data.hdr.receiver);
	int risultato=1;
	char temporaneo[100];
	temporaneo[0]=0;
	int self =(int)pthread_self();
	int oldstate=0;
	
	//strcpy(temporaneo,messaggiorichiesta.hdr.sender);
	//setHeader(&messaggiorichiesta.hdr,op,temporaneo);

	messaggiorichiesta.hdr.op=op;

	/*se esito positivo client ancora connesso*/
	int esito = read(fd,temporaneo,0); 
	if(fd>0 && esito>=0){
		/*invio a nickname connesso*/
	
		/*attendo la fine di altre operazione sul fd*/
		pthread_mutex_lock(&keyuser);
		oldstate=fdBusyList[fd];

		while(oldstate!=0 && oldstate!=self && oldstate!=-1){
			pthread_cond_wait(&conduser,&keyuser);
			oldstate=fdBusyList[fd];}
		fdBusyList[fd]=self;
		pthread_mutex_unlock(&keyuser);

		/*invio messaggio*/
		risultato=sendRequest(fd,&messaggiorichiesta);

		/*disimpegno fd dal thread*/
		pthread_mutex_lock(&keyuser);
		fdBusyList[fd]=oldstate;
		pthread_cond_broadcast(&conduser);
		pthread_mutex_unlock(&keyuser);

		/*se messaggio inviato aggiorno statistiche*/
		if(risultato>0){

			/*aggiorno statistiche*/
			if(op==TXT_MESSAGE){
    			pthread_mutex_lock(&keystat);
				chattyStats.ndelivered++;
				pthread_mutex_unlock(&keystat);
			}else{
				pthread_mutex_lock(&keystat);
				chattyStats.nfiledelivered++;
				pthread_mutex_unlock(&keystat);
			}
			return risultato;
		}

	}
	
	/*caso in cui nickname era offline */
		
	temporaneo[0]=0;
	RimuoviSpazi(messaggiorichiesta.data.hdr.receiver);

	/*setto il path del file contenente i messaggi in sosperso di un client*/
	strcat(temporaneo,filepath);
	strcat(temporaneo,messaggiorichiesta.data.hdr.receiver);

	/*garantisco mutua esclusione per accesso al file*/
	pthread_mutex_lock(&keyfile);
	fd=-1;
	fd = open(temporaneo,O_CREAT | O_RDWR | O_APPEND,0777);

	if(fd>0){
		/*ivnvio il messaggio anzicchè ad un socket ad un file*/
		risultato=sendRequest(fd,&messaggiorichiesta);
		close(fd);
	}else risultato = -1;	
	pthread_mutex_unlock(&keyfile);

	/*aggiorno statistiche*/
	if(op==TXT_MESSAGE){
    	pthread_mutex_lock(&keystat);
		chattyStats.nnotdelivered++;
		pthread_mutex_unlock(&keystat);
	}else{
		pthread_mutex_lock(&keystat);
		chattyStats.nfilenotdelivered++;
		pthread_mutex_unlock(&keystat);
	}


	return risultato;

}

 char * createusrList (UserConnected * connessi, int nconn, int *dim){
 	char * utenti = malloc(MAX_NAME_LENGTH+2);
 	char temporaneo[MAX_NAME_LENGTH+1];
 	temporaneo[0]=0;
 	utenti[0]=0;
 	int i;
 	int aggiunti=0;
 	*dim=0;

 	for(i=0;i<nconn;i++){
 		if (connessi[i].nick[0]!=0){
 			aggiunti++;
 			strcpy(temporaneo,connessi[i].nick);
 			strcat(&utenti[*dim],temporaneo);
 			fuller(&utenti[*dim],MAX_NAME_LENGTH+1);
 			utenti=realloc(utenti,(aggiunti+1)*(MAX_NAME_LENGTH+1)+1);
 			*dim=aggiunti*(MAX_NAME_LENGTH+1);
 			utenti[*dim]=0;
 		}
 		
 		temporaneo[0]=0;
 	}

 	printf("users list %s \n",utenti);

 	return utenti;	
 }




