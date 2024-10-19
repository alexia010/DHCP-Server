# DHCP-Server

## Descriere

Proiectul propus are ca scop configurarea si testarea unui server DHCP (Dynamic Host Configuration Protocol) pe o platforma Linux. Aplicatia va fi implementa folosind o conexiune client-server multithreading. Serverul DHCP este esential intr-o retea de calculatoare pentru a atribui automat adrese IP dispozitivelor conectate, gestionand eficient distributia resurselor IP intr-o retea.

## Functionalitati generale

   1. Realizarea conexiunii client-server multithreading
   2. Stocarea informatiilor esentiale pentru functionarea serverului DHCP intr-o baza de date. 
   3. Alocarea automata a adreselor IP si a altor informatii de configurare retelei precum: subnet mask, gateway si lease time pentru dispozitivele care se conecteaza la o retea.
   4. Clientul dispune de optiunea DNS, care ofera informatii pentru rezolvarea numelui de domeniu in adrese IP.
   5. Suport pentru securitate: filtrarea pe baza adreselor MAC.
   6. Logarea la nivelul serverului DHCP: fisier de log.
   7. Testarea de performanta a raspunsurilor functionarii serverului: timp de raspuns, numar de cereri.

-  Cerintele sunt dezvoltate in fisierul atasat: Functionalitati.docx
