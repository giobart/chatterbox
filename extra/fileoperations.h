/** \file fileoperations.h 
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

#ifndef fileoperations_h
#define fileoperations_h

#include <string.h>
#include <extra/users.h>
#include <stdio.h>
#include <stdlib.h>



/*-----------prototipi----------------*/

/**
* @function rimuovizpazi
*
* @brief funzione che rimuove gli spazi e tabulazioni da una stringa
*
* @var str striga da cui rimuovere gli spazi
*
*/
void rimuovizpazi(char* str);


/**
* @function rimuovizpazi
*
* @brief verifica se siste un elemento in un array di utenti
*
* @var elem elemento da ricercare
*
* @var arr array di utenti dove cercare
*
* @return <0 se fallisce
*
* @var dim grandezza array
*/
int exist(char * elem, UserConnected* arr, int dim);

/**
* @function filepush
*
* @brief inserisce una stringa in coda ad un file
*
* @var path il percorso del file
*
* @var topush variabile da inserire
*
* @return <=0 se fallisce
*
*/
int filepush(char * path,char * topush);

/**
* @function stringremove
*
* @brief elimina la stringa passata da un file
*
* @var path il persorso del file
*
* @var toremove stringa da rimuovere
*
* @return <=0 se fallisce
*/
int stringremove(char * path, char * toremove);

/**
* @function nicktoarray
*
* @brief prende un file con piccole stringhe max 100 caratteri e le mette in un array
*
* @var path percorso del file
*
* @var dim dimensione array creato
*
* @return array
*/
char ** nicktoarray (char * path, int * dim);


/**
* @function nickpresente
*
* @brief verifica se stringa di amx 100 caratteri presente in un file
*
* @var path percorso del file
*
* @var dim nick stringa da cercare
*
* @return <=0 se fallisce
*/
int nickpresente(char * path, char * nick);



#endif