/** \file users.h
       \author Giovanni Bartolomeo 530375 
       Si dichiara che il contenuto di questo file e' in ogni sua parte opera  
       originale dell'autore  
*/  

#ifndef users_h
#define users_h

#include <pthread.h>
#include <config.h>

/*struttura contenete le info di un utente connesso
attualmente c´è solo il nickname ma essendo una struttura in futuro posso
aggiungere informazioni senza problemi*/
typedef struct user {
	char nick[MAX_NAME_LENGTH ];
}UserConnected;




#endif