# Socket

Il protocollo per il trasferimento del file funziona come segue: per richiedere un file il client invia al
server i tre caratteri ASCII “GET” seguito dal carattere ASCII dello spazio e dai caratteri ASCII del
nome del file, terminati dai caratteri ASCII carriage return (CR) e line feed (LF):

G E T filename CR LF

(Nota: il comando include un totale di 6 caratteri ASCII, cioè 6 bytes, più quelli del nome del file)
Il server risponde inviando:

\+ O K CR LF B1 B2 B3 B4 FileContents T1 T2 T3 T4

Notare che il messaggio è composto da 5 caratteri, seguiti dal numero di byte del file richiesto (un
intero senza segno su 32 bit in network byte order - bytes B1 B2 B3 B4 nella figura), seguito dai
bytes del contenuto richiesto, e poi dal timestamp dell’ultima modifica (Unix time, cioè numero di
secondi dall’inizio dell’“epoca”), rappresentato come un intero senza segno su 32 bit in network
byte order (bytes T1 T2 T3 T4 nella figura).
Per ottenere il timestamp dell’ultima modifica al file, si faccia riferimento a
lle chiamate di sistema stat o fstat.
Il client può richiedere più file usando la stessa connessione TCP inviando più comandi GET, uno
dopo l’altro. Quanto il client ha finito di spedire i comandi sulla connessione, incomincia la
procedura per chiudere la connessione. In condizioni normali, la connessione dovrebbe essere
chiusa in maniera ordinata, cioè, l’ultimo file richiesto dovrebbe essere stato trasferito
completamente prima che la procedura di chiusura termini.
In caso di errore (es. comando illegale, file inesistente) il server risponde sempre con

\- E R R CR LF

(6 caratteri) e quindi procede a chiudere in modo ordinato la connessione con il client.
