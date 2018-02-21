/** \file workers.h 
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

/*creazione e rimozione dei thread del server */

/*ho parametrizzato la creazione del pool di thread, così da facilitare la 
manutenzione.
si riceve in input quello che i workers devono fare e una funzione di cleanup da utilizzare
eventualmente*/
/*questo metodo permette di programmare liberamente il funzionamento del 
worker senza stare a preoccuparsi della gestione dei thread, infatti 
questa viene scritta e testata qui e si suppone che questo componente
del programma funzioni bene*/

#ifndef workers_h
#define workers_h

#include <extra/pharser.h>

#define CancelHandler(f) if(f!=0) perror("Thread not removed"); 
#define CreateHandler(f) if(f!=0) perror("Thread not created"); 


/* ----------------------------------- prototipi -------------------*/
/*struttura da passare come parametro a workerscreator e workersdestroyer*/
/*al suo interno passa il file di configurazione già pharsato, la 
funzione che ogni worker eseguirà e l'eventuale cleanup*/
typedef struct pool { 
	ConfigFile cfg;
	void * (*threadFunc)(void*);
	void * (*cleanupFunc)(void*);
	void * argt;
	void * argc;
}	PoolCreator;

/*thread worker che eseguirà la funzione passata per parametro*/
/**
* @function worker
*
* @brief rappresenta il singolo thread in esecuzione
*
* @var arg si aspetta una struttra PoolCreator contenente le funzioni da eseguire
*
*/
void * worker (void * arg);
/*crea i thread worker speficifati nel file di configurazione*/
/**
* @function workerscreator
*
* @brief crea un pool di workers che eseguiranno le funzioni nel parametro arg
*
* @var arg di tipo PoolCreator, contiene le informazioni sulle funzioni che i workers dovranno eseguire
*
*/
void * workerscreator (void * arg);
/*distrugge il numero di threads che sono stati creati grazie al 
file di configurazione*/
/**
* @function workersdestroyer
*
* @brief distrugge un pool di workers 
*
* @var arg di tipo PoolCreator, contiene le informazioni sulle funzioni che i workers dovranno eseguire
*
*/
void * workersdestroyer (void * arg);
/*funzione di cleanup che esegue il cleanup passato per parametro
e si occupa del rilascio della memoria allocata dai thread*/
/**
* @function cleanup_handler
*
* @brief clean_up eseguita alla distruzione di un worker, esegue la funzione di cleanup di PoolCreator
*
* @var arg di tipo PoolCreator, contiene le informazioni sulle funzioni che i workers dovranno eseguire
*
*/
void cleanup_handler (void * arg);





#endif