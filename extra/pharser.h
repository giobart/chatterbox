/** \file pharser.h
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

/* PHARSER CHATTY */


#ifndef pharser_h
#define pharser_h

#define PHARSERBUF 200
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*----------------------------------------------- Prototipi --------------------------------------------------------*/

/*
* @struct rappresenta tutte le variabili prese dal file di configurazione
*/
typedef struct cfg { 
	/*path utilizzato per la creazione del socket AF_UNIX*/
	/*ipotizzo sempre path di lunghezza inferiore a 200 caratteri*/
	char unixpath[200];
	/*numero massimo di connessioni concorrenti gestite dal server*/
	int maxconnections;
	/*numero di thread nel pool*/
	int threadsinpool;
	/*dimensione massima di un messaggio testuale (numero di caratteri)*/
	int maxmsgsize;
	/* dimensione massima di un file accettato dal server (kilobytes) */
	int maxfilesize;
	/*numero massimo di messaggi che il server ’ricorda’ per ogni client*/
	int maxhistmsgs;
	/*directory dove memorizzare i files da inviare agli utenti*/
	char dirname[200];
	/*file nel quale verranno scritte le statistiche*/
	char statfilename[200];
}	ConfigFile;

/**
* @function PharseConfigFile
*
* @brief effettua il pharsing del file di configurazione del server
*
* @var path il percorso del file
*
* @return struttura contenente tutto
*
*/
ConfigFile PharseConfigFile (char * path);

/**
* @function KeywordContent
*
* @brief ritorna il valore associato ad una Keyword del file
*
* @var keyword parola chiave da cercare
*
* @var path percorso del file dove cercare
*
* @contentc se ==NULL trasforma risultato in intero e lo mette in contenti altirmenti mette il contenuto qui
*
* @contentii se contentc==NULL qui c´è il risultato
*
* @return <=0 fallimento
*
*/
int KeywordContent (char * keyword, char * path, char * contentc, int * icontent);

/**
* @function stranalyze
*
* @brief estrapola da una stringa il valore dopo = di una keyword
*
* @var str stringa dove cercare
*
* @var keyword parola chiave da cercare
*
* @var content dove mette il risultato
*
* @return <=0 fallimento
*
*/
int stranalyze (char * str, char * keyword, char * content);


/**
* @function RimuoviSpazi
*
* @brief elimina spazi e caratteri speciali da una stringa
*
* @var str stringa bersaglio
*
* @return <=0 fallimento
*
*/
void RimuoviSpazi(char* str);



#endif