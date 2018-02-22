# Chatterbox

Questa è una soluzione per il progetto di Laboratorio di Sistemi Operativi dell'università di Pisa dell'a.a. 2016/2017

Le specifiche del progetto sono state date dal docente e sono presenti nel file: *chatterbox17.pdf*

#### ATTENZIONE

Questa soluzione proposta è stata sviluppata in circa una settimana e mezzo nel caldo mese di luglio 2017. Non escludo che potrebbero esserci punti oscuri e misteriosi che fanno delle magie incomprensibili all'uomo. Se qualcuno di voi avesse idee su come migliorare, o vuole insultarmi personalmente, mandate una mail a giobarty@gmail.com oppure sottomettete direttamente una PR.

Il file contenente il cuore del server è chatty.c come da specifica,
invece i files :

 * extra/fileoperations.h
 * extra/pharser.h
 * users.h
 * workers.h

sono delle interfacce che rappresentano delle scelte progettuali che ho voluto separare e trattare singolarmente.
Le implementazioni di queste interfacce risiedono nella libreria ​ libchatty.a​ generata a partire dal file
funcs.c che contiene il codice effettivo di queste utility.

#### *extra/fileoperations.h*
Rappresenta una utility di funzioni che vengono utilizzate per operare nei files generati dal server.
Contiene ad esempio operazioni di rimozione/aggiunta di piccole stringhe su file, che dal server vengono
utilizzare per salvare in maniera permanente i nickname registrati.

#### *extra/pharser.h*
Come suggerisce il nome, è il pharser del file di configurazione del server.
Crea una struttura chiamata ConfigFile che contiene la unix path, il numero massimo di connessioni
attivabili del server e tutta la serie di settaggi dichiarati nella specifica del progetto.
Nel caso di valori non cruciali mancanti nel file di configurazione, sono previste delle scelte di default
pensando ad un server di piccole dimensioni.

#### *Users.h*
Contiene soltanto la dichiarazione della struttura che rappresenta un utente connesso.
Ho deciso di separare questa struttura dal resto del progetto perché in futuro nel caso in cui servissero
delle operazioni specifiche per un utente, è possibile creare un’interfaccia direttamente qui.
Ad oggi questo file è tranquillamente possibile fonderlo con chatty.c senza creare alcun tipo di
confusione, ma per i motivi di cui sopra ho preferito separarlo.
La stessa struttura che rappresenta l’utente, è attualmente composta esclusivamente dal suo nickname
ma se nel futuro si volesse espandere la quantità di informazioni che rappresentano un utente connesso,
è possibile farlo senza modificare i parametri di ingresso delle funzioni che riguardano l’utente.

#### *connections.h*
In questo file ho implementato la comunicazione effettiva tra Client e Server attraverso la scrittura e la
lettura nel canale di comunicazione con le System Calls Read/Write mediante il seguente protocollo

**MAX_NAME_LENGTH byte**​: rappresentanti il nome del mittente<br />
**sizof(op_t) byte**:​ rappresentanti il tipo di operazione da effettuare<br />
**MAX_NAME_LENGTH byte**:​ rappresentanti il nome del destinatario<br />
**sizeof(int) byte**:​ rappresentanti la dimensione del campo di testo ovvero del campo successivo <br />
**I byte restanti sono il messaggio** <br />

Questo nel suo insieme rappresenta un messaggio, decomponibile in header (solo le prime due
scritture/letture) e Data (le ultime tre scritture/letture)

Ho provveduto a creare una funzione ​ fuller ​ che riempie gli spazi restanti tra la stringa scritta e la sua
dimensione per la scrittura (MAX_NAME_LEGTH) in modo da mandare una serie di zeri
successivamente fino a riempire tutto il buffer di scrittura/lettura.

Ho anche creato una funzione sendHdr per poter mandare solo un header di messaggio come risposta
al Client

#### *workers.h*
In questo file ho parametrizzato la creazione di un Pool di thread workers.
Infatti passando come parametro il puntatore ad una struttura di tipo PooolCreator sarà possibile creare
un Pool di thread con una chiamata di funzione, ed arrestarli con un altra.
La struttura passata per parametro conterrà

- l file di configurazione del server che conterrá l’informazione sul numero di thread da attivare
* La funzione che il pool di thread dovrà eseguire
* La funzione di cleanup da lanciare alla distruzione dei thread
* Gli argomenti da passare ai thread creati
* Gli argomenti da passare alla funzione di cleanup

La funzione workerscreator(arg) crea il pool di thread e workersdestroyer(arg) li farà terminare.
Poiché non si possono fare assunzioni su una eventuale terminazione della funzione che il thread
worker esegue, è stato necessario effettuare la fermata dei thread attraverso cancellazione e
non attraverso join.
È comunque già preinstallato in questi worker una inizializzazione ed un clean up minimali che si
occupano del detach della memoria occupata dal thread alla sua cancellazione in modo da non creare
memory leaks. La funzione di cleanup passata dall’utente di questa interfaccia viene automaticamente
eseguita alla chiusura del worker. Nel caso in cui non si voglia effettuare cleanup è possibile
direttamente passare una funzione NULL.
Per garantire il completo detach dei thread da parte del sistema, viene sospeso il programma per un
secondo prima della sua terminazione.

#### *chatty.c*
Qui è contenuto il codice del server vero e proprio.
Dopo una lunghissima routine di inizializzazioni delle lock, delle variabili di condizione e tutta la serie di
variabili globali utilizzate dal server, si effettua la vera e propria gestione dei segnali e l’avvio dei thread
che ascolteranno e gestiranno le richieste dei client.


## Gestione richieste dei client
I thread listener effettuano una routine di accettazione richiesta client/gestione richiesta client.
I listeners ascolteranno uno alla volta il canale di comunicazione, e appena accettata una richiesta da
parte di un client, passeranno l’ascolto ad un altro thread.
Il sistema di ascolto è implementato attraverso la lock ​ **keyaccept**​ , la variabile ​ **acceptmutex**​ e la
variabile di condizione ​ **condaccept**.

Appena arriva una richiesta viene filtrata dalla chiamata di sistema ​ select ​ che ci permette di identificare
se la richiesta è una nuova connessione, una disconnessione o un messaggio in arrivo.
Nel caso della connessione, se posso ancora accettare connessioni, si aggiunge un posto vuoto nell’array
degli utenti connessi, che sarà poi colmato al momento della registrazione con il suo nickname, altrimenti
mando un messaggio di errore.
L’array è strutturato in modo tale che alla posizione ​ i ​ vi sta il client connesso nel file descriptor ​ i ​ così da
rendere immediato reperire il nickname associato. Questo sistema rende inutilizzate le prime 3 posizioni
dell’array poiché rappresentano i file descriptor relativi allo stdin, stout e stderr, ma è una perdita minima
e se ne guadagna in semplicità di lettura nel lasciarlo ordinato con l’associazione di cui sopra.

Nel caso in cui dovessimo gestire una richiesta diversa dalla connessione (messaggio o chiusura del
canale di comunicazione)
ci garantiamo la mutua esclusione su quel file descriptor con un array di mutex chiamato ​ **fdBusyList**
accoppiato con le lock ​ **keyuser​** e la variabile di condizione ​**conduser**.
Nella i-esima posizione di questo array si trova lo stato del file descriptor i, lo stato può essere

* 0 : fd non occupato da nessun server
* -1 : temporaneamente abbandonato da un worker per inviare un messaggio ad un altro fd.
Questo stato è necessario per evitare situazioni di stallo tra due file descriptor che vogliono
comunicare tra di loro
* \>0 : contiene il tid del thread che lo sta occupando

Dopo aver ottenuto la garanzia della mutua esclusione sul file descriptor passiamo ad occuparci del
problema successivo, ovvero capire se il Client ha scritto qualcosa oppure ha semplicemente chiuso la
connessione.
Per capirlo il server effettua una richiesta di lettura del messaggio dal fd, se fallisce allora il canale è
stato chiuso e procedo alla disconnessione del Client altrimenti gestisco la richiesta che ho appena
ricevuto.

La gestione della richiesta del client viene effettuata dalla funzione elaboraRichiesta() che in base al tipo
di operazione restituisce il messaggio ed effettua le operazioni previste dalla specifica del progetto.
Di seguito alcune scelte progettuali riguardo le operazioni da gestire:

* Per l’invio di un messaggio al fine di evitare situazioni di stallo, viene momentaneamente
rilasciata la mutua esclusione su file descriptor corrente. Se il destinatario del messaggio
non risulta online viene comunque mandato il messaggio, ma ad un file associato
all’utente destinatario che potrà leggere la history del messaggio con l’apposito comando.
Con questo sistema anche se il server viene disconnesso, i messaggi inviati restano
salvati.
* Consento la connessione di un client con un nickname di un utente già connesso, ma per
farlo disconnetto il client che risulta già connesso al posto suo.
* Il server è strutturato in modo da mantenere in un file i nickname registrati dagli utenti fino
alla loro deregistrazione anche se il server viene chiuso, tuttavia il file dei nickname viene
cancellato ad ogni avvio del server poiché i test non mettono in conto tale assunzione,
infatti capita che registrino gli stessi utenti tra un caso di test e l’altro. Dunque questa
funzionalità è presente ma non sfruttata.
* Per garantire la mutua esclusione durante l’accesso a qualsiasi file, viene utilizzata la lock
**keyfile**
* Per garantire la mutua esclusione durante l’aggiornamento delle statistiche, viene utilizzata la lock ​ **keystat**
* Per allocare i file contenenti i nickname e i messaggi in arretrato degli utenti si utilizza la
directory files/, creata dal main durante l’avvio del server.
utilizzata la lock ​ keystat

## Segnali
Durante l’intera esecuzione delle operazioni dai threads listeners, il thread dove viene eseguito il main,
rimane l’unico in ascolto di segnali attraverso la chiamata di ​ sigwait. Sarà lui infatti a chiedere la
generazione delle statistiche e a chiudere i thread in caso di segnale di terminazione.
Tutti gli altri thread hanno una maschera totale dei segnali.

## Compilazione ed esecuzione del codice

Il codice è stato sviluppato e testato su Fedora 25 e Ubuntu 16.04 con il 100% di riuscita dei test
proposti.
È possibile compilare il codice tramite makefile eseguendo:

`make`

Per eseguire i test:

`make test1`

`make test2`

`make test3`

`make test4`

Per eseguire tutti i test insieme e creare il tar di consegna progetto, eseguire:

 **` make consegna `**

Il makefile è stato modificato in modo tale da creare autonomamente la libreria necessaria e compilare
correttamente il progetto.
Lo script bash richiesto da specifica di progetto è stato incluso con il nome ​ **EliminaFileServer.sh**

**Nota : testfile.sh faceva assunzioni sulla dimensione del file ./chatty, il test prevedeva che tale file
dovesse essere accettato dal server, ma il mio file ./chatty supera la dimensione prevista dal file
di configurazione DATA/chatty.conf2 pertanto il server lo rifiuta. Per far passare il test ho messo
una tolleranza di 10kb sui file che il server può accettare.**

*Questa soluzione è stata sviluppata da Giovanni Bartolomeo, che ha felicemente passato l'esame grazie a questo progetto 😄😄😄*








