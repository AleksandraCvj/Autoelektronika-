# Autoelektronika
Provera stanja vrata u automobilu 

# Ideja projekta 
Osnovna ideja ovog projekta je sistem za proveru stanja vrata u automobilu. 

Praćenjem podataka sa senzora (kanal 0), dobijamo informaciju o tome da li su vrata otvorena ili zatvorena, 
koja vrata su u pitanju, kao i kolika je trenutna brzina kojom se kreće automobil.
Pomoću kanala 1 šalje se maksimalna brzina pri kojoj vrata smeju biti otvorena.
U slučaju da se detektuje da su vrata otvorena dok se vozilo kreće brzinom većom od dozvoljene, 
javlja se upozorenje u vidu treptanja diode i ispisivanja odgovarajućeg broja vrata.
Jedina razlika je u slučaju otvorenog gepeka, gde treptanje diodi zavisi od toga da li je donji taster 
na led baru pritisnut ili ne tj. da li je alarm upaljen ili ne.


## Pokretanje sistema 
Za realizaciju ovog projekta koriste se tri periferije:
  1) AdvUniCom softver za simulaciju serijske komunikacije(kanal 0 i kanal 1)
  2) LED_bar - RGb - prva dva stupca su led diode, dok je poslednji stubac namenjen tasterima
  3) 7seg displej - pet cifara 
  
Na CH0 se šalje niz karaktera npr. '1 1 250+', gde se znakom + detektuje kraj unosa poruke. 
Prvi karakter se odnosi na stanje vrata, drugi na to koja su vrata(1 - prednja leva, 2 - prednja desna, 3 - zadnja leva, 4 - zadnja desna, 5 - gepek), 
a prikazani trocifreni broj trenutnu brzinu automobila.
Preko kanala CH1 se šalje maksimalna dozvoljena brzina automobila u vidu poruke '\00MAXBRZ150\0d', gde poslednje dve cifre označavaju kraj poruke
tj. carriage return. 
Ukoliko su vrata otvorena pri nedozvoljenoj brzini, led diode sijaju, a na 7seg displeju se prikazuje ispis( door1, door2, door3, door4 ili dooor5).
U slučaju gepeka, 7seg displej uvek ispisuje door5, a diode sijaju samo u slučaju da se detektuje pritisak najdonjeg tastera.
 
## Kratak pregled korišćenih task-ova
### SerialReceiveTask_0
Task služi za obradu podataka koji stižu sa kanala 0. Oni se skladište u red serial_queue.
### SerialSendTask_0
Ovim taskom simuliramo vrednosti koje stižu sa senzora svakih 1s, tako što
svakih šaljemo karakter 's'.
### SerialReceiveTask_1
Task služi za prijem podataka sa kanala 1.
### Obrada_podataka
Ovde pristižu red koji sadrži podatke sa kanala 0 i red koji sadrži vrednost maksimalne brzine, 
a zatim se te vrednosti ispisuju na terminalu. Ukoliko su vrata otvorena, a brzina veća od dozvoljene, 
podatak o broju vrata se smešta u red broj_vrata.
### Led_bar
Task koji omogućava treptanje dioda. 
### Displej 
Task koji omogućava ispis na 7seg displej.

