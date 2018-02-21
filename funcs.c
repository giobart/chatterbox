/** \file funcs.c  
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

/*--------tutte le funzioni delle interfacce da me definite--------*/

#include <message.h>
#include <extra/pharser.h>
#include <pthread.h>
#include <extra/workers.h>
#include <connections.h>
#include <extra/fileoperations.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <extra/pharser.h>
#include <string.h>
#include <errno.h>
#include <config.h>
#include <string.h>
#include <extra/users.h>
#include <sys/un.h>
#include <sys/socket.h>

/*variabili globali*/
static pthread_mutex_t keythread_t= PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condthread_t = PTHREAD_COND_INITIALIZER;

pthread_t * workersArray;
int thread_t_closed;

/*------------- Funzioni ------------------*/

/* da completare da parte dello studente con eventuali altri metodi di interfaccia */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
	
	struct sockaddr_un sa;
	int tentativo=1;
	int res=-1;
	
	/*inizializzazioni*/
	strcpy(sa.sun_path,path);
	sa.sun_family = AF_UNIX;

	/*avvio socket client */
	int soc=socket(AF_UNIX,SOCK_STREAM,0);

	while((res=connect(soc,(struct sockaddr *)&sa,sizeof(sa)))==-1 && tentativo<ntimes){
		sleep(secs);
		tentativo++;
		
	}

	if(res>=0)	
		return soc;
	else return res;
}

int sendRequest(long fd, message_t *msg){

	//char * tmp;
	char name[MAX_NAME_LENGTH+1];
	int retval=1;

	message_hdr_t hdr;
	message_data_t data;
	message_data_hdr_t datahdr;

	hdr=msg->hdr;
	data=msg->data;
	datahdr=data.hdr;

	//tmp=malloc(sizeof(int)+1);

	/*
	protocollo di scrittura :
	SENDER 		| dim : MAX_NAME_LEGTH
	OP 			| dim : sizeof(op_t)
	RECEIVER 	| dim : MAX_NAME_LENGTH
	LEN 		| dim : sizeof(int)
	BUF 		| dim : LEN
	*/
	if(hdr.sender!=NULL)
	strcpy(name,hdr.sender);
	else name[0]=0;
	fuller(name,MAX_NAME_LENGTH);
	ErrorHandlerWriteRead(write(fd,name,MAX_NAME_LENGTH),MAX_NAME_LENGTH,&retval);

	ErrorHandlerWriteRead(write(fd,(char*)&(hdr.op),sizeof(op_t)),sizeof(op_t),&retval);
	
	if(datahdr.receiver!=NULL)
		strcpy(name,datahdr.receiver);
	else name[0]=0;
	fuller(name,MAX_NAME_LENGTH);
	ErrorHandlerWriteRead(write(fd,name,MAX_NAME_LENGTH),MAX_NAME_LENGTH,&retval);
	
	//sprintf(tmp, "%d",(int)datahdr.len);
	//fuller(tmp,sizeof(int)+1);
	ErrorHandlerWriteRead(write(fd,(char*)&(datahdr.len),sizeof(int)),sizeof(int),&retval);

	if(datahdr.len!=0){
		//fuller(data.buf,datahdr.len);
		ErrorHandlerWriteRead(write(fd,data.buf,datahdr.len),1,&retval);
	}

	return retval;

}

int sendData(long fd, message_data_t *msg){
	
	char name[MAX_NAME_LENGTH+1];
	int retval=1;

	message_data_t data;
	message_data_hdr_t datahdr;

	data=*msg;
	datahdr=data.hdr;

	if(datahdr.receiver!=NULL)
		strcpy(name,datahdr.receiver);
	else name[0]=0;
	fuller(name,MAX_NAME_LENGTH);
	ErrorHandlerWriteRead(write(fd,name,MAX_NAME_LENGTH),MAX_NAME_LENGTH,&retval);
	
	//sprintf(tmp, "%d",(int)datahdr.len);
	//fuller(tmp,sizeof(int)+1);
	ErrorHandlerWriteRead(write(fd,(char*)&(datahdr.len),sizeof(int)),sizeof(int),&retval);

	if(datahdr.len!=0){
		//fuller(data.buf,datahdr.len);
		ErrorHandlerWriteRead(write(fd,data.buf,datahdr.len),1,&retval);
	}

	return retval;

}

int sendHdr(long fd, message_hdr_t *msg){

	char name[MAX_NAME_LENGTH+1];
	int retval=1;

	message_hdr_t hdr;
	
	hdr=*msg;


	/*
	protocollo di scrittura :
	SENDER 		| dim : MAX_NAME_LEGTH
	OP 			| dim : sizeof(op_t)
	RECEIVER 	| dim : MAX_NAME_LENGTH
	LEN 		| dim : sizeof(int)
	BUF 		| dim : LEN
	*/
	if(hdr.sender!=NULL)
	strcpy(name,hdr.sender);
	else name[0]=0;
	fuller(name,MAX_NAME_LENGTH);
	ErrorHandlerWriteRead(write(fd,name,MAX_NAME_LENGTH),MAX_NAME_LENGTH,&retval);

	ErrorHandlerWriteRead(write(fd,(char*)&(hdr.op),sizeof(op_t)),sizeof(op_t),&retval);

	return retval;
}

int readMsg(long fd, message_t *msg){

	/*protocollo di lettura :
	SENDER 		| dim : MAX_NAME_LEGTH
	OP 			| dim : sizeof(op_t)
	RECEIVER 	| dim : MAX_NAME_LENGTH
	LEN 		| dim : sizeof(int)
	BUF 		| dim : LEN
	*/
	
	char * sender = calloc(MAX_NAME_LENGTH+1,1);
	char * receiver = calloc(MAX_NAME_LENGTH+1,1);
	int retval = 1;
	msg->data.hdr.len=0;


	ErrorHandlerWriteRead(read(fd,sender,MAX_NAME_LENGTH),1,&retval);
	strcpy(msg->hdr.sender,sender);

	if(retval>0)
	ErrorHandlerWriteRead(read(fd,(char*)&(msg->hdr.op),sizeof(op_t)),1,&retval);

	if(retval>0)
	ErrorHandlerWriteRead(read(fd,receiver,MAX_NAME_LENGTH),1,&retval);
	strcpy(msg->data.hdr.receiver,receiver);

	if(retval>0)
	ErrorHandlerWriteRead(read(fd,(char*)&(msg->data.hdr.len),sizeof(int)),1,&retval);

	if(msg->data.hdr.len==0) msg->data.buf=NULL;
	else{
		msg->data.buf=calloc(msg->data.hdr.len+1,1);
		ErrorHandlerWriteRead(read(fd,msg->data.buf,msg->data.hdr.len),msg->data.hdr.len,&retval);	
	}

	free(sender);
	free(receiver);
	
	
	return retval;

}

int readHeader(long connfd, message_hdr_t *hdr){

	/*protocollo di lettura :
	SENDER 		| dim : MAX_NAME_LEGTH
	OP 			| dim : sizeof(op_t)
	RECEIVER 	| dim : MAX_NAME_LENGTH
	LEN 		| dim : sizeof(int)
	BUF 		| dim : LEN
	*/
	
	char * sender = calloc(MAX_NAME_LENGTH+1,1);
	int retval = 1;

	ErrorHandlerWriteRead(read(connfd,sender,MAX_NAME_LENGTH),1,&retval);
	strcpy(hdr->sender,sender);


	if(retval>0)
	ErrorHandlerWriteRead(read(connfd,(char*)&(hdr->op),sizeof(op_t)),1,&retval);

	free(sender);
	return retval;
}

int readData(long fd, message_data_t *data){

		/*protocollo di lettura :
	SENDER 		| dim : MAX_NAME_LEGTH
	OP 			| dim : sizeof(op_t)
	RECEIVER 	| dim : MAX_NAME_LENGTH
	LEN 		| dim : sizeof(int)
	BUF 		| dim : LEN
	*/
	
	char * receiver = calloc(MAX_NAME_LENGTH+1,1);

	int retval = 1;
	data->hdr.len=0;

	ErrorHandlerWriteRead(read(fd,receiver,MAX_NAME_LENGTH),1,&retval);
	strcpy(data->hdr.receiver,receiver);


	if(retval>0)
	ErrorHandlerWriteRead(read(fd,(char*)&(data->hdr.len),sizeof(int)),1,&retval);


	if(data->hdr.len==0) data->buf=NULL; 
	else{
		data->buf=calloc(data->hdr.len+1,1);

		int letti=0,len=data->hdr.len,pos=0;
		while((letti=read(fd,data->buf+pos,len))!=len){ 
			len-=letti;
			pos+=letti;	

		}

	}


	free(receiver);
	
	return retval;
}

void ErrorHandlerWriteRead(int v1, int v2, int * v3 ){

	if(v1<v2){ 
		if(*v3){ 
		if(errno==EBADF) *v3=0; 
	 		*v3 =-1; }
	} 

}

void fuller (char * s,int size){

	int i;
	int can=0;
	for(i=0;i<size;i++){
		if(can) s[i]=0;
		else {
			if(s[i]==0) can=1;
		}
	}

}

/*-----------prototipi----------------*/

void rimuovizpazi(char* str)
{
  char* x = str;
  char* y = str;
  
  while(*y != 0)
  {
    *x = *y++;
    if(*x != ' ' && *x != 9 && *x != 10)
      x++;
  }
  *x = 0;
}


/*verifica se siste un elemento in un array di utenti*/
int exist(char * elem, UserConnected* arr, int dim){
	int i;

	for(i=0;i<dim;i++){
		if(strcmp(arr[i].nick,elem)==0) break;
	}

	if(i<dim) return i;
	
	return -1;
}

/*push string al file*/
int filepush(char * path,char * topush){
	FILE * ofp;
	ofp = fopen(path,"a+");

	if(ofp!=NULL){
		fprintf(ofp, "%s\n", topush);
		fclose(ofp);
		return 1;
	}

	return -1;
	
}

/*rimuove string da file*/
int stringremove(char * path, char * toremove){

	FILE * ofp,*ifp;
	ifp = fopen(path,"r");
	int i;
	int tot=0;
	

	if(ifp!=NULL){

		char tmp[100];
		char ** found = malloc (sizeof(char*));
		int ret=-1;

		while(fscanf(ifp, "%s", tmp)==1){
			if(strcmp(toremove,tmp)!=0){
				tot++;
				found=realloc(found,(tot+1)*sizeof(char*));
				found[tot-1]=malloc(100);
				strcpy(found[tot-1],tmp);
			}else ret=1;
		} 

		fclose(ifp);
		ofp = fopen(path,"w");

		for(i=0;i<tot;i++){
			fprintf(ofp, "%s\n", found[i]);
			free(found[i]);
		}

		fclose(ofp);

		free(found);

		return ret;	
	}

	return -1;

}
/*nick di un file in array*/
char ** nicktoarray (char * path, int * dim){
	FILE *ifp;
	ifp = fopen(path,"r");
	int tot=0;
	char ** found = malloc (sizeof(char*));

	found[0]=malloc(100);
	while(fscanf(ifp, "%s", found[tot])==1){
		tot++;
		found=realloc(found,(tot+1)*sizeof(char*));
		found[tot]=malloc(100);
	}

	free(found[tot]);
	*dim=tot;
	fclose(ifp);
	return found; 
}


/*nick presente*/
int nickpresente(char * path, char * nick){

	FILE * ifp;

	/*creo file se non esiste*/
	ifp = fopen(path,"a+");
	fclose(ifp);

	/*ora lo apro*/
	ifp = fopen(path,"r");

	char tmp[100];
	while(fscanf(ifp, "%s", tmp)==1){
		rimuovizpazi(tmp);
		if(strcmp(nick,tmp)==0){
			fclose(ifp);
			return 1;
		}
	}

	fclose(ifp);
	return 0;

}


/* ---------------------------------------------- Funzioni --------------------------------------------------------*/

ConfigFile PharseConfigFile (char * path){

	/*nel caso in cui qualcosa non sia presente nel file di configurazione si suppongono delle scelte di default
	tenendo conto di essere in una macchina dalle scarse prestazioni e con poca memoria a disposizione*/

	ConfigFile cf;
	int tmp;

	/*scelta di default exit failure*/
	if(KeywordContent("UnixPath",path,cf.unixpath,&tmp)==-1){ 
		perror("UnixPath non settata in config file"); 
		exit(EXIT_FAILURE); 
	}

	/*scelta di default 10*/
	if(KeywordContent("MaxConnections",path,NULL,&cf.maxconnections)==-1){
		cf.maxconnections=10;
	}

	/*scelta di default 5*/
	if(KeywordContent("ThreadsInPool",path,NULL,&cf.threadsinpool)==-1){
		cf.threadsinpool=3;
	}

	/*scelta di default 128*/
	if(KeywordContent("MaxMsgSize",path,NULL,&cf.maxmsgsize)==-1){
		cf.maxmsgsize=128;
	}

	/*scelta di default 128*/
	if(KeywordContent("MaxFileSize",path,NULL,&cf.maxfilesize)==-1){
		cf.maxfilesize=128;
	}

	/*scelta di default 3*/
	if(KeywordContent("MaxHistMsgs",path,NULL,&cf.maxhistmsgs)==-1){
		cf.maxhistmsgs=3;
	}

	/*scelta di default exit failure*/
	if(KeywordContent("DirName",path,cf.dirname,&tmp)==-1){
		perror("DirName non settata in config file"); 
		exit(EXIT_FAILURE); 
	}

	/*scelta di default 256*/
	if(KeywordContent("StatFileName",path,cf.statfilename,&tmp)==-1){
		perror("StatFileName non settata in config file"); 
		exit(EXIT_FAILURE); 
	}

	return cf;

}

int KeywordContent (char * keyword, char * path, char * contentc, int * icontent){

	int continuo=0,
		letti,
		totletti=0,
		found=-1;
	FILE * ifp;
	char 	* tmpbuf,
			*buf,
		 	content[200];

	buf=(char*) malloc ((PHARSERBUF+1)*sizeof(char));

	/*apertura file */
	if((ifp=fopen(path,"r"))==NULL){
		free(buf);
		return -1;
	}

	/*bufferizzare le righe*/
	while(fgets(buf,PHARSERBUF,ifp)!=NULL)
	{
		/*se riga letta del tutto setto che posso continuare*/
		if((letti=strlen(buf))<PHARSERBUF) continuo=1;

		/*inserisco riga letta in buffer temporaneo*/
		totletti+=letti;
		if(totletti>letti){
			tmpbuf=(char *)realloc(tmpbuf,totletti+2);
			/*caso in cui stringa troppso grossa per la memoria*/
			if (tmpbuf==NULL) {
				free(buf);
				free(tmpbuf); 
				fclose(ifp); 
				return -1;
			}
		}else{
			tmpbuf=(char*) malloc(sizeof(char)*(totletti+2));
			tmpbuf[0]=0;
		}

		tmpbuf=strcat(tmpbuf,buf);
		
		/*se riga bufferizzata del tutto procedo a scandire*/
		if(continuo==1){

			/*se trovo la keyword nella riga concludo*/
			if((found=stranalyze(tmpbuf,keyword,content))==1){
				free(tmpbuf);
				break;
			}
			free(tmpbuf);
			continuo=0;
			totletti=0;
			
		}
		
	}

	fclose(ifp);
	free(buf);

	/*se non trovo niente setto ERRNO, eltrimenti setto il risultato*/
	if(found!=1) errno=EINVAL; 
	else{
		if(contentc==NULL) *icontent=atoi(content);
		else strcpy(contentc,content);
	}

	return found; 
}

/*ricerca keyword nella stringa*/
int stranalyze (char * str, char * keyword, char * content){

	int lung = strlen(str);
	int i;
	char* foundkeyword ; 

	/*cerco il primo carattere*/
	for(i=0; i<lung;i++){
		/*se il primo carattere è un delimitatore di commento allora ignoro stringa*/
		if(str[i]=='#') return -1;
		if(str[i]!=' ') break;	 
	}

	foundkeyword = strtok(str,"=");
	strcpy(content,foundkeyword);
	RimuoviSpazi(content);

	/*se keyword trovata setto il contenuto altrimenti torno -1*/
	if(strcmp(keyword,content)==0){
		foundkeyword=strtok(NULL,"=");
		strcpy(content,foundkeyword);
		RimuoviSpazi(content);	
	}else return -1;

	return 1;
}

/*rimove spazi da stringa*/
void RimuoviSpazi(char* str)
{
  char* x = str;
  char* y = str;
  
  while(*y != 0)
  {
    *x = *y++;
    if(*x != ' ' && *x != 9 && *x != 10)
      x++;
  }
  *x = 0;

}
  /* ----------------------------------- funzioni -------------------*/
void * workerscreator (void * arg){

	PoolCreator pc = *(PoolCreator*) arg;
	ConfigFile cfg = pc.cfg;
	thread_t_closed=0;

	workersArray = (pthread_t *) malloc(cfg.threadsinpool*sizeof(pthread_t));
	int i;

	/*creazione workers*/
	for(i=0; i<cfg.threadsinpool; i++){
		CreateHandler(pthread_create((workersArray+i),NULL,worker,arg));
	}

	return (void*)1;
	
}

void * workersdestroyer (void * arg){
	PoolCreator pc = *(PoolCreator*) arg;
	int i;

	for(i=0; i<pc.cfg.threadsinpool; i++){
		CancelHandler(pthread_cancel(workersArray[i]));	
	}

	/*aspetto la rimozione dei thread*/

	pthread_mutex_lock(&keythread_t);
	while(pc.cfg.threadsinpool>thread_t_closed){pthread_cond_wait(&condthread_t,&keythread_t);}
	pthread_mutex_unlock(&keythread_t);

	/*aspetto il detach dell'ultimo thread*/
	sleep(1);

	free(workersArray);

	return (void*)1;

}

void * worker (void * arg){
	pthread_detach(pthread_self()); 
	PoolCreator pc = *(PoolCreator*) arg;

	pthread_cleanup_push(cleanup_handler,&pc);

	/*chiamata alla funzione del thread passata come parametro*/
	if(pc.threadFunc!=NULL) pc.threadFunc(pc.argt);

	pthread_cleanup_pop(0);

	return (void*)1;
}

void cleanup_handler (void * arg){

	PoolCreator cl = *(PoolCreator*) arg;

	if(cl.cleanupFunc!=NULL) cl.cleanupFunc(cl.argc);

	pthread_mutex_lock(&keythread_t);
		thread_t_closed++;
	pthread_cond_signal(&condthread_t);
	pthread_mutex_unlock(&keythread_t);
}